/**
 * Copyright (C) 2021 Parrot Drones SAS
 */

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define ULOG_TAG ms_sg_processing
#include <ulog.h>
ULOG_DECLARE_TAG(ULOG_TAG);

#include <libpomp.h>
#include <torch/script.h>
#include <video-ipc/vipc_client.h>

#include <opencv2/opencv.hpp>

#include "evalutation.hpp"
#include "processing.h"
#include "utils/convert.hpp"
#include "utils/performance.hpp"

#define NN_MODEL_ADDR \
  "/mnt/user-internal/missions/com.parrot.missions.samples.hello/payload/services/torch/model/resnet18.pt"

#include <turbojpeg.h>

#include <fstream>

struct processing {
  struct pomp_evt *evt;
  bool started;
  bool stop_requested;
  pthread_t thread;
  pthread_mutex_t mutex;
  pthread_cond_t cond;

  struct processing_input input;
  bool input_available;

  struct processing_output output;
  bool output_available;
};

static void do_step(struct processing *self,
                    const struct processing_input *input,
                    struct processing_output *output) {
  ULOGW("---------------DO-STEP---------------");
  torch::jit::script::Module module;
  std::vector<torch::jit::IValue> inputs;
  at::Tensor img_tensor = torch::rand({1, 3, 224, 224});
  inputs.push_back(img_tensor);
  torch::NoGradGuard no_grad;
  at::Tensor out;
  try {
    module = torch::jit::load(NN_MODEL_ADDR);
    module.eval();
    out = module.forward(inputs).toTensor();
  } catch (const c10::Error &e) {
    ULOGE(e.what());
    ULOGC("RUNNING DON4T WORK");
    return;
  }
  ULOGW("-----------------TEST----------------");

  torch::Tensor rgb_tensor = Nv21ToTensor_rgb(input->frame);
  // cv::Mat rgb_tensor_image = cvToTensor(rgb_tensor);
  // bool res = false;
  // res = cv::imwrite("/mnt/user/DCIM/rgb_tensor.jpg",
  // 		  rgb_tensor_image);
  // if (!res) {
  // 	ULOGN("can't write rgb_tensor");
  // } else
  // 	ULOGN("rgb_tensor : Done");

  // --------------LIBJPEG-TURBOMODULE_LOG--------------
  ULOGI("Libjpeg");

  tjhandle _jpegCompressor = tjInitCompress();;
  const int JPEG_QUALITY = 100;
  const int COLOR_COMPONENTS = 3;
  int _width = input->frame->width;
  int _height = input->frame->height;
  long unsigned int _jpegSize = 0;
  unsigned char *_compressedImage = NULL;                     //!< Memory is allocated by tjCompress2 if _jpegSize == 0
  unsigned char buffer[_width * _height * COLOR_COMPONENTS];  //!< Contains the uncompressed image
  ULOGI("Savin image %dx%d", _width, _height);
  at::Tensor test_tensor = torch::rand({1, 3, _width, _height});

  float *p_rgb_tensor = (float *)test_tensor.data_ptr();

  for (int i = 0; i < _width * _height * COLOR_COMPONENTS; ++i) {
    buffer[i] = p_rgb_tensor[i]*255;
  }
  ULOGI("transform");

  int res = tjCompress2(_jpegCompressor, buffer, _width, 0, _height, TJPF_RGB,
              &_compressedImage, &_jpegSize, TJSAMP_444, JPEG_QUALITY,
              TJFLAG_FASTDCT);
  if (res!=0){
    ULOGC("ERROR TJCOMPRESS2");
  }
  std::chrono::seconds sleeping( 2);
  std::this_thread::sleep_for( sleeping );
  ULOGI("compresser");
  const char *missionDataRoot = getenv("MISSION_DATA");
  ULOGN(missionDataRoot);
  const char* filename = "/mnt/user-internal/missions-data/com.parrot.missions.samples.hello/image_test.jpg";
  // FILE * file = fopen(filename, "wb");
  std::ofstream file(filename, std::ios_base::out | std::ios_base::binary);
  if(file.fail()) {
    ULOGE("can't open file %s failed!\n", filename);
  }
  
  file.write((char *)_compressedImage, _jpegSize);
  ULOGN("Saving : %s, %lu byte", filename, _jpegSize);
  std::this_thread::sleep_for( sleeping );

  // fwrite(_compressedImage, 1, _jpegSize, file);
  ULOGI("file save !!");
  // fclose(file);

  tjDestroy(_jpegCompressor);
  tjFree(_compressedImage);
  ULOGI("fin !!");

  // -----------------------------------------
  output->x = input->position_global.x;
  output->y = input->position_global.y;
  output->z = input->position_global.z;

  /* Save timestamp of the frame */
  output->ts.tv_sec = input->frame->ts_sof_ns / 1000000000UL;
  output->ts.tv_nsec = input->frame->ts_sof_ns % 1000000000UL;
}

static void *thread_entry(void *userdata) {
  int res = 0;
  struct processing *self = (struct processing *)userdata;
  struct processing_input local_input;
  struct processing_output local_output;

  pthread_mutex_lock(&self->mutex);

  while (!self->stop_requested) {
    /* Atomically unlock the mutex, wait for condition and then
      re-lock the mutex when condition is signaled */
    res = pthread_cond_wait(&self->cond, &self->mutex);
    if (res != 0) {
      ULOG_ERRNO("pthread_cond_wait", res);
      continue;
    }

    /* Check booleans in case of spurious wakeup */
    if (self->stop_requested)
      break;
    if (!self->input_available)
      continue;

    /* Copy locally input data */
    local_input = self->input;
    memset(&self->input, 0, sizeof(self->input));
    self->input_available = false;

    /* Do the heavy computation outside lock */
    pthread_mutex_unlock(&self->mutex);
    memset(&local_output, 0, sizeof(local_output));
    do_step(self, &local_input, &local_output);
    pthread_mutex_lock(&self->mutex);

    /* Done with the input frame */
    vipcc_release_safe(local_input.frame);
    memset(&local_input, 0, sizeof(local_input));

    /* Copy output data */
    self->output = local_output;
    self->output_available = true;
    /* Notify main loop that result is available */
    res = pomp_evt_signal(self->evt);
    if (res < 0)
      ULOG_ERRNO("pomp_evt_signal", -res);
  }

  pthread_mutex_unlock(&self->mutex);

  return NULL;
}

int processing_new(struct pomp_evt *evt, struct processing **ret_obj) {
  struct processing *self = NULL;
  ULOG_ERRNO_RETURN_ERR_IF(ret_obj == NULL, EINVAL);
  *ret_obj = NULL;
  ULOG_ERRNO_RETURN_ERR_IF(evt == NULL, EINVAL);

  self = (struct processing *)calloc(1, sizeof(*self));
  if (self == NULL)
    return -ENOMEM;

  self->evt = evt;
  pthread_mutex_init(&self->mutex, NULL);
  pthread_cond_init(&self->cond, NULL);

  // int res; // TODO pas tres propre
  // res = self->nn_model.load(NN_MODEL_ADDR);
  // if (res<0) {
  // 	ULOG_ERRNO("model_load", -res);
  // 	return -1; // TODO VOIR QUELLE EST LA MEILLEUR VALEUR A METTRE
  // ERRNO
  // }

  *ret_obj = self;
  return 0;
}

void processing_destroy(struct processing *self) {
  if (self == NULL)
    return;

  processing_stop(self);

  pthread_mutex_destroy(&self->mutex);
  pthread_cond_destroy(&self->cond);

  free(self);
}

int processing_start(struct processing *self) {
  int res = 0;
  ULOG_ERRNO_RETURN_ERR_IF(self == NULL, EINVAL);
  ULOG_ERRNO_RETURN_ERR_IF(self->started, EBUSY);

  /* Create background thread */
  self->stop_requested = false;
  res = pthread_create(&self->thread, NULL, &thread_entry, self);
  if (res != 0) {
    ULOG_ERRNO("pthread_create", res);
    return -res;
  }
  self->started = true;

  return 0;
}

void processing_stop(struct processing *self) {
  if (self == NULL || !self->started)
    return;

  /* Ask thread to stop */
  pthread_mutex_lock(&self->mutex);
  self->stop_requested = true;
  pthread_cond_signal(&self->cond);
  pthread_mutex_unlock(&self->mutex);

  /* Wait for thread and release resources */
  pthread_join(self->thread, NULL);
  self->started = false;

  /* Cleanup remaining input data if any */
  pthread_mutex_lock(&self->mutex);
  if (self->input_available) {
    vipcc_release_safe(self->input.frame);
    memset(&self->input, 0, sizeof(self->input));
    self->input_available = false;
  }
  pthread_mutex_unlock(&self->mutex);
}

int processing_step(struct processing *self,
                    const struct processing_input *input) {
  int res = 0;
  ULOG_ERRNO_RETURN_ERR_IF(self == NULL, EINVAL);
  ULOG_ERRNO_RETURN_ERR_IF(!self->started, EPERM);

  pthread_mutex_lock(&self->mutex);

  /* If an input is already pending, release it before overwrite */
  if (self->input_available) {
    vipcc_release_safe(self->input.frame);
    memset(&self->input, 0, sizeof(self->input));
    self->input_available = false;
  }

  /* Copy input data and take ownership of frame */
  self->input = *input;
  self->input_available = true;

  /* Wakeup background thread */
  res = pthread_cond_signal(&self->cond);
  if (res != 0) {
    /* pthread return a positive errno value */
    ULOG_ERRNO("pthread_cond_signal", res);
    res = -res;

    /* Do not take frame and give it back to caller */
    memset(&self->input, 0, sizeof(self->input));
    self->input_available = false;
  }

  pthread_mutex_unlock(&self->mutex);

  return 0;
}

int processing_get_output(struct processing *self,
                          struct processing_output *output) {
  int res = 0;
  ULOG_ERRNO_RETURN_ERR_IF(self == NULL, EINVAL);
  ULOG_ERRNO_RETURN_ERR_IF(!self->started, EPERM);

  pthread_mutex_lock(&self->mutex);

  if (!self->output_available) {
    res = -ENOENT;
  } else {
    *output = self->output;
    self->output_available = false;
  }

  pthread_mutex_unlock(&self->mutex);

  return res;
}

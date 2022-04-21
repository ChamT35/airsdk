#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#define ULOG_TAG ms_nn_processing
#include <ulog.h>
ULOG_DECLARE_TAG(ULOG_TAG);

#include <libpomp.h>

#include <opencv2/opencv.hpp>

#include "nn_processing.h"

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
		    struct processing_output *output)
{
	output->x = input->x + 1
}

static void *thread_entry(void *userdata)
{

}

int processing_new(struct pomp_evt *evt, struct processing **ret_obj)
{
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

	*ret_obj = self;
	return 0;
}

void processing_destroy(struct processing *self)
{
	if (self == NULL)
		return;

	processing_stop(self);

	pthread_mutex_destroy(&self->mutex);
	pthread_cond_destroy(&self->cond);

	free(self);
}

int processing_start(struct processing *self)
{
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

void processing_stop(struct processing *self)
{
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

int processing_step(struct processing *self, const struct processing_output *output)
{
	int res = 0;
	ULOG_ERRNO_RETURN_ERR_IF(self == NULL, EINVAL);
	ULOG_ERRNO_RETURN_ERR_IF(!self->started, EPERM);
	
	pthread_mutex_lock(&self->mutex);

	self->input = output;
	self->input_available = true;

	res = pthread_cond_signal(&self->cond);
	if (res != 0) {
		ULOG_ERRNO("pthread_cond_signal", res);
		res = -res;

		memset(&self->input, 0, sizeof(self->input));
		self->input_available = false;
	}

	pthread_mutex_unlock(&self->mutex);

	return 0;

}

int processing_get_output(struct processing *self,
			  struct processing_output *output)
{
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

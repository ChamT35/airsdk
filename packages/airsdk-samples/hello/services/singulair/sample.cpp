/**
 * Copyright (C) 2020 Parrot Drones SAS
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <math.h>
#include <unistd.h>

#define ULOG_TAG ms_sg_sample
#include <ulog.h>
ULOG_DECLARE_TAG(ULOG_TAG);

#include <libpomp.hpp>
#include <libtelemetry.h>
#include <video-ipc/vipc_client.h>
#include <video-ipc/vipc_client_cfg.h>

#include <google/protobuf/empty.pb.h>
#include <samples/singulair/cv-service/messages.msghub.h>

#include <opencv2/opencv.hpp>
// #include <opencv2/imgcodecs.hpp>

#include "processing.h"
#include "evalutation.hpp"
// #include <d_utils/cv_images.hpp>

#define VIPC_DEPTH_MAP_STREAM "fcam_streaming"
#define MSGHUB_ADDR "unix:/tmp/singulair-cv-service"


#define IMAGE_ADRR \
	"/mnt/user-internal/missions/com.parrot.missions.samples.hello/payload/services/15.jpg"


class HelloServiceCommandHandler : public ::samples::singulair::cv_service::
					   messages::msghub::CommandHandler {
public:
	inline HelloServiceCommandHandler(struct context *ctx) : mCtx(ctx) {}

	virtual void processingStart(
		const ::google::protobuf::Empty &args) override;

	virtual void processingStop(
		const ::google::protobuf::Empty &args) override;

private:
	struct context *mCtx;
};

struct context {
	using HelloServiceEventSender =
		::samples::singulair::cv_service::messages::msghub::EventSender;

	pomp::Loop loop;

	struct vipcc_ctx *vipcc;
	struct vipc_dim frame_dim;

	struct pomp_evt *processing_evt;
	struct processing *processing;

	msghub::MessageHub *msg;
	msghub::Channel *msg_channel;
	HelloServiceCommandHandler msg_cmd_handler;
	HelloServiceEventSender msg_evt_sender;

	bool is_close;

	inline context() : msg_cmd_handler(this), is_close(false) {}
};

/* Global context (so the signal handler can access it */
static struct context s_ctx;
/* Stop flag, set to 1 by signal handler to exit cleanly */
static volatile int stop;

static void context_clean(struct context *ctx)
{
	int res = 0;

	if (ctx->processing != NULL) {
		processing_destroy(ctx->processing);
		ctx->processing = NULL;
	}

	if (ctx->processing_evt != NULL) {
		res = pomp_evt_detach_from_loop(ctx->processing_evt, ctx->loop);
		if (res < 0)
			ULOG_ERRNO("pomp_evt_detach_from_loop", -res);
		res = pomp_evt_destroy(ctx->processing_evt);
		if (res < 0)
			ULOG_ERRNO("pomp_evt_destroy", -res);
		ctx->processing_evt = NULL;
	}

	delete ctx->msg;
	ctx->msg = NULL;

}

static int context_start(struct context *ctx)
{
	/* msg */
	ctx->msg_channel =
		ctx->msg->startServerChannel(pomp::Address(MSGHUB_ADDR), 0666);
	if (ctx->msg_channel == nullptr) {
		ULOGE("Failed to start server channel on '%s'", MSGHUB_ADDR);
		return -EPERM;
	}

	ctx->msg->attachMessageHandler(&ctx->msg_cmd_handler);
	ctx->msg->attachMessageSender(&ctx->msg_evt_sender, ctx->msg_channel);
	
	return 0;
}

static int context_stop(struct context *ctx)
{
	int res = 0;

	/* Processing */
	processing_stop(ctx->processing);
	/* Stop vipc and processing */
	ctx->msg_cmd_handler.processingStop(::google::protobuf::Empty());

	/* msg */
	ctx->msg->detachMessageSender(&ctx->msg_evt_sender);
	ctx->msg->detachMessageHandler(&ctx->msg_cmd_handler);
	ctx->msg->stop();
	ctx->msg_channel = nullptr;

	return res;
}

static void processing_evt_cb(struct pomp_evt *evt, void *userdata)
{
	int res = 0;
	struct context *ud = (struct context *)userdata;
	struct processing_output output;

	/* Get result from processing object */
	res = processing_get_output(ud->processing, &output);
	if (res < 0) {
		ULOG_ERRNO("processing_get_output", -res);
		return;
	}
}

static void status_cb(struct vipcc_ctx *ctx,
		      const struct vipc_status *st,
		      void *userdata)
{
	int res = 0;
	struct context *ud = (struct context *)userdata;

	for (unsigned int i = 0; i < st->num_planes; i++) {
		ULOGI("method %s, plane %d, w %d, h %d, stride %d",
		      st->method,
		      i,
		      st->width,
		      st->height,
		      st->planes[i].stride);
	}

	ud->frame_dim.width = st->width;
	ud->frame_dim.height = st->height;

	res = vipcc_start(ctx);
	if (res < 0)
		ULOG_ERRNO("vipcc_start", -res);
}

static void frame_cb(struct vipcc_ctx *ctx,
		     const struct vipc_frame *frame,
		     void *be_frame,
		     void *userdata)
{
		int res = 0;
	struct context *ud = (struct context *)userdata;
	struct processing_input input;

	ULOGD("received frame %08x", frame->index);

	/* Sanity checks */
	if (frame->width != ud->frame_dim.width) {
		ULOGE("frame width (%d) different than status width (%d)",
		      frame->width,
		      ud->frame_dim.width);
		goto out;
	}
	if (frame->height != ud->frame_dim.height) {
		ULOGE("frame height (%d) different than status height (%d)",
		      frame->height,
		      ud->frame_dim.height);
		goto out;
	}
	if (frame->num_planes != 2) { //TODO J'ai changer le nombre de plan a 2
		ULOGE("wrong number of planes (%d)", frame->num_planes);
		goto out;
	}
	if (frame->format != VACQ_PIX_FORMAT_NV21) { // TODO METTRE LE BON FORMAT
		ULOGE("wrong format");
		goto out;
	}

	/* Setup input structure for processing */
	memset(&input, 0, sizeof(input));
	input.frame = frame;

	res = processing_step(ud->processing, &input);
	if (res < 0) {
		ULOG_ERRNO("processing_step", -res);
		goto out;
	}

	/* The frame has been given to the processing object */
	frame = NULL;

	/* In case of error, need to release the input frame */
out:
	if (frame != NULL)
		vipcc_release(ctx, frame);
}

static void conn_status_cb(struct vipcc_ctx *ctx,
			   bool connected,
			   void *userdata)
{
	ULOGI("connected: %d", connected);
}

static void eos_cb(struct vipcc_ctx *ctx,
		   enum vipc_eos_reason reason,
		   void *userdata)
{
	ULOGI("eos received: %s (%u)", vipc_eos_reason_to_str(reason), reason);
}

static const struct vipcc_cb s_vipc_client_cbs = {	.status_cb = status_cb,
													.configure_cb = NULL,
													.frame_cb = frame_cb,
													.connection_status_cb =
														conn_status_cb,
													.eos_cb = eos_cb};

static int context_init(struct context *ctx)
{
	int res = 0;

	/* Create message hub */
	ctx->msg = new msghub::MessageHub(&s_ctx.loop, nullptr);
	if (ctx->msg == nullptr) {
		res = -ENOMEM;
		ULOG_ERRNO("msg_new", -res);
		goto error;
	}

	/* Create processing notification event */
	ctx->processing_evt = pomp_evt_new();
	if (ctx->processing_evt == NULL) {
		res = -ENOMEM;
		ULOG_ERRNO("pomp_evt_new", -res);
		goto error;
	}
	res = pomp_evt_attach_to_loop(
		ctx->processing_evt, ctx->loop.get(), &processing_evt_cb, ctx);
	if (res < 0) {
		ULOG_ERRNO("pomp_evt_attach_to_loop", -res);
		goto error;
	}

	/* Create processing object */
	res = processing_new(ctx->processing_evt, &ctx->processing);
	if (res < 0) {
		ULOG_ERRNO("processing_new", -res);
		goto error;
	}

	return 0;

error:
	context_clean(ctx);
	return res;
}

void HelloServiceCommandHandler::processingStart(
	const ::google::protobuf::Empty &args)
{
	ULOGW("Message recu");
	int res = 0;
	struct vipcc_cfg_info vipc_info;
	memset(&vipc_info, 0, sizeof(vipc_info));
	
	/* Make sure not already in progress */
	ULOG_ERRNO_RETURN_IF(mCtx->vipcc != NULL, EBUSY);

	/* Get vipc cfg info */
	res = vipcc_cfg_get_info(VIPC_DEPTH_MAP_STREAM, &vipc_info);
	if (res < 0) {
		ULOG_ERRNO("vipcc_cfg_get_info('%s')",
			   -res,
			   VIPC_DEPTH_MAP_STREAM);
		goto error;
	} else {
		/* Create vipc client */
		mCtx->vipcc = vipcc_new(mCtx->loop.get(),
					&s_vipc_client_cbs,
					vipc_info.be_cbs,
					vipc_info.address,
					mCtx,
					5,
					true);
		if (mCtx->vipcc == NULL) {
			ULOG_ERRNO("vipcc_new", -res);
			goto error;
		}
	}
	vipcc_cfg_release_info(&vipc_info);

	/* Background thread processing */
	res = processing_start(mCtx->processing);
	if (res < 0) {
		ULOG_ERRNO("processing_start", -res);
		goto error;
	}

error:
	/* TODO */
	;
}

void HelloServiceCommandHandler::processingStop(
	const ::google::protobuf::Empty &args)
{
	/* Background thread processing */
	processing_stop(mCtx->processing);

	if (mCtx->vipcc != NULL) {
		vipcc_stop(mCtx->vipcc);
		vipcc_destroy(mCtx->vipcc);
		mCtx->vipcc = NULL;
	}
}

static void sighandler(int signum)
{
	/* Set stopped flag and wakeup loop */
	ULOGI("signal %d (%s) received", signum, strsignal(signum));
	stop = 1;
	pomp_loop_wakeup(s_ctx.loop.get());
}

int main(int argc, char *argv[])
{
	int res = 0;
	ULOGW("HHHHHHHHHHHEEEEEEEEEEEEELLLLLLLLLLLLLLLLLOOOOOOOOOOOOO");

	 /* Initialize context */
	res = context_init(&s_ctx);
	if (res < 0)
		goto out;

	 /* Setup signal handler */
	signal(SIGINT, &sighandler);
	signal(SIGTERM, &sighandler);
	signal(SIGPIPE, SIG_IGN);

	context_start(&s_ctx);

	/* Run loop until stop is requested */
	while (!stop){
		pomp_loop_wait_and_process(s_ctx.loop.get(), -1);
	}
	/* Stop and cleanup */
	context_stop(&s_ctx);

	signal(SIGINT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);
	context_clean(&s_ctx);
out:
	return res == 0 ? 0 : -1;
}

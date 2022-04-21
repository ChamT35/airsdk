#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <math.h>
#include <unistd.h>


// #include <stdio.h>
// #include <stdlib.h>
// #include <signal.h>

#define ULOG_TAG ms_test
#include <ulog.h>
ULOG_DECLARE_TAG(ULOG_TAG);

#include <libpomp.hpp>

#include <google/protobuf/empty.pb.h>
#include <samples/hello/cv-service/messages.msghub.h>

#include "nn_processing.h"

namespace test {

#define MSGHUB_ADDR "unix:/tmp/hello-cv-service"

class HelloServiceCommandHandler : public ::samples::hello::cv_service::
					   messages::msghub::CommandHandler {
public:
	inline HelloServiceCommandHandler(struct context *ctx) : mCtx(ctx) {}

	virtual void nnProcessingStart(
		const ::google::protobuf::Empty &args) override;

	virtual void nnProcessingStop(
		const ::google::protobuf::Empty &args) override;

private:
	struct context *mCtx;
};

struct context {
	using HelloServiceEventSender =
		::samples::hello::cv_service::messages::msghub::EventSender;

	pomp::Loop loop;
	struct pomp_evt *processing_evt;
    struct processing *processing;

	/* Message hub */
	msghub::MessageHub *msg;

	/* Message hub channel */
	msghub::Channel *msg_channel;

	/* Message hub command handler */
	HelloServiceCommandHandler msg_cmd_handler;

	/* Message hub event sender */
	HelloServiceEventSender msg_evt_sender;

    int count = 0;
	
	inline context() : msg_cmd_handler(this){}
};

/* Global context (so the signal handler can access it */
static struct context s_ctx;
/* Stop flag, set to 1 by signal handler to exit cleanly */
static volatile int stop;

static void context_clean(struct context *ctx)
{
    int res = 0;

    ULOGI("CLEAN");
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
    ULOGI("START");

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
    ULOGI("STOP");

	/* Processing */
	processing_stop(ctx->processing);
	/* Stop vipc and processing */
	ctx->msg_cmd_handler.nnProcessingStop(::google::protobuf::Empty());

	/* msg */
	ctx->msg->detachMessageSender(&ctx->msg_evt_sender);
	ctx->msg->detachMessageHandler(&ctx->msg_cmd_handler);
	ctx->msg->stop();
	ctx->msg_channel = nullptr;

    return res;
}

static void processing_evt_cb(struct pomp_evt *evt, void *userdata)
{
    ULOGI("EVENT");
	struct context *ud = (struct context *)userdata;
    ud->count += 1;
    ULOGI("TESTTESTTESTTESTTEST : %d", ud->count);

	if (ud->count % 10  == 0){
		const ::google::protobuf::Empty message;
		ud->msg_evt_sender.nnDone(message);
	}
}

static int context_init(struct context *ctx)
{
    int res = 0;
    ULOGI("INIT");
    
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


   	return res;

error:
	context_clean(ctx);
	return res;
}

static void sighandler(int signum)
{
	/* Set stopped flag and wakeup loop */
	ULOGI("signal %d (%s) received", signum, strsignal(signum));
	stop = 1;
	pomp_loop_wakeup(s_ctx.loop.get());
}

void HelloServiceCommandHandler::nnProcessingStart(
	const ::google::protobuf::Empty &args)
{
	ULOGI("VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV");
	int res = 0;

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

void HelloServiceCommandHandler::nnProcessingStop(
	const ::google::protobuf::Empty &args)
{

	/* Background thread processing */
	processing_stop(mCtx->processing);

}

int test()
{
	int res = 0;
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
        ULOGI("111111111111111111111111111111111111111111111111111111111111111111111111111111111");

		pomp_loop_wait_and_process(s_ctx.loop.get(), -1);
    }
	/* Stop and cleanup */
	context_stop(&s_ctx);

	signal(SIGINT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGPIPE, SIG_DFL);
	context_clean(&s_ctx);
out:
    ULOGI("EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE");

	return res == 0 ? 0 : -1;
}
}
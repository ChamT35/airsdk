#include <chrono>
// #include <filesystem>
#include <string>

#define ULOG_TAG ms_sg_eval
#include <ulog.h>
ULOG_DECLARE_TAG(ULOG_TAG);

#include "utils/performance.hpp"

#include <libpomp.h>
#include <opencv2/opencv.hpp>
#include <torch/script.h>
#include <sgia.hpp>

#define NN_MODEL_ADDR \
	"/mnt/user-internal/missions/com.parrot.missions.samples.hello/payload/services/torch/model/resnet18.pt"
#define TORCH_MODEL_ADDR \
	"/mnt/user-internal/missions/com.parrot.missions.samples.hello/payload/services/torch/model/"


void model_performance(torch::jit::script::Module module)
{
	ULOGN("CA FINIS ????");
	for (int i = 0; i < 2; i++) {
		cv::Mat img(224, 224, CV_8UC3);
		ULOGN("IMAGE CREER");
		sgia_run(module, img);
	}
}

int evaluate()
{	
	ULOGN("Start evaluate()");
	// namespace fs = std::filesystem;

	// std::string path =
	// 	"/mnt/user-internal/missions/com.parrot.missions.samples.hello/payload/services/torch/model/";
	// for (const auto &entry : fs::directory_iterator(path)) {
	// 	std::uintmax_t size =
	// 		std::filesystem::file_size(entry.path()) / 1000000;
	// 	std::string outfilename_str = entry.path().string();
	// 	const char *outfilename_ptr = outfilename_str.c_str();
	// 	ULOGI(entry.path().filename().string().c_str());
	// 	torch::jit::script::Module model =
			
	// 	ULOGI("size : %ju MB", size);

	// 	{
	// 		Timer timer;
	// 		model_performance(model);
	// 	}
	// }
    torch::jit::script::Module model = torch::jit::load(NN_MODEL_ADDR);
    {
        Timer timer;
        model_performance(model);
		ULOGN("CA FINIS ????");
    }
	return 0;
}

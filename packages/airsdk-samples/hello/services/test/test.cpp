#include <torch/script.h> // One-stop header.
#include <iostream>
#include <memory>
#include <chrono>

#include <timer.hpp>

#define NN_MODEL_ADDR \
	"/mnt/user-internal/missions/com.parrot.missions.samples.hello/payload/services/torch/model/resnet18.pt"

int main(int argc, const char *argv[])
{
	torch::jit::script::Module module;
	{
		Timer t;
		try {
			// Deserialize the ScriptModule from a file using
			// torch::jit::load().
			module = torch::jit::load(NN_MODEL_ADDR);
		} catch (const c10::Error &e) {
			std::cerr << e.what();
			std::cerr << "error loading the model\n";
			return -1;
		}
	}
	std::vector<torch::jit::IValue> inputs;
	at::Tensor img_tensor = torch::rand({1, 3, 224, 224});
	inputs.push_back(img_tensor);
	torch::NoGradGuard no_grad;
	at::Tensor output;

	for (int i = 0; i < 10; ++i) {
		Timer t;
		try {
			module.eval();
			output = module.forward(inputs).toTensor();
		} catch (const c10::Error &e) {
			std::cerr << e.what();
			std::cerr << "error running the model\n";
			return -1;
		}
	}

	std::cout << "ok\n";
	return 0;
}

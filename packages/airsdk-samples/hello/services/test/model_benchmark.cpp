#include <dirent.h>
#include <torch/script.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>

#define MODULE_RELATIVE_PATH "torch/model/"
#define MODULE_LOG "/mnt/user-internal/torch/modele_benchmark.csv"

float benchmark(torch::jit::script::Module module) {
  std::chrono::time_point<std::chrono::system_clock> start, end;
  std::chrono::duration<float, std::micro> duration;
  std::vector<torch::jit::IValue> inputs;

  at::Tensor img_tensor = torch::rand({1, 3, 224, 224});
  inputs.push_back(img_tensor);
  start = std::chrono::system_clock::now();
  //Tested function
  module.forward(inputs);

  end = std::chrono::system_clock::now();
  duration = end - start;
  return duration.count() * 0.001f * 0.001f;
}

// int model_size_estimation()

int main(int argc, const char *argv[]) {
  float duration;
  int iter = 20;
  char main_path[256];
  if (argc != 2) {
    strcpy(main_path, MODULE_RELATIVE_PATH);
  } else {
    strcpy(main_path, argv[1]);
  }

  DIR *dirp = opendir(main_path);
  dirent *dp;
  std::ofstream modele_log;
  modele_log.open(MODULE_LOG, std::ofstream::app);
  std::cout << "--BENCHMARK-NEURAL-NETWORK-MODEL--\nNb d'iteration par modele:" << iter
            << "\nModels Path : " << main_path
            << "\nResult file : " << MODULE_LOG
            << std::endl;
  while ((dp = readdir(dirp)) != NULL) {
    if ((strcmp(dp->d_name, ".") && strcmp(dp->d_name, "..") != 0)) {
      std::cout << "----------------------------------\nNom du Modele : ";
      char module_path[256];
      strcpy(module_path, main_path);
      strcat(module_path, dp->d_name);
      torch::jit::script::Module module = torch::jit::load(module_path);
      std::cout << dp->d_name << "\n";
      modele_log << dp->d_name;
      c10::InferenceMode guard;
      module.eval();
      std::vector<float> duration_list;

      float first_iteration = benchmark(module);
      std::cout << "First iteration time : " << first_iteration << std::endl;

      if ((iter > 9)) {
        std::cout << "Traitement : ";
      }
      // Test Loop
      for (int i = 0; i < iter; ++i) {
        duration = benchmark(module);
        duration_list.push_back(duration);
        modele_log << "," << duration;
        if ((iter > 9) && (i % (iter / 20) == 0)) {
          std::cout << "=" << std::flush;
        }
      }
      modele_log << "\n";

      float duration_total = accumulate(duration_list.begin(), duration_list.end(), 0);
      std::cout << "\nTemps total : " << duration_total
                << " s\nTemps Moyen : " << duration_total / duration_list.size() << " s (" << duration_total / duration_list.size() * 100
                << " ms)\nTemps Min : " << *std::min_element(duration_list.begin(), duration_list.end())
                << " s, Temps Max : " << *std::max_element(duration_list.begin(), duration_list.end())
                << " s" << std::endl;
    }
  }
  (void)closedir(dirp);
  return 0;
}
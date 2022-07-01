#include <torch/script.h>

#include <opencv2/opencv.hpp>
#include <vector>

#include "timer.hpp"



main() {
  std::chrono::time_point<std::chrono::system_clock> start, end;
  std::chrono::duration<float, std::micro> duration;
  std::vector<float> duration_list;

  for (int i = 0; i < iter; ++i) {
    at::Tensor tensor = torch::rand({1, 3, 224, 224});
    start = std::chrono::system_clock::now();
    
    

    end = std::chrono::system_clock::now();
    duration = end - start;
    duration_list.push_back(duration.count() * 0.001f);
  }
  std::cout << "----------------------------------\n"
            << "Fonction: \n";
  float duration_total = accumulate(duration_list.begin(), duration_list.end(), 0);
  std::cout << "Temps total : " << duration_total * 0.001f
            << "s\nTemps Moyen : " << duration_total / duration_list.size()
            << "ms\nTemps Min : " << *std::min_element(duration_list.begin(), duration_list.end())
            << "ms, Temps Max :" << *std::max_element(duration_list.begin(), duration_list.end())
            << "ms" << std::endl;

  return 0
}
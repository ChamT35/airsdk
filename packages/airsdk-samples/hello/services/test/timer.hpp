#include <chrono>

struct Timer {
	std::chrono::time_point<std::chrono::system_clock> start, end;
	std::chrono::duration<float> duration;

	Timer()
	{
		start = std::chrono::system_clock::now();
	}
	~Timer()
	{
		end = std::chrono::system_clock::now();
		duration = end - start;
		float duration_ms = duration.count() * 1000.0f;

		std::cout << "System Clock = " << duration_ms << "ms ("
			  << duration.count() << " s)\n";
	}
};
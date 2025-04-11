#include <vector>

#include <chrono>

#include "error.hpp"

namespace pgm {
	void
	insert_cpu_info(const std::chrono::duration<float> &refresh_interval, std::stringstream &output, pgm::error &error);
}


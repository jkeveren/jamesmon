#pragma once

#include <sstream>

#include "error.hpp"

namespace pgm {
	void insert_time_info(std::stringstream &output, pgm::error &error);
}


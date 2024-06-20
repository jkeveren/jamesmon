#include "error.hpp"

#include <chrono>

namespace pgm {	
	class arguments {
		public:
		bool printed_usage = false;
		std::chrono::duration<float> refresh_interval = std::chrono::duration<float>(1) / 60;

		arguments(int argc, char **argv, error &error);
	};
}
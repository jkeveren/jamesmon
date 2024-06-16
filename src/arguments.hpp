#include "error.hpp"

namespace pgm {	
	class arguments {
		public:
		bool printed_usage = false;
		int refresh_interval = 0; // ms

		arguments(int argc, char **argv, error &error);
	};
}
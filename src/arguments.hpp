#include "error.hpp"

namespace pgm {	
	class arguments {
		public:
		bool printed_usage = false;
		int refresh_interval = 16; // ms // defaults to 16 == 1000/60 for 60hz display which is most common.

		arguments(int argc, char **argv, error &error);
	};
}
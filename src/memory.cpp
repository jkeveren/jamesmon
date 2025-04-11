#include "memory.hpp"

#include <iomanip>

#include <fcntl.h>
#include <unistd.h>
#include <string.h>

void
pgm::insert_memory_info(std::stringstream &output, pgm::error &error) {
	do {
		// There is no way to get available memory from sysconf() or sysinfo().
		// Both can provide total memory and free memory but not available memory or cached memory so not possible to compute available memory.
		// The only way it seems to do this is by parsing the human readable /proc/meminfo.

		// Get meminfo file descriptor
		static int meminfo_fd = open("/proc/meminfo", O_RDONLY);
		if (meminfo_fd == -1) {
			error.strerror().append("Error opening /proc/meminfo.");
			break;
		}

		// Read /proc/meminfo in one chunk to avoid tearing.
		char meminfo[2000];
		lseek(meminfo_fd, 0, SEEK_SET);
		::ssize_t n = ::read(meminfo_fd, meminfo, sizeof(meminfo));
		if (n == -1) {
			error.strerror().append("Error reading /proc/meminfo.");
			break;
		}
		int meminfo_length = strlen(meminfo);

		// Parse specific values from /proc/meminfo.
		// Yes I could have done this with scanf of whatever but this is a personal project and sometimes I like to chase imaginary efficiency that I don't need.
		bool reading_key = true;
		bool ignore_rest_of_line = false;

		std::string key;
		std::string value_string;

		long long mem_total;
		long long mem_available;
		long long *value_ptr = nullptr;

		int values_found = 0;
		int values_required = 2;

		for (int i = 0; i < meminfo_length; i++) {
			char c = meminfo[i];
			// if starting new line
			if (c == '\n') {
				if (value_ptr != nullptr) {
					*value_ptr = std::stoll(value_string) * 1024;
					value_ptr = nullptr;
					
					if (++values_found >= values_required) {
						break; // Stop looking for more values once we have eveything we need.
					}
				}
				// reset to new line state.
				reading_key = true;
				ignore_rest_of_line = false;
				key = "";
				value_string = "";
				continue;
			}

			// if the rest of the line is not useful
			if (ignore_rest_of_line) {
				// this will not run forever because of the previous if statement
				continue;
			}

			if (reading_key) {
				if (c == ':') {
					// end of key is marked by a colon
					reading_key = false;
					if (key == "MemTotal") {
						value_ptr = &mem_total;
					} else if (key == "MemAvailable") {
						value_ptr = &mem_available;
					}
					continue;
				}

				key += c;
			} else {
				if (c == ' ') {
					// Ignore spaces
					continue;
				} else if (c == 'k') {
					// Ignore the kB suffix.
					ignore_rest_of_line = true;
					continue;
				}

				value_string += c;
			}
		}

		// Insert stats into output
		long long mem_unavailable = mem_total - mem_available;

		output
			<< "Memory: "
			<< std::fixed << std::setprecision(3)
			<< mem_unavailable / 1e9 << '/' << std::setprecision(1) << mem_total / 1e9 << "GB "
			<< mem_unavailable / (mem_total / 100) << "% "
			<< "\n\n"
		;

	} while (false);

	if (error) {
		error.append("Error inserting memory info.");
	}
}

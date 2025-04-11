#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <format>
#include <vector>
#include <functional>
#include <string>

#include <ctime>
#include <cstring>

#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>

#include "error.hpp"
#include "arguments.hpp"

#include "time.hpp"
#include "cpu.hpp"
#include "memory.hpp"

namespace pgm {

	void
	refresh(const std::chrono::duration<float> &refresh_interval, pgm::error &error) {
		do {
			// Use a stringstream instead of printing directly to cout so lines are ensured to be printed at the same time. This avoids flickering.
			std::stringstream output;

			// Reset the screen.
			output
				<< "\x1B[H" // move to 0,0
				"\x1B[2J" // clear
			;

			// Time
			insert_time_info(output, error);
			if (error) {
				break;
			}

			// CPU
			insert_cpu_info(refresh_interval, output, error);
			if (error) {
				break;
			}

			// Memory
			insert_memory_info(output, error);
			if (error) {
				break;
			}

			// Battery
//			insert_power_info(output, error);
//			if (error) {
//				break;
//			}

			std::cout << output.rdbuf() << std::flush;
		} while (false);

		if (error) {
			error.append("Error refreshing information.");
		}
	}
}

int
main(int argc, char **argv) {
	pgm::error error;

	// Parse Arguments
	pgm::arguments arguments(argc, argv, error);
	if (error) {
		return error.print();
	}
	// Exit if the result of arguments was to simply print usage.
	if (arguments.printed_usage) {
		return 0;
	}

	// Main Refresh Loop
	std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<float>> next_refresh = std::chrono::steady_clock::now();
	for (;;) {
		refresh(arguments.refresh_interval, error);
		if (error) {
			return error.print();
		}
		next_refresh += arguments.refresh_interval;
		std::this_thread::sleep_until(next_refresh);
	}

}

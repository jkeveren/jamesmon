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

namespace pgm {

	void insert_time_info(std::stringstream &output, [[maybe_unused]] pgm::error &error) {
		// Timestamps
		output
			<< "TAI:  " << std::chrono::tai_clock::now().time_since_epoch() << "\n"
			<< "UNIX: " << std::chrono::utc_clock::now().time_since_epoch() << "\n"
			"\n"
		;

		// Local Time
		std::time_t ctime = std::time(nullptr);
		std::tm *tm = std::localtime(&ctime);
		char local_time[50];
		std::strftime(local_time, sizeof(local_time) * sizeof(local_time[0]), "%F %b %a %T", tm);
		output << local_time << "\n\n";

		// Uptime
		static std::ifstream uptime_ifstream("/proc/uptime");
		uptime_ifstream.seekg(0);
		float uptime;
		uptime_ifstream >> uptime;
		output << "Uptime: " << std::fixed << std::setprecision(0) << uptime << "s (" << std::setprecision(5) << uptime / 86400 /*seconds per day*/ << "d)\n\n";
	}

	struct cpu {
		long long max_cycles;
		long loaded_cycles_fd;
	};

	std::vector<cpu>
	get_cpus(const std::chrono::duration<float> &refresh_interval, pgm::error &error) {
		std::vector<cpu> cpus;

		do {
			double refresh_frequency = std::chrono::duration<float>(1) / refresh_interval;
			int cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
			if (cpu_count == -1) {
				error.strerror().append("Error making sysconf call to get bumber of cpus.");
				break;
			}
			cpus.reserve(static_cast<std::vector<pgm::cpu>::size_type>(cpu_count));

			for (int cpu_index = 0; cpu_index < cpu_count; cpu_index++) {
				cpu cpu;

				// Get max cycles per second.
				std::ifstream max_frequency_stream(std::format("/sys/devices/system/cpu/cpu{}/cpufreq/scaling_max_freq", cpu_index));
				if (!max_frequency_stream.is_open()) {
					error.append(std::format("Error opening max_freq file for cpu {}.", cpu_index));
					break;
				}
				long long thousand_cycles_per_second;
				max_frequency_stream >> thousand_cycles_per_second;
				// Convert to cycles per interval
				long long cycles_per_second = thousand_cycles_per_second * 1000;
				long long cycles_per_interval = static_cast<long long>(static_cast<float>(cycles_per_second) / refresh_frequency); // use float for good accuracy with unusual refresh_interval_ms like 400. This would otherwise round intervals_per_second and give a bad cpu.max_cycles.
				cpu.max_cycles = cycles_per_interval;

				// Get file descriptor for reading cycles used for all processes.
				perf_event_attr pea = {}; // init to 0;
				pea.type = PERF_TYPE_HARDWARE;
				pea.size = sizeof(pea);
				pea.config = PERF_COUNT_HW_CPU_CYCLES;
				pea.disabled = 0;
				// These should already be zeroed but explicit is nice and shows all the exclude options.
				// pea.exclude_idle = 0; // Does not apply to PERF_TYPE_HARDWARE, only PERF_TYPE_SOFTWARE.
				pea.exclude_user = 0;
				pea.exclude_kernel = 0;
				pea.exclude_hv = 0;
				pea.exclude_host = 0;
				pea.exclude_guest = 0;
				pea.exclude_callchain_kernel = 0;
				pea.exclude_callchain_user = 0;
				cpu.loaded_cycles_fd = syscall(SYS_perf_event_open, &pea, -1, cpu_index, -1, 0);
				if (cpu.loaded_cycles_fd == -1) {
					error.strerror().append(std::format("Error getting file descriptor for cpu {} loaded cycle count.", cpu_index));
					break;
				}

				cpus.push_back(cpu);
			}
			if (error) { // Redundant but follows the pattern.
				break;
			}

		} while (false);

		if (error) {
			error.append("Error getting cpus.");
		}

		return cpus;
	}

	void insert_cpu_info(const std::chrono::duration<float> &refresh_interval, std::stringstream &output, pgm::error &error) {
		do {
			// Get CPU info and file descriptors
			static pgm::error static_error;
			static std::vector<pgm::cpu> cpus = get_cpus(refresh_interval, static_error);
			if (static_error) {
				error = static_error;
				break;
			}


			output << "CPUs: ";
			// print usage for each cpu
			for (cpu &cpu: cpus) {
				// Read cycle counts
				long long loaded_cycles;
				read(cpu.loaded_cycles_fd, &loaded_cycles, sizeof(loaded_cycles));
				ioctl(cpu.loaded_cycles_fd, PERF_EVENT_IOC_RESET, 0); // Reset for the next refresh.

				// Find level character to print for cpu
				// static const std::string levels = "_-^";
				static const std::string levels = "_-^";
				static const int max_level = levels.size() - 1;
				long long level_index = loaded_cycles / (cpu.max_cycles / static_cast<long long>(levels.size()));
				// Clamp level index to bounds.
				if (level_index < 0) {level_index = 0;}
				else if (level_index > max_level) {level_index = max_level;}

				// Print usage level character
				output << levels[static_cast<std::string::size_type>(level_index)];
			}
			output << "\n\n";
		} while (false);

		if (error) {
			error.append("Error inserting cpu info.");
		}
	}

	void insert_memory_info(std::stringstream &output, pgm::error &error) {
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
	
	void
	insert_power_info(std::stringstream &output, pgm::error &error) {
		do {
			std::filesystem::directory_iterator power_supply_entries("/sys/class/power_supply");

			for (const std::filesystem::directory_entry &entry : power_supply_entries) {
				// Ignore entries that are not directories (should be none but who knows?).
				if (!entry.is_directory()) {
					continue;
				}

				// Get power supply type
				// Open type file for power supply.
				int type_fd = ::open((entry.path() / "type").c_str(), O_RDONLY);
				if (type_fd == -1) {
					error.strerror();
					break;
				}

				// Read type file
				constexpr ::size_t count = 30;
				char buffer[count];
				::ssize_t read_size = ::read(type_fd, buffer, count);
				if (read_size == -1) {
					error.strerror();
					break;
				}
				::close(type_fd);
				buffer[read_size] = 0; // null terminate.
//				output << std::string(buffer);

				if (strcmp(buffer, "Mains\n") == 0) {
					output << "So many volts!" << std::endl;
				} else if (strcmp(buffer, "Battery\n") == 0) {
					output << "BADDERY!" << std::endl;
				}
			}

			if (error) {
				break;
			}
		} while (false);

		if (error) {
			error.append("Error inserting battery info.");
		}
	}

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

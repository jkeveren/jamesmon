#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <format>
#include <vector>

#include <ctime>
#include <cstring>

#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>

#include "error.hpp"
#include "arguments.hpp"

namespace pgm {

	struct cpu {
		long long max_cycles;
		long loaded_cycles_fd;
	};

	std::vector<cpu>
	get_cpus(int refresh_interval_ms, pgm::error &error) {
		double intervals_per_second = 1e3 / refresh_interval_ms;
		unsigned cpu_count = std::thread::hardware_concurrency();
		std::vector<cpu> cpus;
		cpus.reserve(cpu_count);

		for (unsigned cpu_index = 0; cpu_index < cpu_count; cpu_index++) {
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
			long long cycles_per_interval = static_cast<long long>(static_cast<float>(cycles_per_second) / intervals_per_second); // use float for good accuracy with unusual refresh_interval_ms like 400. This would otherwise round intervals_per_second and give a bad cpu.max_cycles.
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
			cpu.loaded_cycles_fd = syscall(SYS_perf_event_open, &pea, -1, static_cast<int>(cpu_index), -1, 0);
			if (cpu.loaded_cycles_fd == -1) {
				error.strerror().append(std::format("Error getting file descriptor for cpu {} loaded cycle count.", cpu_index));
				break;
			}

			cpus.push_back(cpu);
		}

		if (error) {
			error.append("Error getting cpus.");
		}

		return cpus;
	}

	void
	refresh(std::vector<pgm::cpu> &cpus, pgm::error &error) {
		// Measure actual refresh interval.
		std::chrono::time_point time = std::chrono::steady_clock::now();
		static std::chrono::time_point previous_refresh_time = time;
		std::chrono::duration refresh_interval = time - previous_refresh_time;
		previous_refresh_time = time;
		long long interval_microseconds = std::chrono::duration_cast<std::chrono::microseconds>(refresh_interval).count();

		// Reset the screen.
		std::cout
			<< "\x1B[2J" // clear
			"\x1B[H" // move to 0,0
		;

		// Timestamps
		std::cout
			<< "TAI:  " << std::chrono::tai_clock::now().time_since_epoch() << "\n"
			<< "UNIX: " << std::chrono::utc_clock::now().time_since_epoch() << "\n"
			"\n"
		;

		// Local Time
		std::time_t ctime = std::time(nullptr);
		std::tm *tm = std::localtime(&ctime);
		char local_time[50];
		std::strftime(local_time, sizeof(local_time) * sizeof(local_time[0]), "%F %b %a %T", tm);
		std::cout << local_time << "\n\n";

		// Uptime
		static std::ifstream uptime_ifstream("/proc/uptime");
		uptime_ifstream.seekg(0);
		float uptime;
		uptime_ifstream >> uptime;
		std::cout << "Uptime: " << std::fixed << std::setprecision(0) << uptime << "s (" << std::setprecision(5) << uptime / 86400 /*seconds per day*/ << "d)\n\n";

		// CPU
		std::cout << "CPUs: ";
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
			std::cout << levels[static_cast<std::string::size_type>(level_index)];

			// std::cout << cpu.max_cycles << "\n" << loaded_cycles << "\n" << percentage << "\n\n";
		}

		std::cout << std::endl; // also flushes the stream.
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

	// Get CPU info and file descriptors
	std::vector<pgm::cpu> cpus = get_cpus(arguments.refresh_interval_ms, error);
	if (error) {
		return error.print();
	}

	// Main Refresh Loop
	const std::chrono::milliseconds refresh_interval_ms(arguments.refresh_interval_ms);
	std::chrono::time_point<std::chrono::steady_clock> next_refresh = std::chrono::steady_clock::now();
	for (;;) {
		refresh(cpus, error);
		if (error) {
			return error.print();
		}
		next_refresh += refresh_interval_ms;
		std::this_thread::sleep_until(next_refresh);
	}

}

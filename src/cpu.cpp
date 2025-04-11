#include "cpu.hpp"

#include <fstream>

#include <unistd.h>
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>

#include "error.hpp"

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
		cpus.reserve(static_cast<std::vector<	cpu>::size_type>(cpu_count));

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

void
pgm::insert_cpu_info(const std::chrono::duration<float> &refresh_interval, std::stringstream &output, pgm::error &error) {
	do {
		// Get CPU info and file descriptors
		static pgm::error static_error;
		static std::vector<cpu> cpus = get_cpus(refresh_interval, static_error);
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


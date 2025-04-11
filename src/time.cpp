#include "time.hpp"

#include <chrono>
#include <fstream>

void
pgm::insert_time_info(std::stringstream &output, pgm::error &error) {
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


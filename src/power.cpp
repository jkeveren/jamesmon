#include "power.hpp"

#include <filesystem>

#include <unistd.h>
#include <fcntl.h>
#include <string.h>

void
pgm::insert_power_info(std::stringstream &output, pgm::error &error) {
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


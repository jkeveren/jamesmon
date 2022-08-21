#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <dirent.h>
#include <float.h>
#include <limits.h>
#include <stdarg.h>

// Prints time section
void printTime() {
	const int maxLength = 40;

	struct timeval tv;
	gettimeofday(&tv, NULL);

	struct tm *lt = localtime(&tv.tv_sec);
	char format[maxLength];
	strftime(format, maxLength, "%F %a %d %b %T.%%03d\n", lt);

	printf(format, tv.tv_usec/1000);
}

int printUptime(int uptimeFD) {
	// Seek to start of file.
	lseek(uptimeFD, 0, SEEK_SET);

	// Read entire file.
	unsigned char l = 255;
	char buf[l];
	int n = read(uptimeFD, buf, l);
	if (n == -1) {
		return 1;
	}

	// Parse first number
	double uptime = atof(buf); // s

	printf("Up: %.1es (%.1fd)\n\n", uptime, uptime/60/60/24);

	return 0;
}

int readIntFile(char *path) {
	// Open file
	FILE *file = fopen(path, "r");
	if (file == NULL) {
		return INT_MIN;
	}

	// Fead file
	int value;
	int n = fscanf(file, "%d", &value);
	if (n == EOF) {
		return INT_MIN;
	}

	n = fclose(file);
	if (n == EOF) {
		return INT_MIN;
	}

	return value;
}

// Reads energy in Wh from file and returns energy in kj.
float readBatteryFile(int batIndex, char *fileName) {
	// Format path
	char path[256];
	int n = sprintf(path, "/sys/class/power_supply/BAT%d/%s", batIndex, fileName);
	if (n < 0) {
		return FLT_MAX;
	}

	int value = readIntFile(path);
	if (value == INT_MIN) {
		return FLT_MAX;
	}
	
	// Convert to base unit.
	float SIBaseValue = (float)value/1e6;

	return SIBaseValue;
}

float readEnergyFile(int batIndex, char *fileName) {
	float energy = readBatteryFile(batIndex, fileName);
	// convert Wh to j.
	return energy*3600;
}

// Prints battery section
int printBattery(DIR *PSDir) {
	/*
		bats array
		- Indicates whether a battery with each index was found.
		- 1 for found, 0 for not found.
		- Very simple and efficient sorting technique.
	*/
	int maxBats = 256;
	int bats[maxBats];
	memset(bats, 0, maxBats);
	int highestBat = 0; // highest index of all batteries that are found.
	int batFound = 0; // Indicates whether any batteries are found.
	int ACConnected = 0;

	// Rewind directory stream.
	rewinddir(PSDir);

	// Find entries in "/sys/class/power_supply" directory.
	for (
		struct dirent *entry = readdir(PSDir);
		entry != NULL;
		entry = readdir(PSDir)
	) {
		char *name = entry->d_name;

		// Check if AC is connected.
		if (strcmp(name, "AC") == 0) {
			int online = readIntFile("/sys/class/power_supply/AC/online");
			if (online == INT_MAX) {
				return 1;
			}
			ACConnected = online;
			continue;
		}
		
		// Skip non-batteries.
		if (strncmp(name, "BAT", 3) != 0) {
			continue;
		}
		
		// Record that a battery was found.
		batFound = 1;
		// Parse battery index from name e.g. "BAT1" -> 1.
		int batIndex = atoi(name + 3);
		// Mark index as present.
		bats[batIndex] = 1;
		// Record highest battery index for iteration.
		if (batIndex > highestBat) {
			highestBat = batIndex;
		}
	}

	// Print heading.
	if (batFound) {
		printf("Battery:\n", ACConnected);
	}

	// Print stats for each battery.
	for (int batIndex = 0; batIndex <= highestBat; batIndex++) {
		// Skip battery indexes for batteries that were not found.
		if (!bats[batIndex]) {
			continue;
		}

		// Read files
		// Energy
		float energyNow = readEnergyFile(batIndex, "energy_now");
		if (energyNow == FLT_MAX) {
			return 1;
		}
		float energyFull = readEnergyFile(batIndex, "energy_full");
		if (energyFull == FLT_MAX) {
			return 1;
		}
		// Power
		float power = readBatteryFile(batIndex, "power_now");
		if (power == FLT_MAX) {
			return 1;
		}
		// Voltage
		float voltage = readBatteryFile(batIndex, "voltage_now");
		if (voltage == FLT_MAX) {
			return 1;
		}

		char format[100] = "%d: %.2fV %.2fA %05.2fW %05.1f/%03.fkj %03.f%%";
		if (!ACConnected && power > 0) {
			strcat(format, " %04.1fks");
		}
		strcat(format, "\n");
		
		// Print battery line.
		printf(
			format,
			batIndex,
			voltage,
			power/voltage, // Current
			power,
			energyNow/1000,
			energyFull/1000,
			100/energyFull*energyNow,
			energyNow/power/1000
		);
	}

	// Padding
	if (batFound) {
		printf("\n");
	}

	return 0;
}

// enum EntryType {
// 	EntryTypeCPU,
// 	EntryTypeCPUN,
// 	EntryTypeOther,
// };

int readIntFD(int fd) {
		// Seek to begining of file from previos reads.
		lseek(fd, 0, SEEK_SET);

		// Read file
		int l = 256;
		char buf[l];
		int n = read(fd, buf, l);
		if (n == -1) {
			return 1;
		}
		return atoi(buf);
}

int printCPU(int *freqFDs, int nprocs) {
	printf("CPU:\n");
	int maxFreq = 0;

	for (int i = 0; i < nprocs; i++) {

		int freq = readIntFD(freqFDs[i]);
		
		// Update maxFreq
		if (freq > maxFreq) {
			maxFreq = freq;
		}

		printf("%3d %.1fGHz\n", i, freq/1e6);
	}

	printf("max %.1fGHz\n", maxFreq/1e6);

	// /proc/stat stuff. Commenting out for now. Focusing on instantationus info.
	// int uh = sysconf(_SC_CLK_TCK);
	// printf("%d\n", uh);

	// // Read entire file
	// int maxLength = 20000;
	// char buf[maxLength];
	// read(procStatFD, (void *) buf, maxLength);

	// unsigned long long systemIdle;
	// unsigned long long individualIdle[1000];
	// int CPUCount = 0;

	// // Tokenise into entries
	// char *entryDelim = "\n";
	// char *entrySavePtr;
	// char *entry = strtok_r(buf, entryDelim, &entrySavePtr);
	// while (1) {
	// 	// Tokenise into columns
	// 	char *colDelim = " ";
	// 	char *colSavePtr;
	// 	char *col = strtok_r(entry, colDelim, &colSavePtr);
	// 	enum EntryType entryType;
	// 	int CPUN;
	// 	for (int i = 0; 1; i++) {
	// 		if (i == 0) {
	// 			// Handle type/name column
	// 			if (strncmp(col, "cpu", 3) == 0) {
	// 				if (strlen(col) == 3) {
	// 					// System CPU
	// 					entryType = EntryTypeCPU;
	// 				} else {
	// 					// Individual CPU
	// 					entryType = EntryTypeCPUN;
	// 					// Read CPU number from column
	// 					CPUN = strtol(col + 3, NULL, 10); // 3 is the length of "cpu"
	// 					CPUCount++;
	// 				}
	// 			} else {
	// 				break;
	// 			}
	// 		} else if (i == 4) {
	// 			// Handle data column
	// 			if (entryType == EntryTypeCPU) {
	// 				systemIdle = strtol(col, NULL, 10);
	// 				break;
	// 			} else if (entryType == EntryTypeCPUN) {
	// 				individualIdle[CPUN] = strtol(col, NULL, 10);
	// 				break;
	// 			}
	// 		}

	// 		col = strtok_r(NULL, colDelim, &colSavePtr);
	// 		if (col == NULL) {
	// 			break;
	// 		}
	// 	}

	// 	entry = strtok_r(NULL, entryDelim, &entrySavePtr);
	// 	if (entry == NULL) {
	// 		break;
	// 	}
	// }

	// printf("System: %d\n", systemIdle);
	// for (int i = 0; i < CPUCount; i++) {
	// 	printf("%d: %d\n", i, individualIdle[i]);
	// }
	return 0;
}

int printAll(int uptimeFD, DIR *PSDir, int *freqFDs, int nprocs) {
	printTime();

	int e = printUptime(uptimeFD);
	if (e == 1) {
		perror(NULL);
		return 5;
	}

	e = printBattery(PSDir);
	if (e == 1) {
		perror(NULL);
		return 5;
	}

	e = printCPU(freqFDs, nprocs);
	if (e == 1) {
		perror(NULL);
		return 6;
	}
}

int main(int argc, char *argv[]) {
	// Parse arguments.
	int refreshRate = 0;
	char opt;
	while ((opt = getopt(argc, argv, "r:h")) != -1) {
		if (opt == '?') {
			return 7;
		}
		switch (opt) {
			case 'r':
				char *endP;
				refreshRate = strtol(optarg, &endP, 10);
				printf("%p, %p, %d\n", endP, optarg, endP == optarg);
				if (refreshRate == LONG_MIN || refreshRate == LONG_MAX) {
					perror(NULL);
					return 8;
				} else if (endP == optarg) {
					fprintf(stderr, "Refresh rate requires a number but got \"%s\".\n", optarg);
					return 9;
				}
				printf("%d\n", refreshRate);
				break;
			case 'h':
				printf("Usage: syspector [-r <refresh-rate (Hz)>]\n");
				return 9;
				break;
		}
	}

	// Open uptime file.
	int uptimeFD = open("/proc/uptime", O_RDONLY);
	if (uptimeFD == -1) {
		perror("NULL");
		return 1;
	}

	// Open power supply directory.
	DIR *PSDir = opendir("/sys/class/power_supply");
	if (PSDir == NULL) {
		perror(NULL);
		return 2;
	}

	// Get processor frequency FDs.
	int nprocs = get_nprocs();
	int freqFDs[nprocs];
	for (int i = 0; i < nprocs; i++) {
		char path[256];
		int n = sprintf(path, "/sys/devices/system/cpu/cpufreq/policy%d/scaling_cur_freq", i, "cur");
		if (n < 0) {
			perror(NULL);
			return 3;
		}
		freqFDs[i] = open(path, O_RDONLY);
		if (freqFDs[i] == -1) {
			perror("NULL");
			return 4;
		}
	}

	if (refreshRate == 0) {
		int e = printAll(uptimeFD, PSDir, freqFDs, nprocs);
		if (e != 0) {
			perror(NULL);
			return e;
		}
	} else {
		const int interval = 1e6 / refreshRate; // us
		// Clear terminal.
		for (int i = refreshRate;; i++) {
			// Clear the terminal every second
			if (i == refreshRate) {
				i = 0;
				printf("\e[2J");
			}
			// go to 0, 0
			printf("\033[1;1H");

			int e = printAll(uptimeFD, PSDir, freqFDs, nprocs);
			if (e != 0) {
				perror(NULL);
				return e;
			}

			// Very dumb sleep. Should really calc remaining interval time.
			usleep(interval);
		}
	}

	return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <errno.h>
#include <dirent.h>
#include <float.h>

void printTime() {
	const int maxLength = 40;

	struct timeval tv;
	gettimeofday(&tv, NULL);

	struct tm *lt = localtime(&tv.tv_sec);
	char format[maxLength];
	strftime(format, maxLength, "%F %a %d %b %T.%%03d\n\n", lt);

	printf(format, tv.tv_usec/1000);
}

float readEnergyFile(char *fmt, char *state) {
	// Format path
	char path[100];
	int n = sprintf(path, fmt, state);
	if (n < 0) {
		return FLT_MAX;
	}

	// Open file
	FILE *file = fopen(path, "r");
	if (file == NULL) {
		return FLT_MAX;
	}

	// Fead file
	float uWh; // Micro Watt Hours
	n = fscanf(file, "%f", &uWh);
	if (n == EOF) {
		return FLT_MAX;
	}
	
	// Convert to kj
	float kj = uWh*3600/1e9;

	return kj;
}

int printBattery(DIR *PSDir) {
	/*
		bats array
		- Indicates whether a battery with each index was found.
		- 1 for found, 0 for not found.
		- Very simple and efficient sorting technique.
	*/
	int maxBats = 255;
	int bats[maxBats];
	memset(bats, 0, maxBats);
	int highestBat = 0; // highest index of all batteries that are found.
	int batFound = 0; // Indicates whether any batteries are found.

	// Find batteries in "/sys/class/power_supply" directory.
	for (
		struct dirent *entry = readdir(PSDir);
		entry != NULL;
		entry = readdir(PSDir)
	) {
		char *name = entry->d_name;
		// Skip non-batteries
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
		printf("Battery:\n");
	}

	// Print stats for each battery.
	for (int batIndex = 0; batIndex <= highestBat; batIndex++) {
		// Skip battery indexes for batteries that were not found.
		if (!bats[batIndex]) {
			continue;
		}

		// Create a path format string for battery. (note the "%%s" at the end fo the initial format string).
		char fmt[100];
		int n = sprintf(fmt, "/sys/class/power_supply/BAT%d/energy_%%s", batIndex);
		if (n < 0) {
			return 1;
		}

		// Read each energy file.
		float kjNow = readEnergyFile(fmt, "now");
		if (kjNow == FLT_MAX) {
			return 1;
		}
		float kjFull = readEnergyFile(fmt, "full");
		if (kjFull == FLT_MAX) {
			return 1;
		}
		float kjDesign = readEnergyFile(fmt, "full_design");
		if (kjDesign == FLT_MAX) {
			return 1;
		}
		
		// Print battery line.
		printf("%d: %06.2f/%05.1f/%05.1fkj (%06.2f%%)\n", batIndex, kjNow, kjFull, kjDesign, 100/kjFull*kjNow);
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

int printCPU(int *freqFDs, int nprocs) {
	printf("CPU:\n");
	for (int pi = 0, fdi = 0; pi < nprocs; pi++, fdi += 3) {
		int l = 20;
		char buf[l];

		int n = read(freqFDs[fdi], buf, l);
		if (n == -1) {
			return 1;
		}
		int min = strtol(buf, NULL, 10);

		n = read(freqFDs[fdi + 1], buf, l);
		if (n == -1) {
			return 1;
		}
		int max = strtol(buf, NULL, 10);

		n = read(freqFDs[fdi + 2], buf, l);
		if (n == -1) {
			return 1;
		}
		int cur = strtol(buf, NULL, 10);

		printf("%.1f/%.1f/%.1fGHz\n", min/1e6, cur/1e6, max/1e6);
	}

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

int printAll(DIR *PSDir,int *freqFDs, int nprocs) {
	printTime();

	int e = printBattery(PSDir);
	if (e != 0) {
		return 1;
	}

	e = printCPU(freqFDs, nprocs);
	if (e != 0) {
		return 1;
	}

	// USE sysfs for everything but CPU!
	return 0;
}

int main() {
	// Get file descriptors for reuse once monitoring is implemented.
	// char *path = "/proc/stat";
	// int procStatFD = open(path, O_RDONLY);
	// if (procStatFD == -1) {
	// 	printf("Unable to open %s\n", path);
	// 	return 1;
	// }

	// Processor frequency file descriptors
	int nprocs = get_nprocs();
	int freqFDs[nprocs * 3];

	int l = 50; // Allocated path length
	char *format = "/sys/devices/system/cpu/cpufreq/policy%d/scaling_%s_freq";

	for (int pi = 0, fdi = 0; pi < nprocs; pi++, fdi += 3) {
		// Min
		char minPath[l];
		int n = sprintf(minPath, format, pi, "min");
		if (n < 0) {
			perror(NULL);
			return 1;
		}
		freqFDs[fdi] = open(minPath, O_RDONLY);

		// Max
		char maxPath[l];
		n = sprintf(maxPath, format, pi, "max");
		if (n < 0) {
			perror(NULL);
			return 1;
		}
		freqFDs[fdi + 1] = open(maxPath, O_RDONLY);

		// Current
		char curPath[l];
		n = sprintf(curPath, format, pi, "cur");
		if (n < 0) {
			perror(NULL);
			return 1;
		}
		freqFDs[fdi + 2] = open(curPath, O_RDONLY);
	}

	// Get list of power supplies
	DIR *PSDir = opendir("/sys/class/power_supply");
	if (PSDir == NULL) {
		return 1;
	}

	int e = printAll(PSDir, freqFDs, nprocs);
	if (e != 0) {
		perror(NULL);
		return 1;
	}
	return 0;
}
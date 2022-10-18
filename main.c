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
#include <errno.h>
#include <math.h>

char *fineChars[] = {
	" ",
	"\u258F",
	"\u258E",
	"\u258D",
	"\u258C",
	"\u258B",
	"\u258A",
	"\u2589",
	"\u2588"
};

// char *fineChars[] = {
// 	"0",
// 	"1",
// 	"2",
// 	"3",
// 	"4",
// 	"5",
// 	"6"
// };

#define fineCharCount (sizeof fineChars / sizeof fineChars[0])
#define maxFineChar (fineCharCount - 1)

/*
Success: Prints a bar that represents a value as a portion of another value.
Error: Sets `err` to 1 and errno appropriately.
Arguments:
	s: String to write bar in.
		- Must have a size of at least (`maxChars` + 1) * 4 + 1 to allow for 4 byte unicode characters, max value line and null termination.
	length: Visual length of the bar.
		- Does not include the max value character on end of bar.
	minValue: Value representing 0 on the bar.
	maxValue: Value that fills the bar.
	value: Value to display with the bar.
	err: Should be 0 before calling. Get's seto to 1 on error.
*/
char makeBar(char *s, int length, float minValue, float maxValue, float value, int *err) {
	/*
	A bar is made of four sections:
		| 1 |2| 3 |4|
		█████▌     |

		1. Course: Full characters that display most of the value.
		2. Fine: A non-full width character that provides high resolution at the "tip" of the bar.
		3. Spaces: Literal space chars that overwrite the previous frame.
		4. Max: Character representing the maximum possible value.
	*/

	// Clear buffer.
	s[0] = 0;

	// Limit value to between min and max.
	if (value < minValue) {
		value = minValue;
	} else if (value > maxValue) {
		value = maxValue;
	}

	float range = maxValue - minValue;

	// Convert `value` to fine units.
	// This also makes the minimum `fineValue` 1 so a `value` of 0 is still visible
	int fineValue = ((length * fineCharCount) - 1) / range * (value - minValue);
	fineValue++;

	// Convert `value` to coarse units (characters).
	float coarseValue = fineValue / fineCharCount;

	// Get index for fine character to print.
	int fineCharIndex = fineValue % fineCharCount;

	int i = 0;
	// Print coarse chars.
	while (i < coarseValue) {
		strcat(s + i++, fineChars[maxFineChar]);
	}

	// Print fine char if index is more than 0.
	// Don't print blank space (This fixes a bug where the limit line is pushed out one extra space when bar is full).
	if (fineCharIndex) {
		strcat(s + i++, fineChars[fineCharIndex]);
	}

	while (i < length) {
		strcat(s + i++, " ");
	}

	// Print max value line
	strcat(s, "\u258F");
}

/*
Success: Returns a long int that is read from start of file at `path`.
Error: Sets `err` to 1 and errno appropriately.
*/
long readLongFD(int fd, int *err) {
	// Seek to start of file
	lseek(fd, 0, SEEK_SET);

	// Read file
	int l = 256;
	char buf[l];
	int n = read(fd, buf, l);
	if (n == -1) {
		*err = 1;
		return 0;
	}

	errno = 0;
	long value = strtol(buf, NULL, 10);
	if (errno != 0) {
		*err = 1;
		return 0;
	}

	return value;
}

/*
Success: Returns a long int that is read from start of file at `path`.
Error: Sets `err` to 1 and errno appropriately.
*/
long readLongFile(char *path, int *err) {
	// Open file
	FILE *file = fopen(path, "r");
	if (file == NULL) {
		*err = 1;
		return 0;
	}

	// Read file
	long value;
	fscanf(file, "%ld", &value);
	if (errno != 0) {
		*err = 1;
		return 0;
	}

	int n = fclose(file);
	if (errno != 0) {
		*err = 1;
		return 0;
	}

	return value;
}

/*
Success: Returns float value read from file at "/sys/class/power_supply/BAT<batIndex>/<fileName>".
Error: Sets `err` to 1 and errno appropriately.
*/
float readBatteryFile(int batIndex, char *fileName, int *err) {
	// Format path
	char path[256];
	int n = sprintf(path, "/sys/class/power_supply/BAT%d/%s", batIndex, fileName);
	if (n < 0) {
		*err = 1;
		return 0;
	}

	long value = readLongFile(path, err);
	if (*err) {
		return 0;
	}
	
	// Convert to base unit.
	float SIBaseValue = (float)value/1e6;

	return SIBaseValue;
}

/*
Similar to: `readBatteryFile`
Difference: Converts return value from Wh to j.
Note: `fileName` should typically be "energy_now", "energy_full" or "energy_full_design".
*/
float readEnergyFile(int batIndex, char *fileName, int *err) {
	float energy = readBatteryFile(batIndex, fileName, err);
	if (*err) {
		return 0; // use same error value as `readBatteryFile`
	}
	// convert Wh to j.
	return energy*3600;
}

/*
Success: Returns frequency for `cpu`.
Error: Sets `err` to 1 and errno appropriately.
*/
long readCPUFrequencyFile(int cpu, char *limitName, int *err) {
	// Format path
	char path[256];
	int n = sprintf(path, "/sys/devices/system/cpu/cpufreq/policy%d/scaling_%s_freq", cpu, limitName);
	if (n < 0) {
		*err = 1;
		return 0;
	}

	// Read file
	long freq = readLongFile(path, err);
	if (*err) {
		return 0;
	}

	return freq;
}

/*
Success: Prints the time section.
Error: Sets `err` to 1 and errno appropriately.
*/
void printTime(int *err) {
	const int maxLength = 256;

	// Get high resolution time.
	struct timeval tv;
	int s = gettimeofday(&tv, NULL);
	if (s == -1) {
		*err = 1;
		return;
	}

	// Get low resolution time
	struct tm *lt = localtime(&tv.tv_sec);
	if (lt == NULL) {
		*err = 1;
		return;
	}

	// Format low resolution time with format chars for high resolution time (note the %% near the end).
	char format[maxLength];
	int n = strftime(format, maxLength, "%F %b %a %T.%%03d\n", lt);
	if (n == 0) {
		*err = 1;
		return;
	}

	// Print formatted low res time with high res time on end.
	n = printf(format, tv.tv_usec/1000);
	if (n == 0) {
		*err = 1;
		return;
	}

	// n = 8*2-1;
	// int barLength = 2;
	// char bar[(barLength+1) * 4 + 1];
	// for (int i = 0; i <= n; i++) {
	// 	makeBar(bar, barLength, 0, n, i, err);
	// 	printf("%s\n", bar);
	// }

	// makeBar(bar, 1, 0, 1, sin(tv.tv_usec / 1e6 * 2 * M_PI) / 2 + 0.5, err);
	// printf("%s\n", bar);

	// makeBar(bar, 2, 0, 1, sin(tv.tv_usec / 1e6 * 2 * M_PI) / 2 + 0.5, err);
	// printf("%s\n", bar);

	// makeBar(bar, 3, 0, 1, sin(tv.tv_usec / 1e6 * 2 * M_PI) / 2 + 0.5, err);
	// printf("%s\n", bar);
}

/*
Success: Prints the uptime section.
Error: Sets `err` to 1 and errno appropriately.
*/
void printUptime(int uptimeFD, int *err) {
	// Seek to start of file.
	int n = lseek(uptimeFD, 0, SEEK_SET);
	if (n == -1) {
		*err = 1;
		return;
	}

	// Read file.
	unsigned char l = 255;
	char buf[l];
	n = read(uptimeFD, buf, l);
	if (n == -1) {
		*err = 1;
		return;
	}

	// Parse first number
	errno = 0;
	double uptime = strtof(buf, NULL); // s
	if (errno != 0) {
		*err = 1;
		return;
	}

	n = printf("Up: %.1es (%.1fd)\n", uptime, uptime/60/60/24);
	if (n == 0) {
		*err = 1;
		return;
	}
}

/*
Success: Prints the battery section.
Error: Sets `err` to 1 and errno appropriately.
*/
void printBattery(DIR *PSDir, int *err) {
	int n;

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
	struct dirent *entry;
	while ((entry = readdir(PSDir)) != NULL) {
		char *name = entry->d_name;

		// Check if AC is connected.
		if (strcmp(name, "AC") == 0) {
			long online = readLongFile("/sys/class/power_supply/AC/online", err);
			if (*err) {
				return;
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
		errno = 0;
		long batIndex = strtol(name + 3, NULL, 10);
		if (*err) {
			*err = 1;
			return;
		}
		// Mark index as present.
		bats[batIndex] = 1;
		// Record highest battery index for iteration.
		if (batIndex > highestBat) {
			highestBat = batIndex;
		}
	}

	// Print heading.
	if (batFound) {
		n = printf("\nBattery:\n");
		if (n == 0) {
			*err = 1;
			return;
		}
	}

	// Print stats for each battery.
	for (int batIndex = 0; batIndex <= highestBat; batIndex++) {
		// Skip battery indexes for batteries that were not found.
		if (!bats[batIndex]) {
			continue;
		}

		// Read files
		// Energy
		float energyNow = readEnergyFile(batIndex, "energy_now", err);
		if (*err) {
			return;
		}
		float energyFull = readEnergyFile(batIndex, "energy_full", err);
		if (*err) {
			return;
		}
		float energyFullDesign = readEnergyFile(batIndex, "energy_full_design", err);
		if (*err) {
			return;
		}
		// Power
		float power = readBatteryFile(batIndex, "power_now", err);
		if (*err) {
			return;
		}
		// Voltage
		float voltage = readBatteryFile(batIndex, "voltage_now", err);
		if (*err) {
			return;
		}

		float secondsRemaining;
		float hoursRemaining;

		char bar[50];
		makeBar(bar, 3, 0, energyFull, energyNow, err);

		char format[100] = "\033[2K%d: %.2fV %.2fA %05.2fW %05.1f/%03.f/%03.fkj %03.f%% %s";
		if (!ACConnected && power > 0) {
			strcat(format, "%.1fks (%.1fh)");

			secondsRemaining = energyNow/power;
			hoursRemaining = secondsRemaining/60/60;
		}
		strcat(format, "\n");
		
		// Print battery line.
		n = printf(
			format,
			batIndex,
			voltage,
			power/voltage, // current
			power,
			energyNow/1000,
			energyFull/1000,
			energyFullDesign/1000,
			100/energyFull*energyNow, // percentage
			bar,
			secondsRemaining/1000,
			hoursRemaining
		);
		if (n == 0) {
			*err = 1;
			return;
		}
		// printBar(5, 0, energyFull, energyNow, err);
		// printf("\n");
	}
}

/*
Success: Prints the CPU section.
Error: Sets `err` to 1 and errno appropriately.
*/
void printCPU(int nprocs, int *freqFDs, int *freqMins, int *freqMaxs, int *err) {
	int n = printf("\nCPU:\n");
	if (n == 0) {
		*err = 1;
		return;
	}

	// Values for CPU with highest current freq.
	int highestMin = 0;
	int highestMax = 0;
	int highestCur = 0;

	char bar[50];
	int barLength = 5;

	// Print info for each processor.
	for (int i = 0; i < nprocs; i++) {
		int cur = readLongFD(freqFDs[i], err);
		int min = freqMins[i];
		int max = freqMaxs[i];
		
		// Update maxFreq
		if (cur > highestCur) {
			highestMin = min;
			highestMax = max;
			highestCur = cur;
		}

		makeBar(bar, barLength, min, max, cur, err);
		if (*err) {
			return;
		}

		n = printf("\033[2K%3d %.1fGHz %s\n", i, cur/1e6, bar);
		if (n == 0) {
			*err = 1;
			return;
		}
	}

	// Print highest CPU frequency.
	makeBar(bar, barLength, highestMin, highestMax, highestCur, err);
	if (*err) {
		return;
	}
	n = printf("\033[2Kmax %.1fGHz %s\n", highestCur/1e6, bar);
	if (n == 0) {
		*err = 1;
		return;
	}
}

/*
Success: Returns kiB as SI GB float.
Error: Impossible.
*/
float kiBToSIGB(float kiB) {
	return (kiB*1024)/1e9;
}

/*
Success: Prints the memory section.
Error: Sets `err` to 1 and errno appropriately.
*/
void printMemory(FILE *memFile, int *err) {
	int n = printf("\nMemory:\n");
	if (n < 0) {
		*err = 1;
		return;
	}
	// Go to start of meminfo file.
	rewind(memFile);

	// Memory properties
	float total = 0;
	float available = 0;

	// Iterate key, value lines.
	char key[256];
	float value; // Typically kiB (written as kB in meminfo) but some are unitless.
	while (1) {
		errno = 0;
		n = fscanf(memFile, "%s %f kB\n", &key, &value);
		if (errno != 0) {
			*err = 1;
			return;
		}
		if (n == EOF) {
			break;
		}

		if (strcmp(key, "MemTotal:") == 0) {
			total = kiBToSIGB(value);
		} else if (strcmp(key, "MemAvailable:") == 0) {
			available = kiBToSIGB(value);
		}
	}

	float used = total - available;

	char bar[20];
	makeBar(bar, 5, 0, total, used, err);
	printf("\033[2K%.3f/%.1fGB %.f%% %s\n", used, total, 100/total*(used), bar);
}

/*
Success: Prints all sections.
Error: Sets `err` to 1 and errno appropriately.
*/
void printAll(
	int uptimeFD,
	DIR *PSDir,
	int nprocs,
	int *freqFDs,
	int *freqMins,
	int *freqMaxs,
	FILE *memFile,
	int *err
) {
	printTime(err);
	if (*err) {
		return;
	}

	printUptime(uptimeFD, err);
	if (*err) {
		return;
	}

	printBattery(PSDir, err);
	if (*err) {
		return;
	}

	printCPU(nprocs, freqFDs, freqMins, freqMaxs, err);
	if (*err) {
		return;
	}

	printMemory(memFile, err);
	if (*err) {
		return;
	}
}

int main(int argc, char *argv[]) {
	int e = 0;
	int *err = &e;
	int n = 0;

	// Parse arguments.
	float refreshRate = 0;
	char opt;
	while ((opt = getopt(argc, argv, "r:h")) != -1) {
		if (opt == '?') {
			return 7;
		}
		switch (opt) {
			case 'h':
				n = printf("Usage: syspector [-r <refresh-rate (Hz)>]\n");
				if (n == 0) {
					perror("Error printing help message.");
					return 1;
				}
				return 1;
				break;
			case 'r':
				char *endP;
				refreshRate = strtof(optarg, &endP);
				errno = 0;
				if (errno != 0) {
					n = fprintf(stderr, "Refresh rate requires a number but got \"%s\".\n", optarg);
					if (n == 0) {
						perror("Error printing refresh rate message.");
						return 1;
					}
					return 1;
				}
				break;
		}
	}

	// Open uptime file.
	int uptimeFD = open("/proc/uptime", O_RDONLY);
	if (uptimeFD == -1) {
		perror("Error opening '/proc/uptime' file.");
		return 1;
	}

	// Open power supply directory.
	DIR *PSDir = opendir("/sys/class/power_supply");
	if (PSDir == NULL) {
		perror("Error opening '/sys/class/power_supply' directory");
		return 1;
	}

	// Get processor frequency FDs and min/max.
	int nprocs = get_nprocs();
	int freqFDs[nprocs];
	int freqMins[nprocs];
	int freqMaxs[nprocs];
	char *basePath = "";
	for (int i = 0; i < nprocs; i++) {
		// Get "scaling_cur_freq" FD
		char path[256];
		int n = sprintf(path, "/sys/devices/system/cpu/cpufreq/policy%d/scaling_cur_freq", i);
		if (n < 0) {
			perror("Error formatting scaling_cur_freq path.");
			return 1;
		}
		freqFDs[i] = open(path, O_RDONLY);
		if (freqFDs[i] == -1) {
			perror("Error opening scaling_cur_freq file.");
			return 1;
		}

		// Get limits
		// min
		int limit = readCPUFrequencyFile(i, "min", err);
		if (*err) {
			perror("Error opening min frequency file.");
			return 1;
		}
		freqMins[i] = limit;

		// max
		limit = readCPUFrequencyFile(i, "max", err);
		if (*err) {
			perror("Error opening max frequency file.");
			return 1;
		}
		freqMaxs[i] = limit;
	}

	// Open memory file.
	FILE *memFile = fopen("/proc/meminfo", "r");
	if (memFile == NULL) {
		perror("Error opening meminfo file.");
		return 1;
	}

	if (refreshRate == 0) {
		printAll(uptimeFD, PSDir, nprocs, freqFDs, freqMins, freqMaxs, memFile, err);
		if (*err) {
			perror("Error printing all sections.");
			return 1;
		}
	} else {
		const int interval = 1e6 / refreshRate; // us
		// Clear terminal.
		for (int i = refreshRate;; i++) {
			// Clear the terminal every second
			if (i == refreshRate || refreshRate < 1) {
				i = 0;
				printf("\e[2J");
			}
			// go to 0, 0
			printf("\033[1;1H");

			printAll(uptimeFD, PSDir, nprocs, freqFDs, freqMins, freqMaxs, memFile, err);
			if (*err) {
				perror("Error printing all sections.");
				return 1;
			}

			// Very dumb sleep. Should really calc remaining interval time.
			usleep(interval);
		}
	}

	return 0;
}

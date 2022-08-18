#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>

char *getTimeString() {
	const int maxLength = 50;

	struct timeval tv;
	gettimeofday(&tv, NULL);

	struct tm *lt = localtime(&tv.tv_sec);
	char format[maxLength];
	strftime(format, maxLength, "%T.%%03d %F %a %d %b", lt);

	char *timeString = malloc(maxLength);
	sprintf(timeString, format, tv.tv_usec/1000);

	return timeString;
}

int main() {
	char *timeString = getTimeString(&timeString);
	puts(timeString);

	// USE SYSFS for everything!

	return 0;
}
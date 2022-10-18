CC ?= gcc

syspect: main.c
	$(CC) -Werror -lm -g -O0 main.c -o syspect

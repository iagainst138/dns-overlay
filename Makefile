CC=gcc
CFLAGS=-Wall -g -std=gnu11 -pedantic -Werror -lcap


build: clean
	$(CC) dns-overlay.c $(CFLAGS) -o dns-overlay


setcap: build
	sudo setcap cap_sys_admin+epi dns-overlay


valgrind: build
	sudo valgrind --leak-check=full --track-origins=yes --trace-children=yes ./dns-overlay -v -c uptime -f dns-overlay.c


clean:
	rm -f dns-overlay

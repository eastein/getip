#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void pushbuffer(int fd, char* buf, size_t count) {
	size_t sent = 0;
	while (sent < count) {
		sent += write(fd, &buf[sent], count - sent);
	}
}

void pullbuffer(int fd, char* buf, size_t count) {
	size_t got = 0;
	while (got < count) {
		got += read(fd, &buf[got], count - got);
	}
}

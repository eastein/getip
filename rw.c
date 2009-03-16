#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
copyright 2009 Eric Stein <eastein@wpi.edu>
Licensed under the GNU Public License version 2 or any subsequent version
published by the Free Software Foundation, at your option.  If you want to use
this code in a non-GPL application, contact me for permission.
*/

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

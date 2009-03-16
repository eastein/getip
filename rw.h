#ifndef RW_H
#define RW_H

/*
copyright 2009 Eric Stein <eastein@wpi.edu>
Licensed under the GNU Public License version 2 or any subsequent version
published by the Free Software Foundation, at your option.  If you want to use
this code in a non-GPL application, contact me for permission.
*/

void pushbuffer(int fd, char* buf, size_t count);
void pullbuffer(int fd, char* buf, size_t count);

#endif

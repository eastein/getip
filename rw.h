#ifndef RW_H
#define RW_H

void pushbuffer(int fd, char* buf, size_t count);
void pullbuffer(int fd, char* buf, size_t count);

#endif

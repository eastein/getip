#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#include <sys/time.h>
#include <sys/select.h>

#include <sys/wait.h>
#include <signal.h>
#include <semaphore.h>
#include <pthread.h>

#include <string.h>
#include <stdio.h>

#include "rw.h"
#include "tsq.cpp"
#include "defs.h"

#define CWAIT 100
#define DIE_USEC 1000
#define RTHREADS 100

struct sockaddr_in waddr;
int wsock;

void assert(bool truth, const char* string) {
	if (!truth) {
		printf("FATAL ERROR: %s\n", string);
		exit(1);
	}
}

void acceptloop(tsq<struct connection_descriptor>* fdqueue, int sockfd) {
	//struct for storing client address (not stored)
	struct sockaddr_in caddr;
	unsigned int clen = sizeof(caddr);
	struct connection_descriptor cdesc;

	//support structures for select waits
	fd_set ss;
	struct timeval accept_timeout;

	while (1) {
		//wait up to DIE_USEC microseconds for a connection
		accept_timeout.tv_sec = 0;
		accept_timeout.tv_usec = DIE_USEC;
		FD_ZERO(&ss);
		FD_SET(sockfd, &ss);
		if (select(sockfd + 1, &ss, &ss, &ss, &accept_timeout) > 0) {
			//accept the connection
			int cfd = accept(sockfd, (struct sockaddr*) &caddr, &clen);

			//store the descriptor and IP address
			char* ipbuf = (char*) malloc(256);
			if (inet_ntop(AF_INET, &caddr.sin_addr, ipbuf, 256) == NULL) {
				bcopy("Unable to determine client address.", ipbuf, 36 * sizeof(char));
			}

			cdesc.fd = cfd;
			cdesc.ipaddr = ipbuf;

			//store the file descriptor on the queue if it's valid
			if (cfd >= 0) {
				fdqueue->push(cdesc);
			} else {
				#ifdef DEBUG_NETWORK
				printf("%d: accept error: %d\n", pid_me, cfd);
				#endif
			}
		}
	}
}

void sendresponse(int fd, char* ipstring) {
	int ipsize = strlen(ipstring);
	char* response = (char*) malloc(370 * sizeof(char));
	sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Transer-Encoding: 7bit\r\nContent-Length: %d\r\n\r\n%s\r\n", ipsize + 2, ipstring);
	pushbuffer(fd, response, strlen(response));
	free(response);
}

void* http_worker(void* q) {
	tsq<struct connection_descriptor>* tq = (tsq<struct connection_descriptor>*) q;
	struct connection_descriptor cdesc;

	//buffer management
	int BUFBLOCK = 512;
	int BUFFREE = 512;
	int BUFSIZE = BUFBLOCK;
	int BUFUSED = 0;

	//allocate the buffer
	char* nbuf = NULL;
	char* buf = (char*) malloc (BUFSIZE * sizeof(char));
	assert(buf != NULL, "could not allocate buffer");

	int tid = 1;
	cdesc.fd = 0;
	while (cdesc.fd >= 0) {
		cdesc = tq->pop();

		bool done = false;
		//number of recognized parts of a CRLF read
		int crlfmode = 0;
		while (!done) {
			// check if the buffer is big enough.
			if (BUFSIZE - BUFUSED < BUFFREE) {
				// it's not. make it bigger by BUFBLOCK bytes.
				int nbsize = BUFSIZE + BUFBLOCK;
				
				// allocate new buffer size
				nbuf = (char*) realloc(buf, sizeof(char) * nbsize);
				assert(nbuf != NULL, "could not enlarge buffer");
				buf = nbuf;
			}

			#ifdef DEBUG_BUFFERS
			printf("reading up to %d bytes\n", BUFSIZE - BUFUSED);
			#endif

			// read up to the size of the buffer from the network
			int r = read(cdesc.fd, &buf[BUFUSED], BUFSIZE - BUFUSED);
			assert(r >= 0, "read error from pipe");
			BUFUSED += r;

			#ifdef DEBUG_BUFFERS
			printf("got %d bytes, buffer is now%d\n", r, BUFUSED);
			#endif

			#define CR 13
			#define LF 10
			for (int i = 0; i < BUFUSED; i++) {
				//state machine for recognizing CRLF tuples
				if (crlfmode == 0) {
					if (buf[i] == CR) {
						crlfmode++;
					}
				} else if (crlfmode == 1) {
					if (buf[i] == LF) {
						crlfmode++;
					} else {
						crlfmode = 0;
					}
				}

				if (crlfmode == 2) {
					//reset the CRLF recognizing state machine
					crlfmode = 0;
					//determine size of the line

					int lsize = i - 1;
					buf[lsize] = 0;

					//process line here
					if (lsize == 0) {
						//HTTP request is complete
						done = true;
					} else {
						//detect proxy header, copy IP from it
						if (strncasecmp(buf, "X-Forwarded-For: ", 17) == 0) {
							char* comma = strchr(buf, ',');
							if (comma != NULL)
								comma[0] = 0;

						}
					}
					
					//purge line from buffer, update buffer status variables
					BUFUSED -= (lsize + 2);
					if (BUFUSED > 0) {
						#ifdef DEBUG_BUFFERS
						printf("buffer now is of size %d\n", BUFUSED);
						#endif
						bcopy(&buf[i+1], buf, BUFUSED);
					}
					//continue the loop at an earlier part of the buffer
					i = -1;
				}
			}
		}
		sendresponse(cdesc.fd, cdesc.ipaddr);
		close(cdesc.fd);
		free(cdesc.ipaddr);
	}

	free(buf);

	#ifdef DEBUG_MANAGEMENT
	printf("killing self\n");
	#endif
	pthread_exit(NULL);		
}

void die(const char* s) {
	printf("ERROR: %s\n", s);
	exit(1);
}

int main() {
	unsigned short wport = 8080;

	if ((wsock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		die("socket() failed");
	
	waddr.sin_family = AF_INET;
	waddr.sin_addr.s_addr = htonl(INADDR_ANY);
	waddr.sin_port = htons(wport);

	int reuse = 1;
	setsockopt(wsock, SOL_SOCKET, SO_REUSEADDR, (char *) &reuse, sizeof(reuse));

	if (bind(wsock, (struct sockaddr*) &waddr, sizeof(waddr)) < 0)
		die("bind() failed");
	
	if (listen(wsock, CWAIT) < 0)
		die("listen() failed");

	// create a connection queue for passing the connections to the workers
	tsq<struct connection_descriptor>* rtsq = new tsq<struct connection_descriptor>();

	// create threads
	pthread_t rloops[RTHREADS];
	for (int i = 0; i < RTHREADS; i++) {
		if (pthread_create(&rloops[i], NULL, http_worker, (void*) rtsq)) {
			die("failed to create thread");
		}
	}

	//single threaded acceptor, pipes to queue of connection descriptors
	acceptloop(rtsq, wsock);

	//join threads and delete queue, as if this is ever going to happen
	for (int i = 0; i < RTHREADS; i++) {
		pthread_join(rloops[i], NULL);
	}
	delete rtsq;
}

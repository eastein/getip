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

#define READFD 0
#define WRITEFD 1

#define CWAIT 100
#define DIE_USEC 1000
#define RTHREADS 2
//DO NOT INCREASE ABOVE 1.  THERE IS NO THREAD SAFETY FOR WRITES.
#define WTHREADS 1

struct sockaddr_in waddr;
int wsock;

void assert(bool truth, const char* string) {
	if (!truth)
		printf("FATAL ERROR: %s\n", string);
}

void acceptloop(tsq<int>* fdqueue, sem_t* status_sem, int* acceptflag, int sockfd, int threads) {
	//struct for storing client address (not stored)
	struct sockaddr_in caddr;
	unsigned int clen = sizeof(caddr);

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

			//store the file descriptor on the queue if it's valid
			if (cfd >= 0) {
				fdqueue->push(cfd);
			} else {
				#ifdef DEBUG_NETWORK
				printf("%d: accept error: %d\n", pid_me, cfd);
				#endif
			}
		}
	}
}

void* http_worker(void* q) {
	tsq<int>* tq = (tsq<int>*) q;

	int tid = 1;
	int got = 0;
	while (got >= 0) {
		got = tq->pop();
		#ifdef DEBUG_LOW
		printf("thread got fd %d\n", got);

		//read http request & headers
		int BUFBLOCK = 256;
		int BUFFREE = 128;
		int BUFSIZE = BUFBLOCK;
		int BUFUSED = 0;
		char* buf = (char*) malloc (BUFSIZE * sizeof(char));
		assert(buf != NULL, "could not allocate buffer");
		char* nbuf = NULL;

		bool done;
		int crlfbegin = 0;
		while (!done) {
			// check if the buffer is big enough.
			if (BUFSIZE - BUFUSED < BUFFREE) {
				int nbsize = BUFSIZE + BUFBLOCK;
				
				// it's not. make it bigger.
				nbuf = (char*) realloc(buf, sizeof(char) * nbsize);
				assert(nbuf != NULL, "could not enlarge buffer");
				buf = nbuf;
			}

			// read a bunch of data.
			int r = read(got, &buf[BUFUSED], BUFSIZE - BUFUSED);
			assert(r >= 0, "read error from pipe");

			#define CR 13
			#define LF 10
			for (int i = BUFUSED; i < r + BUFUSED; i++) {
				if (crlfmode == 0) {
					if (buf[i] = CR) {
						crlfbegin = i;
						crlfmode++;
					}
				} else if (crlfmode == 1) {
					if (buf[i] = LF
			}

			if (BUFUSED > 0) {
				buf[BUFUSED] = 0;
				printf("got some data: %s\n", buf);
			}
			BUFUSED += r;
		}
		close(got);
		#endif
	}
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
	//create read and write bound sockets
	unsigned short wport = 80;

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

	tsq<int>* rtsq = new tsq<int>();

	//threads
	pthread_t rloops[RTHREADS];
	
	for (int i = 0; i < RTHREADS; i++) {
		if (pthread_create(&rloops[i], NULL, http_worker, (void*) rtsq)) {
			die("failed to create thread");
		}
	}

	acceptloop(rtsq, NULL, NULL, wsock, RTHREADS);

	for (int i = 0; i < RTHREADS; i++) {
		pthread_join(rloops[i], NULL);
	}

	delete rtsq;
}

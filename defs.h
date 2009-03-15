#ifndef DEFS_H
#define DEFS_H

#define DEBUG_RETURN 1
//#define DEBUG_INITIALIZE 1
//#define DEBUG_FREE

#define DEBUG_LOW 1
//#define DEBUG_HIGH 1
#define DEBUG_MANAGEMENT 1
//#define DEBUG_NETWORK 1

// #define DEBUG_BUFFERS 1

struct connection_descriptor {
	char*	ipaddr;
	int	fd;
};

#endif

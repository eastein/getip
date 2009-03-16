#ifndef DEFS_H
#define DEFS_H

/*
copyright 2009 Eric Stein <eastein@wpi.edu>
Licensed under the GNU Public License version 2 or any subsequent version
published by the Free Software Foundation, at your option.  If you want to use
this code in a non-GPL application, contact me for permission.
*/

#define DEBUG_RETURN 1
//#define DEBUG_INITIALIZE 1
//#define DEBUG_FREE

#define DEBUG_LOW 1
//#define DEBUG_HIGH 1
#define DEBUG_MANAGEMENT 1
//#define DEBUG_NETWORK 1

//#define DEBUG_BUFFERS 1

struct connection_descriptor {
	char*	ipaddr;
	int	fd;
};

#endif

#ifndef TSQ_CPP
#define TSQ_CPP

#include <semaphore.h>
#include <string.h>
#include <sys/time.h>
#include <math.h>
#include <unistd.h>
#include <stdio.h>

/*
copyright 2006 Eric Stein <eastein@wpi.edu>
Licensed under the GNU Public License version 2 or any subsequent version
published by the Free Software Foundation, at your option.  If you want to use
this code in a non-GPL application, contact me for permission.

Thread-safe templated queue implemented using semaphores

Achieves fast size increase by using memcpy() - any object that needs to
be moved using operator= or similar will break - use pointers instead.  If
pointers to objects are placed in the queue, deleting the queue will not delete
the objects.

Only one operation can occur on the tsq object at one time - this produces short
	term blocking

Long term blocking operations:
* push() - if not enough spaces are left to add the element, this call will
	block until a space is available
* pop() - if nothing is in the queue, this call will block until something can
	be returned.

Revisions:
	Tue Sep 26 13:45 EDT 2006
		First working version.

	Tue Oct  3 00:05 EDT 2006
		Small efficiency improvements in semaphore code.

	Fri Dec 15 07:51 EST 2006
		Added pop() function with time-out.  Release 0.1.
*/

template<typename T>
class tsq {
	private:
		//index of the beginning of the queue
		unsigned int begin;
		//number of items in the queue currently
		unsigned int count;
		//current size of allocation
		unsigned int current;
		//maximum size of allocation (zero to unlimit)
		unsigned int maximum;

		//buffer for storing the actual queue
		T* buffer;
		//semaphores
		sem_t produce, consume, mutex;

		//initialization function called by constructors
		void initialize(unsigned int cur, unsigned int max);

		//minimum of unsigned ints
		unsigned int umin(unsigned int a, unsigned int b);
	public:
		//constructors / destructors
		tsq();
		tsq(unsigned int cur, unsigned int max);
		~tsq();

		// will block if the queue is full and has hit max
		void push(T insert);

		// blocks if data not available yet
		T pop();
		// blocks only for ms milliseconds
		T pop(int ms, bool* success);
};

template<typename T>
tsq<T>::tsq() {
	//initialize to a 32 entry queue (no limit on expansion)
	initialize(32, 0);
}

template<typename T>
tsq<T>::tsq(unsigned int cur, unsigned int max) {
	//pass on initialization instructions
	initialize(cur, max);
}

template<typename T>
void tsq<T>::initialize(unsigned int cur, unsigned int max) {
	//start populating at the start of the buffer
	begin = 0;
	//no items in queue
	count = 0;
	//cur elements in queue
	current = cur;
	//set max
	maximum = max;

	//allocate buffer
	buffer = (T*)(new char[current * sizeof(T)]);

	//plenty of spaces for producers!
	sem_init(&produce, 0, current);
	//no space for consumers yet
	sem_init(&consume, 0, 0);
	//one at a time please, watch your step
	sem_init(&mutex, 0, 1);
}

template<typename T>
unsigned int tsq<T>::umin(unsigned int a, unsigned int b) {
	if (a < b)
		return a;
	return b;
}

template<typename T>
tsq<T>::~tsq() {
	//kill semaphores
	sem_destroy(&produce);
	sem_destroy(&consume);
	sem_destroy(&mutex);

	delete[] buffer;
}

template<typename T>
void tsq<T>::push(T insert) {
	//produce is captured first because otherwise, deadlock could occur
	sem_wait(&produce);
	sem_wait(&mutex);

	buffer[(begin + count++) % current] = insert;

	//if the buffer is full
	//and the buffer is expandable
	if (count == current && (maximum == 0 || maximum > current)) {
		//then expand it - assume double size
		unsigned int newsize = current * 2;
		//if there's a limit, make sure it is respected
		if (maximum > 0)
			newsize = umin(newsize, maximum);
		//allocate the new buffer
		T* nbuf = (T*)(new unsigned char[newsize * sizeof(T)]);

		//calculate how many elements are placed before the wrap
		unsigned int fc = umin(count, current - begin);
		//copy the first part to the start of the new buffer
		memcpy(nbuf, buffer + begin, fc * sizeof(T));
		//if there was another section, copy it as well
		if (fc < count)
			memcpy(nbuf + fc, buffer, (count - fc) * sizeof(T));
		//delete old buffer
		delete[] buffer;
		//add to the available produce spaces
		for(unsigned int i = current; i < newsize; i++)
			sem_post(&produce);

		//new values
		current = newsize;
		buffer = nbuf;
	}

	//another element is available to consumers
	sem_post(&mutex);
	sem_post(&consume);
}

template<typename T>
T tsq<T>::pop() {
	bool t;
	return pop(0, &t);
}

template<typename T>
T tsq<T>::pop(int ms, bool* success) {
	//consume is captured first because otherwise, deadlock could occur.
	int status;
	if (ms == 0) {
		status = sem_wait(&consume);
	} else {
		//current time
		timeval t;
		gettimeofday(&t, NULL);

		//build timeout time
		//total nanoseconds from epoch
		long long tns = t.tv_sec * 1000000000 + t.tv_usec * 1000 + (long)ms * 1000000;
		timespec wt;
		//make timespec
		wt.tv_sec = tns / 1000000000;
		wt.tv_nsec = tns % 1000000000;
		//wait
		status = sem_timedwait(&consume, &wt);
	}

	//if we got anything, do the work.
	if (status == 0) {
		sem_wait(&mutex);

		//retrieve current
		T r = buffer[begin];
		//increment pointer and wrap if needed
		begin = (begin + 1) % current;
		//one less item, now
		count--;

		//another space is available to producers
		sem_post(&mutex);
		sem_post(&produce);
		*success = true;
		return r;
	} else {
		*success = false;
	}
}

#endif

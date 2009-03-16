FLAGS=-g -O2
LIBS= -lpthread
CC= g++

DOTO = ${CC} -c ${FLAGS} 
LINK = ${CC} ${FLAGS} ${LIBS}

getip: main.o rw.o
	${LINK} main.o rw.o -o getip

clean:
	rm -f getip *.o *~

main.o: main.c tsq.cpp defs.h rw.h
	${DOTO} main.c

rw.o: rw.c rw.h
	${DOTO} rw.c

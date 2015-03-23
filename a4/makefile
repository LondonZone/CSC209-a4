CC = gcc
PORT= 31375
CFLAGS= -DPORT=\$(PORT) -g -Wall


# The variable $@ has the value of the target. In this case $@ = pusage
all: battleserver  

battleserver: battleserver.o
	${CC} ${CFLAGS} -o $@ battleserver.o

%.o: %.c
	${CC} ${CFLAGS}  -c $<

clean:
	rm *.o battleserver battle 

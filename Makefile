CC=cc
CFLAGS=-Wall -g
INCLUDES=-I/usr/X11R6/include -I/usr/include/X11
LIBS=-L/usr/X11R6/lib -lX11

PROG=x-agent
OBJS=x-agent.o

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) -o x-agent $(OBJS) $(LIBS)

x-agent.o: x-agent.c
	$(CC) $(CFLAGS) -c -o x-agent.o x-agent.c $(INCLUDES)

.PHONY: clean

clean:
	rm -f $(OBJS) $(PROG) *~

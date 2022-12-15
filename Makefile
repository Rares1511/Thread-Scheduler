CC = gcc
CFLAGS = -g -Wall

build: libscheduler.so

libscheduler.so: so_scheduler.c so_scheduler.h
	$(CC) $(CFLAGS) -c $< -o $@
	mv libscheduler.so ../checker-lin/
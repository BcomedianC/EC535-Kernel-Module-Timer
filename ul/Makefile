# Sam Beaulieu
# EC 535, Spring 2016
# Lab 2, 2/24/16
# Makefile for kernal module user level interface ktimer

ktimer : ktimer.o
	arm-linux-gcc -o ktimer ktimer.o

ktimer.o : ktimer.c
	arm-linux-gcc -c ktimer.c

clean :
	rm ktimer ktimer.o

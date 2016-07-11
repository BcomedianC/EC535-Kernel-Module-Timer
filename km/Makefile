# Sam Beaulieu
# EC 535, Spring 2016
# Lab 2, 2/24/16
# Makefile for kernel module mytimer

ifneq ($(KERNELRELEASE),)
	obj-m := mytimer.o
else
	KERNELDIR := $(EC535)/gumstix/linux-2.6.21gum/
	PWD := $(shell pwd)
	ARCH := arm
	CROSS := arm-linux-

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS) modules

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) clean

endif

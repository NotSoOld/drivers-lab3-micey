ifneq ($(KERNELRELEASE),)
obj-m:=micey.o
else
KDIR:=/home/notsoold-laptop-vm/Downloads/linux-4.13.3

all:
	$(MAKE) -C $(KDIR) M=$$PWD
endif

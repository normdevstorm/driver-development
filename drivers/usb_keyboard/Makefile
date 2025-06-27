obj-m += usb_keyboard.o

# Kernel build directory (adjust for your CentOS 32-bit system)
KERNEL_DIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

install:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules_install

load:
	sudo insmod usb_keyboard.ko

unload:
	sudo rmmod usb_keyboard

reload: unload load

.PHONY: all clean install load unload reload

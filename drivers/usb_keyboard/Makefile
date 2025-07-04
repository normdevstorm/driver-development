obj-m += usb_keyboard.o

# Kernel build directory for CentOS 9 64-bit
KERNEL_DIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# Compiler flags for modern kernel
EXTRA_CFLAGS += -DCONFIG_64BIT -Wno-unused-variable -Wno-unused-function

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

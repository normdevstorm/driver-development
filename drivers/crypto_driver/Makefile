obj-m += crypto_driver.o

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
	sudo insmod crypto_driver.ko
	sudo chmod 666 /dev/crypto_dev

unload:
	sudo rmmod crypto_driver

reload: unload load

.PHONY: all clean install load unload reload

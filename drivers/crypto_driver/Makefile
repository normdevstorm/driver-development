obj-m += crypto_driver.o

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
	sudo insmod crypto_driver.ko
	sudo chmod 666 /dev/crypto_dev

unload:
	sudo rmmod crypto_driver

reload: unload load

.PHONY: all clean install load unload reload

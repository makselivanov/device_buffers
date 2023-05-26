KERNELRELEASE ?= $(shell uname -r)
KERNEL_DIR  ?= /lib/modules/$(KERNELRELEASE)/build
# KERNEL_DIR ?= /lib/modules/$(KERNELRELEASE)
PWD := $(shell pwd)

obj-m := buffer_loader.o

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

install:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules_install

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean

insmod:
	sudo insmod module_name.ko

rmmod:
	sudo rmmod module_name.ko

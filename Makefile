obj-m += src/ring-chrdev.o

ccflags-y := -Wall -Werror

KDIR := /lib/modules/$(shell uname -r)/build

DEBUG ?= 1
ifeq ($(DEBUG), 1)
	ccflags-y += -DDEBUG
endif

src/ring-chrdev.ko: src/ring-chrdev.c include/ring_ioctl.h
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

.PHONY: all
all: src/ring-chrdev.ko

.PHONY: clean
clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

.PHONY: test
test: src/ring-chrdev.ko
	# Using sudo rather than requiring `sudo make test` such that test
	# functions can be `kill`ed by the user running the test.
	sudo rmmod src/ring-chrdev.ko || true
	sudo insmod src/ring-chrdev.ko
	test/run_tests.sh

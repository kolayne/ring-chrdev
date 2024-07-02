obj-m += src/ring-chrdev.o

ccflags-y := -Wall -Werror

KDIR := /lib/modules/$(shell uname -r)/build

DEBUG ?= 1
ifeq ($(DEBUG), 1)
	ccflags-y += -DDEBUG
endif

src/ring-chrdev.ko: src/ring-chrdev.c
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

.PHONY: all
all: src/ring-chrdev.ko

.PHONY: clean
clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

.PHONY: test
test: all
	rmmod ring-chrdev.ko || true
	insmod ring-chrdev.ko
	test/run_tests.sh

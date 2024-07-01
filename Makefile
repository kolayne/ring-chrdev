obj-m += ring-chrdev.o

ccflags-y := -Wall -Werror

KDIR := /lib/modules/$(shell uname -r)/build

DEBUG ?= 1
ifeq ($(DEBUG), 1)
	ccflags-y += -DDEBUG
endif

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

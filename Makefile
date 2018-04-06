obj-m += input_module.o
obj-m += output_module.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules

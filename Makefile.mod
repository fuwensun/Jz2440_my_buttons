KERN_DIR = ~/jz2440/system/linux-2.6.22.6

all:
	make -C $(KERN_DIR) M=`pwd` modules

clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order

obj-m	+= s3c24xx_buttons.o
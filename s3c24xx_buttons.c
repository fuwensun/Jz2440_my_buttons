#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <asm/arch/regs-gpio.h>
#include <asm/hardware.h>

#define DEVICE_NAME "buttons"
#define BUTTON_MAJOR 232

struct button_irq_desc{
	int irq;
	unsigned long flags;
	char *name;
};
/*用来指定按键所用的外部中断引脚及中断触发方式，名字*/
static struct button_irq_desc button_irqs[] = {
	{IRQ_EINT19,IRQF_TRIGGER_FALLING,"KEY1"},
	{IRQ_ENIT11,IRQF_TRIGGER_FALLING,"KEY2"},
	{IRQ_EINT2, IRQF_TRIGGER_FALLING,"KEY3"},
	{IRQ_EINT0, IRQF_TRIGGER_FALLING,"KEY4"},
}

/*按键按下的次数（发生中断的次数）*/
static volatile int press_cnt[] = {0, 0, 0, 0};
/* 等待队列：
 * 当没有按键被按下时，如果由进程调用s3c24xx_buttons_read函数，
 * 它将休眠
 */
static DECLARE_WAIT_QUEUE_HEADARE(button_waitq);

/*中断时件标志，中断服务程序将它置1,s3c24xx_buttons_read将它清0*/
static volatile int ev_press = 0;

static irqreturn_t buttons_interrupt(int irq, void *dev_id)
{
	volatile int *press_cnt = (volatile int *)dev_id;
	*press_cnt = *press_cnt + 1;/*按键计数加1*/
	ev_press = 1;		/*表示中断发生了*/
	wake_up_interruptible(&button_waitq); /*唤醒休眠中的进程*/
	return IRQ_RETAL(IRQ_HANDLED);
}

/* 应用程序对设备文件/dev/buttons执行open(...)时，
 * 就会调用s3c24xx_button_open函数
 */

static int s3c24xx_buttons_open(struct inode *inode, struct file *file)
{
	int i;
	int err;
	for(i = 0; i < sizeof(button_irqs)/sizeof(button_irq[0]; i++)){
		
		//注册中断函数
		err = request_irq(button_irqs[i].irq, buttons_interrupt,button_irq[i].flags,
				button_irqs[i].name, (void)&press_cnt[i]);
		if(err){
			break;
		}
	}

	if(err){
		//释放已经注册的中断
		i--;
		for(; i >= 0; i--)
			free_irq(button_irqs[i].irq, (void *)&press_cnt[i]);
		return -EBUSY;
		
	}
	
}

/* 应用程序对设备文件 /dev/buttons执行close(...)时，
 * 就会调用s3c24xx_buttons_close函数
 */

static int s3c24xx_buttons_close(struct inode *inode, struct file *file)
{
	int i;
	for(i = 0; i < sizeof(button_irqs)/sizeof(button_irqs[0]); i++)
		free_irq(button_irqs[i].irq, (void *)&press_cnt[i]);
	return 0;
}

/* 应用程序对设备文件/dev/buttons执行read(...)时，
 * 就会调用s3c24xx_buttons_read函数
 */
static int s3c24xx_buttons_read(struct file *filp, char __user *buff, size_t count, loff_t *offp)
{
	unsigned long err;

	/*如果ev_press等于0，休眠*/
	wait_event_interruptible(button_waitq, ev_press);

	/*执行到这里时，ev_press等于1,将它清0*/
	ev_press = 0;

	/*将按键状态复制给用户，并清0*/
	err = copy_to_user(buff, (const void *)press_cnt, min(sizeof(press_cnt),count));
	memset((void *)press_cnt, 0, sizeof(press_cnt));
	return err ? -EFAULT : 0;
}

static struct file_operations s3c24xx_buttons_fops = {
	.owner  = THIS_MODULE,/*这时一个宏，指向编译模块时自动创建的__this_module变量*/
	.open  =  s3c24xx_buttons_open,
	.release = s3c24xx_buttons_close,
	.read 	= s3c24xx_buttons_read,

};

/*
 * 执行insmod s3c24xx_buttons.ko时调用这个函数
 */
static int __init s3c24xx_buttons_init(void)
{
	int ret;
/* 注册字符设备驱动程序
 * 参数为主设备号，设备名字，file_operations结构;
 * 这样，主设备号就和具体的file_operations结构联系起来了，
 * 操作主设备为BUTTON_MAJOR的设备文件时，就会调用s3c24xx_buttons_fops中的相关成员函数
 * BUTTON_MAJOR可以设为0,表示由内核自动分配主设备号
 */
	ret = register_chrdev(BUTTON_MAJOR, DEVICE_NAME, &s3c24xx_buttons_fops);
	if(ret < 0){
		printk(DEVICE_NAME "can't register major number\n");
		return ret;
	}

	printk(DEVICE_NAME "initialized\n");
	return 0;
}

/*
 * 执行rmmod s3c24xx_buttons.ko时调用
 */
static void __exit s3c24xx_buttons_exit(void)
{
	/**/
	unregister_chrdev(BUTTON_MAJOR,DEVICE_NAME);
}

module_inti(s3c24xx_buttons_init);
module_exit(s3c24xx_buttons_exit);

MODULE_AUTHOR("http://www.100ask.net");
MODULE_DESCIPTION("S3C2410/S3C2440 BUTTON Driver");
MODULE_LICENSE("GPL");

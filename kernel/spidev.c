#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/spi/spi.h>
#include <linux/types.h> 
#include <linux/cdev.h> 
#include <linux/fs_struct.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <asm/uaccess.h>

#define ADSSPI_MODNAME "adsspi"
#define ADSSPI_BUS_ID 1
#define ADSSPI_CS_ID 0
#define QCASPI_BUS_MODE (SPI_CPOL | SPI_CPHA)
#define SPIADC_DEVICE_NUMBER 123
#define SPIADC_AIN_COUNT 4

#define ADCSPI_MAX_SPEED 50000000
//#define ADCSPI_SPEED 16000000
#define ADCSPI_SPEED 18000000
#define SPIADC_VERSION "0.0.1"

#define ADSSPI_CLASSNAME "ads_spi_driver"

static struct mutex s_ads_mutex;

static struct spi_board_info adc_spi_board_info __initdata = {
	.modalias = ADSSPI_MODNAME,
	.max_speed_hz = ADCSPI_MAX_SPEED,
	.bus_num = ADSSPI_BUS_ID,
	.chip_select = ADSSPI_CS_ID,
	.mode = SPI_MODE_1
};

struct spiadc_dev{
	struct cdev cdev;
	int ad_num;
};

static struct spiadc_dev s_spiadc_dev[SPIADC_AIN_COUNT];
static int s_spiadc_dev_major = SPIADC_DEVICE_NUMBER;
static struct class * spiadc_class;
struct spi_device *s_spi_dev;

static uint16_t s_ad_value[ADS_MAX_READ_SIZE];

static struct spi_transfer s_transfer[ADS_MAX_READ_SIZE];

uint16_t
read_channel_data(struct spi_device* spidev,uint16_t channel,int count,uint16_t* result){
	uint16_t id = channel;
	struct spi_message msg;
	int i = 0;

	memset(s_transfer, 0, sizeof(s_transfer));
	spi_message_init(&msg);

	for(i = 0 ; i < count ;i++){
		s_transfer[ i*2 ].tx_buf = &id;
		s_transfer[ i*2 ].len = sizeof(id);
		s_transfer[ i*2+1 ].rx_buf = &result[i];
		s_transfer[ i*2+1 ].len = sizeof(result[i]);
	
		spi_message_add_tail(&s_transfer[i * 2], &msg);
		spi_message_add_tail(&s_transfer[i * 2 + 1], &msg);
	}

	spi_sync(spidev,&msg);
	return 0;
}


static ssize_t 
spiadc_read(struct file *filp, char __user *buf, size_t sz, loff_t *off){
	int size = sz;
	int channel = 0;
	int copy_cnt = 0;
	struct spiadc_dev* sdev = (struct spiadc_dev*)filp->private_data;
	int i = 0;

	memset(s_ad_value,0,sizeof(s_ad_value));


	if(size > 256){
		size = 256;
	}

	switch(sdev->ad_num){
		case 0:
			channel = ADS_CMD_REG_MAN_Ch_0;
			break;
		case 1:
			channel = ADS_CMD_REG_MAN_Ch_1;
			break;
		case 2:
			channel = ADS_CMD_REG_MAN_Ch_2;
			break;
		case 3:
			channel = ADS_CMD_REG_MAN_Ch_3;
			break;
		default:
			channel = ADS_CMD_REG_MAN_Ch_0;
			break;
	}

	mutex_lock(&s_ads_mutex);
	read_channel_data(s_spi_dev,channel,size,s_ad_value);
	mutex_unlock(&s_ads_mutex);

	copy_cnt = copy_to_user(buf,s_ad_value,size*sizeof(uint16_t));
	if(copy_cnt == size * sizeof(uint16_t))
	  return  -EFAULT;

	return size * 2 - copy_cnt;
}

static ssize_t 
spiadc_write (struct file *filp, const char __user *buf,size_t count, loff_t *ppos) {
	return 0;
}

static ssize_t 
spiadc_open(struct inode *inode, struct file *filp){
	struct spiadc_dev* sdev = container_of(inode->i_cdev,struct spiadc_dev,cdev); 
	nonseekable_open(inode, filp);

	filp->private_data = sdev;
	return 0;
}

long
spiadc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
	return 0;
}

static unsigned int 
spiadc_poll(struct file *filp, struct poll_table_struct *poll){
	return 0;
}

static int 
spiadc_release(struct inode *inode, struct file *filp){
	return 0;
}


static const struct file_operations spiadc_fops = {
	.owner = THIS_MODULE,
	.read = spiadc_read,
	.write = spiadc_write,
	.open = spiadc_open,
	.unlocked_ioctl = spiadc_ioctl,
	.release = spiadc_release,
	.poll = spiadc_poll,
};

static int
setup_spiadc_dev(struct spiadc_dev *sdev,int num){
	int ret = 0;
	cdev_init(&sdev->cdev, &spiadc_fops);
	sdev->cdev.owner = THIS_MODULE;
	sdev->ad_num = num;

	ret = cdev_add(&sdev->cdev, MKDEV(s_spiadc_dev_major,num), 1);
	if(ret != 0){
	  return -1;
	}
	return 0;
}

static int __init
spiadc_mod_init(void){
	struct spi_master *spi_master = NULL;
	int i = 0;
	int j = 0;

	mutex_init(&s_ads_mutex);
	spi_master = spi_busnum_to_master(ADSSPI_BUS_ID);
	if (!spi_master) {
		printk("adcspi master failed\n");
		return -EINVAL;
	}

	printk("[%s %d] master->transfer_one_message : %pS\n, transfer_one: %pS\n",
		__FILE__,__LINE__,spi_master->transfer_one_message,
		spi_master->transfer_one);

	s_spi_dev = spi_new_device(spi_master, &adc_spi_board_info);

	//s_spi_dev->bits_per_word = 16;
	s_spi_dev->bits_per_word = 16;
	s_spi_dev->max_speed_hz = ADCSPI_SPEED;
	if(spi_setup(s_spi_dev) < 0){
		printk("[%s %d]failed\n",__FILE__,__LINE__);
		goto free_spi;
	}
	
	memset(s_spiadc_dev,0,sizeof(s_spiadc_dev));

	spiadc_class = class_create(THIS_MODULE,ADSSPI_CLASSNAME);
	if(!spiadc_class){
		printk("[%s %d]failed\n",__FILE__,__LINE__);
		goto free_spi;
	}

	for(i = 0 ; i < SPIADC_AIN_COUNT ; i++){
		if(setup_spiadc_dev(&s_spiadc_dev[i],i)){
			printk("[%s %d]failed\n",__FILE__,__LINE__);
			goto free_cdev;
		}
		if(NULL == device_create(spiadc_class, NULL, MKDEV(s_spiadc_dev_major, i), NULL, "adsspi%d",i)){
			printk("[%s %d]failed, index %d\n",__FILE__,__LINE__,i);
			goto free_class_device;
		}
	}

	return 0;

free_class_device:
	cdev_del(&s_spiadc_dev[i].cdev);
free_cdev:
	for(j = 0 ; j < i;j++){
		cdev_del(&s_spiadc_dev[j].cdev);
		device_destroy(spiadc_class,MKDEV(s_spiadc_dev_major, j));
	}
	class_destroy(spiadc_class);
free_spi:
	spi_unregister_device(s_spi_dev);
	
	return -EINVAL;
}

static void __exit
spiadc_mod_exit(void){
	int i = 0;

	for(i = 0 ; i < SPIADC_AIN_COUNT;i++){
		device_destroy(spiadc_class,MKDEV(s_spiadc_dev_major, i));
	}
	class_destroy(spiadc_class);
	spi_unregister_device(s_spi_dev);

	for(i = 0 ; i < SPIADC_AIN_COUNT;i++){
		cdev_del(&s_spiadc_dev[i].cdev);
	}
}

module_init(spiadc_mod_init);
module_exit(spiadc_mod_exit);

MODULE_DESCRIPTION("SPI ADC DRIVER");
MODULE_AUTHOR("light");
MODULE_VERSION(SPIADC_VERSION);
MODULE_LICENSE("Dual BSD/GPL");

#include <stdio.h>
#include <termios.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <linux/netlink.h>
#include <unistd.h>
#include <errno.h>

static char tty_name[128] = "/dev/ttyAMA0";

static struct termios oldterm;
static struct termios newterm;
static int tty_fd = 0;


void
init_termios(struct termios* termios){
	if(!termios){
		printf("invalid argument\n");
		exit(-1);
	}

	memset(termios,0,sizeof(struct termios));

	termios->c_cflag = B9600 | CS8 | CREAD | CLOCAL;

	termios->c_iflag = IGNPAR;//忽略校验和错误
	termios->c_cflag &= ~PARENB;//没有奇偶校验
	termios->c_iflag &= ~INPCK;//不允许奇偶校验
	termios->c_cflag &= ~CSTOPB; //只有一个停止位

	//termios->c_oflag = 0;
	//termios->c_lflag = 0;

	//termios->c_cc[VTIME] = 200;
}

static int
open_tty(){
	tty_fd = open(tty_name,O_RDWR| O_NOCTTY | O_NDELAY);	
	if(tty_fd < 0 ){
		printf("Error: can't open device %s\n",tty_name);
		exit(-1);
	}

	tcgetattr(tty_fd,&oldterm);
	init_termios(&newterm);

	tcflush(tty_fd,TCIOFLUSH);
	tcsetattr(tty_fd,TCSANOW,&newterm); 
	return 0;
}

static int
close_tty(){
	if(0 != tty_fd){
		tcsetattr(tty_fd,TCSANOW,&oldterm); 
		close(tty_fd);
	}
	return 0;
}

int 
main(int argc,char* argv[]){
	unsigned char send_cmd[3] = {0};
	unsigned char recv_cmd[3] = {0};
	int ret = 0;
	open_tty();

	send_cmd[0] = 0xFA;
	send_cmd[1] = 0x5;
	//send_cmd[0] = 0x3;
	//send_cmd[1] = 0x0;
	//send_cmd[2] = 0x0;
	send_cmd[2] = send_cmd[0] ^ send_cmd[1];

	printf("a %x b %x c %x\n",send_cmd[0],send_cmd[1],send_cmd[2]);
	
	write(tty_fd,send_cmd,3);
	while(ret <= 0){
		ret = read(tty_fd,recv_cmd,3);
	}
	
	printf("ret %d,prefix %x flag %x last %x\n",ret,recv_cmd[0],recv_cmd[1],recv_cmd[2]);
	return 0;
}




#include <linux/time.h>
#include <linux/timex.h>
#include <linux/rtc.h>

#define myprintk(format, args...) \
    do{\
        print_time();\
        printk("[%s %d] " format ,__FILE__,__LINE__,##args);\
    }while(0)

void
print_time(void){
	struct timex  txc; 
	struct rtc_time tm;

	do_gettimeofday(&(txc.time));

	txc.time.tv_sec -= sys_tz.tz_minuteswest * 60;
	rtc_time_to_tm(txc.time.tv_sec,&tm);

	printk("%04d-%02d-%02d %02d:%02d:%02d ",
		tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,
		tm.tm_hour,tm.tm_min,tm.tm_sec);

}


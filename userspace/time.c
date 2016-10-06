struct timespec tmptime = {0};
clock_gettime(CLOCK_MONOTONIC, &now_time);

//获取当前的时间
static inline void
get_now_string(char* time_msg){
	struct timeval tv; 
	time_t now;
	struct tm* tmnow;

	gettimeofday(&tv, NULL);
	time(&now);
	tmnow = localtime(&now);
	sprintf(time_msg,"%04d-%02d-%02d %02d:%02d:%02d.%03ld", 
				tmnow->tm_year + 1900,tmnow->tm_mon+1,tmnow->tm_mday,
				tmnow->tm_hour, tmnow->tm_min,
				tmnow->tm_sec, tv.tv_usec/1000);
}

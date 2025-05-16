#include <stdio.h>
#include <stdlib.h>
#include <time.h>       // 基本时间函数
#include <sys/time.h>   // 时间结构体和gettimeofday
#include <sys/times.h>  // 进程时间
#include <unistd.h>     // sleep等函数
#include <sys/timex.h>  // adjtimex
#include <string.h>     // strftime

// 打印时间结构体内容的辅助函数
void print_tm(const struct tm *timeptr) {
    printf("  tm_sec: %d\n", timeptr->tm_sec);    // 秒 [0-60] (允许闰秒)
    printf("  tm_min: %d\n", timeptr->tm_min);    // 分 [0-59]
    printf("  tm_hour: %d\n", timeptr->tm_hour);  // 时 [0-23]
    printf("  tm_mday: %d\n", timeptr->tm_mday);  // 日 [1-31]
    printf("  tm_mon: %d\n", timeptr->tm_mon);    // 月 [0-11]
    printf("  tm_year: %d\n", timeptr->tm_year);  // 年 - 1900
    printf("  tm_wday: %d\n", timeptr->tm_wday);  // 周几 [0-6], 0=周日
    printf("  tm_yday: %d\n", timeptr->tm_yday);  // 一年中的第几天 [0-365]
    printf("  tm_isdst: %d\n", timeptr->tm_isdst);// 夏令时标志
}

int main() {
    // ==================== 1. 基本时间函数 ====================
    
    // time_t - 从Epoch(1970-01-01 00:00:00 UTC)开始的秒数
    time_t current_time;
    time(&current_time);  // 获取当前时间(秒级精度)
    printf("1. time() 当前时间戳: %ld\n", current_time);
    
    // ctime - 将time_t转换为可读字符串
    printf("   ctime() 格式: %s", ctime(&current_time));
    
    // ==================== 2. 更精确的时间获取 ====================
    
    // gettimeofday - 微秒级精度
    struct timeval tv;
    gettimeofday(&tv, NULL);
    printf("\n2. gettimeofday():\n");
    printf("   秒: %ld\n", tv.tv_sec);
    printf("   微秒: %ld\n", tv.tv_usec);
    
    // clock_gettime - 纳秒级精度，支持多种时钟
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);  // 系统实时时间
    printf("\n3. clock_gettime(CLOCK_REALTIME):\n");
    printf("   秒: %ld\n", ts.tv_sec);
    printf("   纳秒: %ld\n", ts.tv_nsec);
    
    clock_gettime(CLOCK_MONOTONIC, &ts); // 单调时间(不受系统时间更改影响)
    printf("\n4. clock_gettime(CLOCK_MONOTONIC):\n");
    printf("   秒: %ld\n", ts.tv_sec);
    printf("   纳秒: %ld\n", ts.tv_nsec);
    
    // ==================== 3. 时间转换函数 ====================
    
    // localtime - 将time_t转换为本地时间结构体
    struct tm *local_tm = localtime(&current_time);
    printf("\n5. localtime() 本地时间结构体:\n");
    print_tm(local_tm);
    
    // gmtime - 将time_t转换为UTC时间结构体
    struct tm *utc_tm = gmtime(&current_time);
    printf("\n6. gmtime() UTC时间结构体:\n");
    print_tm(utc_tm);
    
    // mktime - 将本地时间结构体转换为time_t(考虑时区和夏令时)
    time_t converted_time = mktime(local_tm);
    printf("\n7. mktime() 转换回的时间戳: %ld\n", converted_time);
    
    // ==================== 4. 时间格式化 ====================
    
    // strftime - 自定义时间格式
    char time_str[100];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S %Z", local_tm);
    printf("\n8. strftime() 自定义格式: %s\n", time_str);
    
    // ==================== 5. 进程时间 ====================
    
    // times - 获取进程时间信息
    struct tms process_times;
    clock_t clock_ticks = times(&process_times);
    printf("\n9. times() 进程时间信息:\n");
    printf("   用户CPU时间: %ld ticks\n", process_times.tms_utime);
    printf("   系统CPU时间: %ld ticks\n", process_times.tms_stime);
    printf("   子进程用户CPU时间: %ld ticks\n", process_times.tms_cutime);
    printf("   子进程系统CPU时间: %ld ticks\n", process_times.tms_cstime);
    printf("   从系统启动到现在的时钟滴答数: %ld\n", clock_ticks);
    
    // clock - 获取进程使用的CPU时间
    clock_t cpu_time = clock();
    printf("\n10. clock() 进程使用的CPU时间: %ld (%.2f秒)\n", 
           cpu_time, (double)cpu_time/CLOCKS_PER_SEC);
    
    // ==================== 6. 时间调整 ====================
    
    // adjtime - 渐进式调整系统时间
    struct timeval delta;
    delta.tv_sec = 0;
    delta.tv_usec = 500000; // 调整0.5秒
    struct timeval olddelta;
    printf("\n11. adjtime() 尝试调整系统时间0.5秒...\n");
    if (adjtime(&delta, &olddelta)) {
        perror("adjtime失败");
    } else {
        printf("   之前未完成的时间调整: %ld秒 %ld微秒\n", 
               olddelta.tv_sec, olddelta.tv_usec);
    }
    
    // ==================== 7. 高精度睡眠 ====================
    
    // nanosleep - 高精度睡眠
    struct timespec sleep_time;
    sleep_time.tv_sec = 1;
    sleep_time.tv_nsec = 500000000; // 1.5秒
    printf("\n12. nanosleep() 睡眠1.5秒...\n");
    if (nanosleep(&sleep_time, NULL)) {
        perror("nanosleep被中断");
    }
    
    // usleep - 微秒级睡眠(已废弃，推荐用nanosleep)
    printf("13. usleep() 睡眠0.5秒...\n");
    usleep(500000);
    
    // ==================== 8. 系统时钟信息 ====================
    
    // sysconf - 获取时钟滴答频率
    long clock_ticks_per_sec = sysconf(_SC_CLK_TCK);
    printf("\n14. sysconf(_SC_CLK_TCK) 每秒钟时钟滴答数: %ld\n", clock_ticks_per_sec);
    
#if 0
    // ==================== 9. 时区处理 ====================
    
    // tzset - 设置时区信息(从TZ环境变量)
    tzset();
    printf("\n15. tzset() 时区信息:\n");
    printf("   时区名称: %s\n", tzname[0]);  // 标准时间名称
    printf("   夏令时名称: %s\n", tzname[1]); // 夏令时名称
    printf("   与UTC的时差(秒): %ld\n", timezone);
    printf("   夏令时调整(秒): %d\n", daylight);
#endif
    
    return 0;
}

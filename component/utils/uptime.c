#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/sysinfo.h>  // 用于sysinfo()函数
#include <unistd.h>       // 用于sleep()函数

// 获取系统启动时间并格式化为可读字符串
char* get_system_uptime() {
    static char uptime_str[100];
    struct sysinfo info;
    
    // 获取系统信息
    if (sysinfo(&info) != 0) {
        perror("sysinfo");
        return "Error getting uptime";
    }
    
    // 计算天、小时、分钟和秒
    long uptime = info.uptime;
    int days = uptime / (60 * 60 * 24);
    int hours = (uptime % (60 * 60 * 24)) / (60 * 60);
    int minutes = (uptime % (60 * 60)) / 60;
    int seconds = uptime % 60;
    
    // 格式化输出字符串
    if (days > 0) {
        snprintf(uptime_str, sizeof(uptime_str), 
                "%d day%s, %02d:%02d:%02d",
                days, (days > 1) ? "s" : "", 
                hours, minutes, seconds);
    } else {
        snprintf(uptime_str, sizeof(uptime_str), 
                "%02d:%02d:%02d", 
                hours, minutes, seconds);
    }
    
    return uptime_str;
}

// 获取系统启动时间点（日历时间）
char* get_boot_time() {
    static char boottime_str[100];
    struct sysinfo info;
    time_t boot_time;
    
    if (sysinfo(&info) != 0) {
        perror("sysinfo");
        return "Error getting boot time";
    }
    
    // 计算启动时间（当前时间 - 运行时间）
    time_t now = time(NULL);
    boot_time = now - info.uptime;
    
    // 转换为本地时间字符串
    struct tm *btm = localtime(&boot_time);
    strftime(boottime_str, sizeof(boottime_str), 
            "%Y-%m-%d %H:%M:%S", btm);
    
    return boottime_str;
}

int main() {
    printf("系统启动时间信息:\n");
    
    // 获取并显示系统运行时间
    printf("1. 系统已运行时间: %s\n", get_system_uptime());
    
    // 获取并显示系统启动的具体时间
    printf("2. 系统启动时间: %s\n", get_boot_time());
    
    // 使用/proc/uptime获取更精确的时间（精确到0.01秒）
    FILE *fp = fopen("/proc/uptime", "r");
    if (fp) {
        double uptime_seconds, idle_seconds;
        fscanf(fp, "%lf %lf", &uptime_seconds, &idle_seconds);
        fclose(fp);
        
        printf("3. /proc/uptime 信息:\n");
        printf("   精确运行时间: %.2f 秒\n", uptime_seconds);
        printf("   总空闲时间: %.2f 秒\n", idle_seconds);
    } else {
        perror("无法打开/proc/uptime");
    }
    
    return 0;
}

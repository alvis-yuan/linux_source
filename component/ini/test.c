#include <stdio.h>
#include "ini_parser.h"

int main() {
    IniFile *file;
    IniErrorCode ret;
    int value;
    bool flag;
    char str[128];

    /* 创建INI文件对象 */
    file = ini_file_create();
    if (!file) {
        fprintf(stderr, "Failed to create INI file object\n");
        return 1;
    }

    /* 加载INI文件 */
    ret = ini_file_load(file, "config.ini");
    if (ret != INI_SUCCESS && ret != INI_ERR_FILE_OPEN) {
        fprintf(stderr, "Failed to load INI file: %s\n", ini_error_string(ret));
        ini_file_destroy(file);
        return 1;
    }

    /* 读取配置 */
    ret = ini_file_get_int(file, "network", "port", &value);
    if (ret == INI_SUCCESS) {
        printf("Network port: %d\n", value);
    } else if (ret == INI_ERR_NOT_FOUND) {
        /* 如果不存在则设置默认值 */
        printf("Using default port\n");
        ini_file_set_int(file, "network", "port", 8080);
    } else {
        fprintf(stderr, "Error reading port: %s\n", ini_error_string(ret));
    }

    /* 读取布尔值 */
    ret = ini_file_get_bool(file, "settings", "debug", &flag);
    if (ret == INI_SUCCESS) {
        printf("Debug mode: %s\n", flag ? "enabled" : "disabled");
    } else if (ret == INI_ERR_NOT_FOUND) {
        printf("Debug mode not specified, defaulting to disabled\n");
        ini_file_set_bool(file, "settings", "debug", false);
    } else {
        fprintf(stderr, "Error reading debug flag: %s\n", ini_error_string(ret));
    }

    /* 读取字符串 */
    ret = ini_file_get_string(file, "database", "host", str, sizeof(str));
    if (ret == INI_SUCCESS) {
        printf("Database host: %s\n", str);
    } else if (ret == INI_ERR_NOT_FOUND) {
        printf("Using default database host\n");
        ini_file_set_string(file, "database", "host", "localhost");
    } else {
        fprintf(stderr, "Error reading database host: %s\n", ini_error_string(ret));
    }

    /* 保存修改 */
    ret = ini_file_save(file, "config.ini");
    if (ret != INI_SUCCESS) {
        fprintf(stderr, "Failed to save INI file: %s\n", ini_error_string(ret));
    }

    /* 清理 */
    ini_file_destroy(file);
    return 0;
}
/**
 * @file ini_parser.h
 * @brief INI配置文件解析器头文件
 */

 #ifndef _LINUX_INI_PARSER_H
 #define _LINUX_INI_PARSER_H
 
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <pthread.h>
 #include <stdbool.h>
 
 /**
  * @def INI_MAX_LINE_LEN
  * @brief 单行最大长度
  */
 #define INI_MAX_LINE_LEN 1024
 
 /**
  * @def INI_MAX_SECTION_LEN
  * @brief 节名最大长度
  */
 #define INI_MAX_SECTION_LEN 128
 
 /**
  * @def INI_MAX_KEY_LEN
  * @brief 键名最大长度
  */
 #define INI_MAX_KEY_LEN 128
 
 /**
  * @def INI_MAX_VALUE_LEN
  * @brief 值最大长度
  */
 #define INI_MAX_VALUE_LEN 512
 
 /**
  * @enum IniErrorCode
  * @brief 错误码定义
  */
 typedef enum {
	 INI_SUCCESS = 0,        /**< 成功 */
	 INI_ERR_MEMORY,         /**< 内存分配失败 */
	 INI_ERR_FILE_OPEN,      /**< 文件打开失败 */
	 INI_ERR_FILE_READ,      /**< 文件读取失败 */
	 INI_ERR_FILE_WRITE,     /**< 文件写入失败 */
	 INI_ERR_INVALID_FORMAT, /**< 格式无效 */
	 INI_ERR_SECTION_EXISTS, /**< 节已存在 */
	 INI_ERR_KEY_EXISTS,     /**< 键已存在 */
	 INI_ERR_NOT_FOUND,      /**< 未找到 */
	 INI_ERR_NULL_PTR,       /**< 空指针 */
	 INI_ERR_INTERNAL        /**< 内部错误 */
 } IniErrorCode;
 
 /**
  * @struct IniEntry
  * @brief INI条目结构
  */
 typedef struct IniEntry {
	 char key[INI_MAX_KEY_LEN];      /**< 键名 */
	 char value[INI_MAX_VALUE_LEN];  /**< 键值 */
	 struct IniEntry *next;          /**< 下一个条目 */
 } IniEntry;
 
 /**
  * @struct IniSection
  * @brief INI节结构
  */
 typedef struct IniSection {
	 char name[INI_MAX_SECTION_LEN]; /**< 节名 */
	 IniEntry *entries;              /**< 条目链表 */
	 struct IniSection *next;        /**< 下一个节 */
 } IniSection;
 
 /**
  * @struct IniFile
  * @brief INI文件对象
  */
 typedef struct IniFile {
	 IniSection *sections;           /**< 节链表 */
	 pthread_mutex_t lock;           /**< 线程安全锁 */
	 bool modified;                  /**< 修改标志 */
 } IniFile;
 
 /**
  * @brief 创建INI文件对象
  * @return 成功返回INI文件对象指针，失败返回NULL
  */
 IniFile *ini_file_create(void);
 
 /**
  * @brief 销毁INI文件对象
  * @param file INI文件对象指针
  */
 void ini_file_destroy(IniFile *file);
 
 /**
  * @brief 从文件加载INI配置
  * @param file INI文件对象指针
  * @param filename 文件名
  * @return 错误码
  */
 IniErrorCode ini_file_load(IniFile *file, const char *filename);
 
 /**
  * @brief 保存INI配置到文件
  * @param file INI文件对象指针
  * @param filename 文件名
  * @return 错误码
  */
 IniErrorCode ini_file_save(IniFile *file, const char *filename);
 
 /**
  * @brief 获取字符串值
  * @param file INI文件对象指针
  * @param section 节名
  * @param key 键名
  * @param value 输出值缓冲区
  * @param len 缓冲区长度
  * @return 错误码
  */
 IniErrorCode ini_file_get_string(IniFile *file, const char *section, 
								 const char *key, char *value, size_t len);
 
 /**
  * @brief 设置字符串值
  * @param file INI文件对象指针
  * @param section 节名
  * @param key 键名
  * @param value 键值
  * @return 错误码
  */
 IniErrorCode ini_file_set_string(IniFile *file, const char *section, 
								 const char *key, const char *value);
 
 /**
  * @brief 获取整数值
  * @param file INI文件对象指针
  * @param section 节名
  * @param key 键名
  * @param value 输出值指针
  * @return 错误码
  */
 IniErrorCode ini_file_get_int(IniFile *file, const char *section, 
							 const char *key, int *value);
 
 /**
  * @brief 设置整数值
  * @param file INI文件对象指针
  * @param section 节名
  * @param key 键名
  * @param value 键值
  * @return 错误码
  */
 IniErrorCode ini_file_set_int(IniFile *file, const char *section, 
							 const char *key, int value);
 
 /**
  * @brief 获取布尔值
  * @param file INI文件对象指针
  * @param section 节名
  * @param key 键名
  * @param value 输出值指针
  * @return 错误码
  */
 IniErrorCode ini_file_get_bool(IniFile *file, const char *section, 
							  const char *key, bool *value);
 
 /**
  * @brief 设置布尔值
  * @param file INI文件对象指针
  * @param section 节名
  * @param key 键名
  * @param value 键值
  * @return 错误码
  */
 IniErrorCode ini_file_set_bool(IniFile *file, const char *section, 
							  const char *key, bool value);
 
 /**
  * @brief 删除节
  * @param file INI文件对象指针
  * @param section 节名
  * @return 错误码
  */
 IniErrorCode ini_file_remove_section(IniFile *file, const char *section);
 
 /**
  * @brief 删除键
  * @param file INI文件对象指针
  * @param section 节名
  * @param key 键名
  * @return 错误码
  */
 IniErrorCode ini_file_remove_key(IniFile *file, const char *section, 
								const char *key);
 
 /**
  * @brief 获取错误描述
  * @param code 错误码
  * @return 错误描述字符串
  */
 const char *ini_error_string(IniErrorCode code);
 
 #endif /* _LINUX_INI_PARSER_H */
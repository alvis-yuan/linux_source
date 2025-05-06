/**
 * @file ini_parser.c
 * @brief INI配置文件解析器实现
 */

 #include "ini_parser.h"
 #include <ctype.h>
 #include <errno.h>
 
 #define INI_LOG(fmt, ...) \
	 fprintf(stderr, "[INI] %s:%d: " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
 
 /**
  * @brief 分配内存并初始化为0
  * @param size 分配大小
  * @return 成功返回指针，失败返回NULL
  */
 static void *ini_malloc(size_t size)
 {
	 void *ptr = calloc(1, size);
	 if (!ptr) {
		 INI_LOG("Memory allocation failed, size=%zu", size);
	 }
	 return ptr;
 }
 
 /**
  * @brief 释放内存
  * @param ptr 内存指针
  */
 static void ini_free(void *ptr)
 {
	 free(ptr);
 }
 
 /**
  * @brief 去除字符串两端空白字符
  * @param str 输入字符串
  * @return 处理后的字符串
  */
 static char *ini_trim(char *str)
 {
	 char *end;
 
	 /* 去除前导空白 */
	 while (isspace((unsigned char)*str)) {
		 str++;
	 }
 
	 /* 全空白字符串 */
	 if (*str == '\0') {
		 return str;
	 }
 
	 /* 去除尾部空白 */
	 end = str + strlen(str) - 1;
	 while (end > str && isspace((unsigned char)*end)) {
		 end--;
	 }
 
	 /* 写入终止符 */
	 *(end + 1) = '\0';
 
	 return str;
 }
 
 /**
  * @brief 创建新的节
  * @param name 节名
  * @return 成功返回节指针，失败返回NULL
  */
 static IniSection *ini_section_create(const char *name)
 {
	 IniSection *section;
 
	 if (!name || *name == '\0') {
		 INI_LOG("Invalid section name");
		 return NULL;
	 }
 
	 section = ini_malloc(sizeof(IniSection));
	 if (!section) {
		 return NULL;
	 }
 
	 strncpy(section->name, name, INI_MAX_SECTION_LEN - 1);
	 section->name[INI_MAX_SECTION_LEN - 1] = '\0';
	 section->entries = NULL;
	 section->next = NULL;
 
	 return section;
 }
 
 /**
  * @brief 销毁节及其所有条目
  * @param section 节指针
  */
 static void ini_section_destroy(IniSection *section)
 {
	 IniEntry *entry, *next;
 
	 if (!section) {
		 return;
	 }
 
	 entry = section->entries;
	 while (entry) {
		 next = entry->next;
		 ini_free(entry);
		 entry = next;
	 }
 
	 ini_free(section);
 }
 
 /**
  * @brief 创建新的条目
  * @param key 键名
  * @param value 键值
  * @return 成功返回条目指针，失败返回NULL
  */
 static IniEntry *ini_entry_create(const char *key, const char *value)
 {
	 IniEntry *entry;
 
	 if (!key || *key == '\0') {
		 INI_LOG("Invalid key");
		 return NULL;
	 }
 
	 entry = ini_malloc(sizeof(IniEntry));
	 if (!entry) {
		 return NULL;
	 }
 
	 strncpy(entry->key, key, INI_MAX_KEY_LEN - 1);
	 entry->key[INI_MAX_KEY_LEN - 1] = '\0';
 
	 if (value) {
		 strncpy(entry->value, value, INI_MAX_VALUE_LEN - 1);
		 entry->value[INI_MAX_VALUE_LEN - 1] = '\0';
	 } else {
		 entry->value[0] = '\0';
	 }
 
	 entry->next = NULL;
 
	 return entry;
 }
 
 /**
  * @brief 查找节
  * @param file INI文件对象
  * @param section 节名
  * @return 找到返回节指针，未找到返回NULL
  */
 static IniSection *ini_find_section(const IniFile *file, const char *section)
 {
	 IniSection *curr;
 
	 if (!file || !section) {
		 return NULL;
	 }
 
	 curr = file->sections;
	 while (curr) {
		 if (strcasecmp(curr->name, section) == 0) {
			 return curr;
		 }
		 curr = curr->next;
	 }
 
	 return NULL;
 }
 
 /**
  * @brief 查找条目
  * @param section 节指针
  * @param key 键名
  * @return 找到返回条目指针，未找到返回NULL
  */
 static IniEntry *ini_find_entry(const IniSection *section, const char *key)
 {
	 IniEntry *curr;
 
	 if (!section || !key) {
		 return NULL;
	 }
 
	 curr = section->entries;
	 while (curr) {
		 if (strcasecmp(curr->key, key) == 0) {
			 return curr;
		 }
		 curr = curr->next;
	 }
 
	 return NULL;
 }
 
 IniFile *ini_file_create(void)
 {
	 IniFile *file;
 
	 file = ini_malloc(sizeof(IniFile));
	 if (!file) {
		 return NULL;
	 }
 
	 file->sections = NULL;
	 file->modified = false;
 
	 if (pthread_mutex_init(&file->lock, NULL) != 0) {
		 INI_LOG("Failed to initialize mutex");
		 ini_free(file);
		 return NULL;
	 }
 
	 return file;
 }
 
 void ini_file_destroy(IniFile *file)
 {
	 IniSection *section, *next;
 
	 if (!file) {
		 return;
	 }
 
	 pthread_mutex_lock(&file->lock);
 
	 section = file->sections;
	 while (section) {
		 next = section->next;
		 ini_section_destroy(section);
		 section = next;
	 }
 
	 pthread_mutex_unlock(&file->lock);
	 pthread_mutex_destroy(&file->lock);
	 ini_free(file);
 }
 
 IniErrorCode ini_file_load(IniFile *file, const char *filename)
 {
	 FILE *fp = NULL;
	 char line[INI_MAX_LINE_LEN];
	 IniSection *current_section = NULL;
	 IniErrorCode ret = INI_SUCCESS;
	 unsigned long line_num = 0;
	 char *pos, *key, *value;
 
	 if (!file || !filename) {
		 INI_LOG("Invalid parameters");
		 return INI_ERR_NULL_PTR;
	 }
 
	 fp = fopen(filename, "r");
	 if (!fp) {
		 INI_LOG("Failed to open file '%s': %s", filename, strerror(errno));
		 return INI_ERR_FILE_OPEN;
	 }
 
	 pthread_mutex_lock(&file->lock);
 
	 /* 清除现有内容 */
	 while (file->sections) {
		 IniSection *next = file->sections->next;
		 ini_section_destroy(file->sections);
		 file->sections = next;
	 }
 
	 while (fgets(line, sizeof(line), fp)) {
		 line_num++;
		 pos = ini_trim(line);
 
		 /* 跳过空行和注释 */
		 if (*pos == '\0' || *pos == ';' || *pos == '#') {
			 continue;
		 }
 
		 /* 处理节 */
		 if (*pos == '[') {
			 char *end;
 
			 pos++;
			 end = strchr(pos, ']');
			 if (!end) {
				 INI_LOG("Invalid section at line %lu", line_num);
				 ret = INI_ERR_INVALID_FORMAT;
				 goto error;
			 }
 
			 *end = '\0';
			 pos = ini_trim(pos);
 
			 if (*pos == '\0') {
				 INI_LOG("Empty section name at line %lu", line_num);
				 ret = INI_ERR_INVALID_FORMAT;
				 goto error;
			 }
 
			 /* 检查节是否已存在 */
			 if (ini_find_section(file, pos)) {
				 INI_LOG("Section '%s' already exists at line %lu", pos, line_num);
				 ret = INI_ERR_SECTION_EXISTS;
				 goto error;
			 }
 
			 current_section = ini_section_create(pos);
			 if (!current_section) {
				 INI_LOG("Failed to create section at line %lu", line_num);
				 ret = INI_ERR_MEMORY;
				 goto error;
			 }
 
			 /* 添加到链表 */
			 current_section->next = file->sections;
			 file->sections = current_section;
			 continue;
		 }
 
		 /* 必须有节才能添加条目 */
		 if (!current_section) {
			 INI_LOG("Entry outside section at line %lu", line_num);
			 ret = INI_ERR_INVALID_FORMAT;
			 goto error;
		 }
 
		 /* 处理键值对 */
		 value = strchr(pos, '=');
		 if (!value) {
			 INI_LOG("Missing '=' at line %lu", line_num);
			 ret = INI_ERR_INVALID_FORMAT;
			 goto error;
		 }
 
		 *value = '\0';
		 value++;
 
		 key = ini_trim(pos);
		 value = ini_trim(value);
 
		 if (*key == '\0') {
			 INI_LOG("Empty key at line %lu", line_num);
			 ret = INI_ERR_INVALID_FORMAT;
			 goto error;
		 }
 
		 /* 检查键是否已存在 */
		 if (ini_find_entry(current_section, key)) {
			 INI_LOG("Key '%s' already exists in section '%s' at line %lu", 
					key, current_section->name, line_num);
			 ret = INI_ERR_KEY_EXISTS;
			 goto error;
		 }
 
		 /* 创建并添加新条目 */
		 IniEntry *entry = ini_entry_create(key, value);
		 if (!entry) {
			 INI_LOG("Failed to create entry at line %lu", line_num);
			 ret = INI_ERR_MEMORY;
			 goto error;
		 }
 
		 entry->next = current_section->entries;
		 current_section->entries = entry;
	 }
 
	 if (ferror(fp)) {
		 INI_LOG("Error reading file '%s': %s", filename, strerror(errno));
		 ret = INI_ERR_FILE_READ;
		 goto error;
	 }
 
	 file->modified = false;
	 goto done;
 
 error:
	 /* 发生错误时清除已加载内容 */
	 while (file->sections) {
		 IniSection *next = file->sections->next;
		 ini_section_destroy(file->sections);
		 file->sections = next;
	 }
 
 done:
	 pthread_mutex_unlock(&file->lock);
	 fclose(fp);
	 return ret;
 }
 
 IniErrorCode ini_file_save(IniFile *file, const char *filename)
 {
	 FILE *fp = NULL;
	 IniSection *section;
	 IniEntry *entry;
	 IniErrorCode ret = INI_SUCCESS;
 
	 if (!file || !filename) {
		 INI_LOG("Invalid parameters");
		 return INI_ERR_NULL_PTR;
	 }
 
	 fp = fopen(filename, "w");
	 if (!fp) {
		 INI_LOG("Failed to open file '%s': %s", filename, strerror(errno));
		 return INI_ERR_FILE_OPEN;
	 }
 
	 pthread_mutex_lock(&file->lock);
 
	 /* 按逆序写入，因为链表是逆序存储的 */
	 section = file->sections;
	 while (section) {
		 /* 写入节名 */
		 if (fprintf(fp, "[%s]\n", section->name) < 0) {
			 INI_LOG("Failed to write section '%s'", section->name);
			 ret = INI_ERR_FILE_WRITE;
			 goto done;
		 }
 
		 /* 写入条目 */
		 entry = section->entries;
		 while (entry) {
			 if (fprintf(fp, "%s = %s\n", entry->key, entry->value) < 0) {
				 INI_LOG("Failed to write key '%s'", entry->key);
				 ret = INI_ERR_FILE_WRITE;
				 goto done;
			 }
			 entry = entry->next;
		 }
 
		 /* 节之间添加空行 */
		 if (section->next && fprintf(fp, "\n") < 0) {
			 INI_LOG("Failed to write newline");
			 ret = INI_ERR_FILE_WRITE;
			 goto done;
		 }
 
		 section = section->next;
	 }
 
	 if (fflush(fp) != 0) {
		 INI_LOG("Failed to flush file: %s", strerror(errno));
		 ret = INI_ERR_FILE_WRITE;
		 goto done;
	 }
 
	 file->modified = false;
 
 done:
	 pthread_mutex_unlock(&file->lock);
	 fclose(fp);
	 return ret;
 }
 
 IniErrorCode ini_file_get_string(IniFile *file, const char *section, 
								const char *key, char *value, size_t len)
 {
	 IniSection *sec;
	 IniEntry *ent;
 
	 if (!file || !section || !key || !value || len == 0) {
		 INI_LOG("Invalid parameters");
		 return INI_ERR_NULL_PTR;
	 }
 
	 pthread_mutex_lock(&file->lock);
 
	 sec = ini_find_section(file, section);
	 if (!sec) {
		 INI_LOG("Section '%s' not found", section);
		 pthread_mutex_unlock(&file->lock);
		 return INI_ERR_NOT_FOUND;
	 }
 
	 ent = ini_find_entry(sec, key);
	 if (!ent) {
		 INI_LOG("Key '%s' not found in section '%s'", key, section);
		 pthread_mutex_unlock(&file->lock);
		 return INI_ERR_NOT_FOUND;
	 }
 
	 strncpy(value, ent->value, len - 1);
	 value[len - 1] = '\0';
 
	 pthread_mutex_unlock(&file->lock);
	 return INI_SUCCESS;
 }
 
 IniErrorCode ini_file_set_string(IniFile *file, const char *section, 
								const char *key, const char *value)
 {
	 IniSection *sec;
	 IniEntry *ent, *prev;
 
	 if (!file || !section || !key) {
		 INI_LOG("Invalid parameters");
		 return INI_ERR_NULL_PTR;
	 }
 
	 pthread_mutex_lock(&file->lock);
 
	 sec = ini_find_section(file, section);
	 if (!sec) {
		 /* 节不存在则创建 */
		 sec = ini_section_create(section);
		 if (!sec) {
			 INI_LOG("Failed to create section '%s'", section);
			 pthread_mutex_unlock(&file->lock);
			 return INI_ERR_MEMORY;
		 }
 
		 sec->next = file->sections;
		 file->sections = sec;
	 }
 
	 /* 查找现有条目 */
	 ent = sec->entries;
	 prev = NULL;
	 while (ent) {
		 if (strcasecmp(ent->key, key) == 0) {
			 break;
		 }
		 prev = ent;
		 ent = ent->next;
	 }
 
	 if (ent) {
		 /* 更新现有条目 */
		 if (value) {
			 strncpy(ent->value, value, INI_MAX_VALUE_LEN - 1);
			 ent->value[INI_MAX_VALUE_LEN - 1] = '\0';
		 } else {
			 ent->value[0] = '\0';
		 }
	 } else {
		 /* 创建新条目 */
		 ent = ini_entry_create(key, value);
		 if (!ent) {
			 INI_LOG("Failed to create entry '%s'", key);
			 pthread_mutex_unlock(&file->lock);
			 return INI_ERR_MEMORY;
		 }
 
		 /* 添加到链表头部 */
		 ent->next = sec->entries;
		 sec->entries = ent;
	 }
 
	 file->modified = true;
	 pthread_mutex_unlock(&file->lock);
	 return INI_SUCCESS;
 }
 
 IniErrorCode ini_file_get_int(IniFile *file, const char *section, 
							 const char *key, int *value)
 {
	 char str_value[INI_MAX_VALUE_LEN];
	 char *end;
	 long int val;
	 IniErrorCode ret;
 
	 if (!value) {
		 INI_LOG("Invalid parameters");
		 return INI_ERR_NULL_PTR;
	 }
 
	 ret = ini_file_get_string(file, section, key, str_value, sizeof(str_value));
	 if (ret != INI_SUCCESS) {
		 return ret;
	 }
 
	 errno = 0;
	 val = strtol(str_value, &end, 0);
	 if (errno != 0 || *end != '\0') {
		 INI_LOG("Invalid integer value '%s' for key '%s'", str_value, key);
		 return INI_ERR_INVALID_FORMAT;
	 }
 
	 *value = (int)val;
	 return INI_SUCCESS;
 }
 
 IniErrorCode ini_file_set_int(IniFile *file, const char *section, 
							 const char *key, int value)
 {
	 char str_value[32];
	 snprintf(str_value, sizeof(str_value), "%d", value);
	 return ini_file_set_string(file, section, key, str_value);
 }
 
 IniErrorCode ini_file_get_bool(IniFile *file, const char *section, 
							  const char *key, bool *value)
 {
	 char str_value[INI_MAX_VALUE_LEN];
	 IniErrorCode ret;
 
	 if (!value) {
		 INI_LOG("Invalid parameters");
		 return INI_ERR_NULL_PTR;
	 }
 
	 ret = ini_file_get_string(file, section, key, str_value, sizeof(str_value));
	 if (ret != INI_SUCCESS) {
		 return ret;
	 }
 
	 if (strcasecmp(str_value, "true") == 0 || 
		 strcasecmp(str_value, "yes") == 0 || 
		 strcasecmp(str_value, "1") == 0) {
		 *value = true;
	 } else if (strcasecmp(str_value, "false") == 0 || 
				strcasecmp(str_value, "no") == 0 || 
				strcasecmp(str_value, "0") == 0) {
		 *value = false;
	 } else {
		 INI_LOG("Invalid boolean value '%s' for key '%s'", str_value, key);
		 return INI_ERR_INVALID_FORMAT;
	 }
 
	 return INI_SUCCESS;
 }
 
 IniErrorCode ini_file_set_bool(IniFile *file, const char *section, 
							  const char *key, bool value)
 {
	 return ini_file_set_string(file, section, key, value ? "true" : "false");
 }
 
 IniErrorCode ini_file_remove_section(IniFile *file, const char *section)
 {
	 IniSection *prev = NULL;
	 IniSection *curr;
 
	 if (!file || !section) {
		 INI_LOG("Invalid parameters");
		 return INI_ERR_NULL_PTR;
	 }
 
	 pthread_mutex_lock(&file->lock);
 
	 curr = file->sections;
	 while (curr) {
		 if (strcasecmp(curr->name, section) == 0) {
			 if (prev) {
				 prev->next = curr->next;
			 } else {
				 file->sections = curr->next;
			 }
 
			 ini_section_destroy(curr);
			 file->modified = true;
			 pthread_mutex_unlock(&file->lock);
			 return INI_SUCCESS;
		 }
 
		 prev = curr;
		 curr = curr->next;
	 }
 
	 INI_LOG("Section '%s' not found", section);
	 pthread_mutex_unlock(&file->lock);
	 return INI_ERR_NOT_FOUND;
 }
 
 IniErrorCode ini_file_remove_key(IniFile *file, const char *section, 
								const char *key)
 {
	 IniSection *sec;
	 IniEntry *prev = NULL;
	 IniEntry *curr;
 
	 if (!file || !section || !key) {
		 INI_LOG("Invalid parameters");
		 return INI_ERR_NULL_PTR;
	 }
 
	 pthread_mutex_lock(&file->lock);
 
	 sec = ini_find_section(file, section);
	 if (!sec) {
		 INI_LOG("Section '%s' not found", section);
		 pthread_mutex_unlock(&file->lock);
		 return INI_ERR_NOT_FOUND;
	 }
 
	 curr = sec->entries;
	 while (curr) {
		 if (strcasecmp(curr->key, key) == 0) {
			 if (prev) {
				 prev->next = curr->next;
			 } else {
				 sec->entries = curr->next;
			 }
 
			 ini_free(curr);
			 file->modified = true;
			 pthread_mutex_unlock(&file->lock);
			 return INI_SUCCESS;
		 }
 
		 prev = curr;
		 curr = curr->next;
	 }
 
	 INI_LOG("Key '%s' not found in section '%s'", key, section);
	 pthread_mutex_unlock(&file->lock);
	 return INI_ERR_NOT_FOUND;
 }
 
 const char *ini_error_string(IniErrorCode code)
 {
	 static const char *strings[] = {
		 "Success",
		 "Memory allocation failed",
		 "Failed to open file",
		 "Failed to read file",
		 "Failed to write file",
		 "Invalid INI format",
		 "Section already exists",
		 "Key already exists",
		 "Section or key not found",
		 "Null pointer",
		 "Internal error"
	 };
 
	 if (code < 0 || code >= (int)(sizeof(strings) / sizeof(strings[0]))) {
		 return "Unknown error";
	 }
 
	 return strings[code];
 }
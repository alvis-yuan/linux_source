#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include "filter.h"

struct procname_filter_node {
	char *name;
	int filtered;
	struct procname_filter_node *next;
};

static struct procname_filter_node *procname_list = NULL;

#define FILTER_PROCESS_DIR "/home/xiaoyuan/workspace/printer/app/CloudPrinter_Application/server/mylog"

static void procname_add(const char *procname)
 {
	 struct procname_filter_node *filter = calloc(sizeof(*filter), 1);
	 if (!filter)
		 return;

	 filter->name = strdup(procname);
	 filter->filtered = 0; // 默认过滤掉
	 filter->next = procname_list;
	 procname_list = filter;
 }

 static void procname_remove(const char *procname)
 {
	 struct procname_filter_node **p = &procname_list;
	 while (*p) {
		 if (strcmp((*p)->name, procname) == 0) {
			 struct procname_filter_node *tmp = *p;
			 *p = (*p)->next;
			 free(tmp->name);
			 free(tmp);
			 return;
		 }
		 p = &(*p)->next;
	 }
 }

 static void procname_destroy(struct filter_node *node)
 {
	(void)node;
    //使用linux内核二级指针方式释放链表
    struct procname_filter_node **p = &procname_list;
	while (*p) {
		struct procname_filter_node *tmp = *p;
		*p = (*p)->next;
		free(tmp->name);
		free(tmp);
	}
 }

 // 进程名过滤器
 static bool procname_filter(const char *procname)
 {
	 struct procname_filter_node *p = procname_list;
	 while (p) {
		 if (strcmp(p->name, procname) == 0) {
			 //LocalDbg("procname %s filter %s", procname, p->filtered ? "true" : "false");
			 return p->filtered;
		 }
		 p = p->next;
	 }
	 //LocalDbg("procname %s not found in filter list", procname);
	 return false; // 不过滤
 }

 static int procname_init(struct filter_node *node)
 {
	(void)node;

	 // 遍历指定目录下的所有可执行文件，跳过链接文件, 添加到过滤器
	 struct dirent **list = NULL;
	 int count = scandir(FILTER_PROCESS_DIR, &list, NULL, alphasort);
	 for (int i = 0; i < count; i++) {
		 struct stat st;
		 char path[256 * 2];
		 snprintf(path, sizeof(path), "%s/%s", FILTER_PROCESS_DIR, list[i]->d_name);
		 if (stat(path, &st) == 0 && S_ISREG(st.st_mode) && st.st_mode & S_IXUSR) {
			procname_add(list[i]->d_name);
		 }
		 free(list[i]);
	 }
	 if (list)
		 free(list);

	return 0;
 }

 static bool procname_process(struct filter_node *node, log_msg_t *msg)
 {
	(void)node;

	 if (!msg) {
		 LocalDbg("msg is NULL");
		 return false;
	 }

	 // 根据msg->pid获取进程名
	 char procname[256];
	 snprintf(procname, sizeof(procname), "/proc/%d/comm", msg->pid);
	 FILE *fp = fopen(procname, "r");
	 if (fp) {
		 fgets(procname, sizeof(procname), fp);
		 fclose(fp);
	 } else {
		 LocalDbg("Failed to open %s", procname);
		 return false; // 无法获取进程名
	 }
	 // 去掉换行符
	 size_t len = strlen(procname);
	 if (len > 0 && procname[len - 1] == '\n') {
		 procname[len - 1] = '\0';
	 }
	 
	return procname_filter(procname);
 }

 static struct filter_ops procname_ops = {
	.init = procname_init,
	.filter = procname_process,
	.destroy = procname_destroy,
 };

static struct filter_node pn_node = {
	.name = "procname",
	.ops = &procname_ops,
};

static __attribute__((constructor)) void procname_filter_init()
{
	// 注册进程名过滤器
	filter_register(&pn_node);
}

static __attribute__((destructor)) void procname_filter_destroy()
{
	// 注销进程名过滤器
	filter_unregister(&pn_node);
}
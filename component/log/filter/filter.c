/**
 * @file filter.c
 * @brief 过滤器通用框架实现
 */

 #include "filter.h"
 #include <stdlib.h>
 #include <string.h>

 static struct filter_node *filters;

 int filter_register(struct filter_node *node)
 {
 	if (!node || !node->ops)
 		return -1;

 	node->next = filters;
 	filters = node;

 	if (node->ops->init)
 		node->ops->init(node);

 	return 0;
 }

 int filter_unregister(struct filter_node *node)
 {
 	struct filter_node **p = &filters;
 	while (*p) {
 		if (*p == node) {
 			*p = node->next;
 			if (node->ops->destroy)
 				node->ops->destroy(node);
 			return 0;
 		}
 		p = &(*p)->next;
 	}
 	return -1;
 }

 bool filter_process(log_msg_t *msg)
 {
 	struct filter_node *node = filters;
	bool on = false;

 	while (node) {
 		if (node->ops->filter) {
			on = node->ops->filter(node, msg);
			//LocalDbg("filter %s result:%s", node->name, on?"true":"false");
			if(on)
				return true; // 过滤掉
		}
 		node = node->next;
 	}
 	return false;
 }
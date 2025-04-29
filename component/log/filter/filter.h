/**
 * @file filter.h
 * @brief 过滤器通用框架定义
 */

 #ifndef __FILTER_H__
 #define __FILTER_H__
 
 #include "../log_api.h"
 #include <stdbool.h>

 struct filter_node;

 /**
  * @struct filter_ops
  * @brief 过滤器操作函数表
  */
 struct filter_ops {
	int (*init)(struct filter_node *node); /**< 初始化 */
	bool (*filter)(struct filter_node *node, log_msg_t *msg); /**< 过滤 */
	void (*destroy)(struct filter_node *node); /**< 销毁 */
 };
 
 /**
  * @struct filter_node
  * @brief 过滤器节点结构
  */
 struct filter_node {
	char *name; /**< 过滤器名称 */
	struct filter_ops *ops; /**< 过滤器操作函数表 */
	struct filter_node *next; /**< 下一个过滤器节点 */
};
 
 /**
  * @brief 注册过滤器
  * @param node 过滤器节点
  * @return 成功返回0，失败返回-1
  */
  int filter_register(struct filter_node *node);

 /**
  * @brief 注销过滤器
  * @param node 过滤器节点
  * @return 成功返回0，失败返回-1
  */
  int filter_unregister(struct filter_node *node);

 /**
  * @brief 处理过滤器
  * @param data 过滤数据
  * @return true-过滤掉，false-未过滤
  */
 bool filter_process(log_msg_t *msg);
 
 #endif /* __FILTER_H__ */
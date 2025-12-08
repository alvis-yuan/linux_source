/**
 * @file libmyipc.h
 * @brief 基于 Generic Netlink 的通用 IPC 库接口定义
 * @author Assistant
 * @version 1.0
 * @date 2023-10-27
 */

#ifndef _LIB_MYIPC_H_
#define _LIB_MYIPC_H_

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief IPC上下文句柄，外部不可见具体实现 */
typedef struct myipc_ctx myipc_ctx_t;

/**
 * @brief 异步调用的回调函数原型
 * @param status 状态码 (0: 成功, <0: 错误码)
 * @param data 返回的数据指针
 * @param len 返回的数据长度
 * @param priv 用户私有数据
 */
typedef void (*myipc_async_cb_t)(int status, void *data, size_t len, void *priv);

/**
 * @brief 服务端方法处理函数原型
 * @param arg 请求参数数据
 * @param arg_len 参数长度
 * @param ret_buf [out] 结果写入缓冲区
 * @param max_ret_len 结果缓冲区最大长度
 * @return 实际写入长度，负值表示业务错误
 */
typedef int (*myipc_method_handler_t)(const void *arg, size_t arg_len, 
                                      void *ret_buf, size_t max_ret_len);

/**
 * @brief 事件订阅回调函数原型
 * @param topic 主题名称
 * @param data 事件数据
 * @param len 数据长度
 */
typedef void (*myipc_event_cb_t)(const char *topic, const void *data, size_t len);

/* --- 初始化与销毁 --- */

/**
 * @brief 初始化 IPC 上下文
 * @return 成功返回句柄，失败返回 NULL
 */
myipc_ctx_t *myipc_init(void);

/**
 * @brief 销毁 IPC 上下文并释放资源
 * @param ctx IPC 句柄
 */
void myipc_cleanup(myipc_ctx_t *ctx);

/* --- 服务端 API --- */

/**
 * @brief 注册服务名称
 * @param ctx IPC 句柄
 * @param service_name 服务名称 (例如 "network.lan")
 * @return 0 成功, <0 失败
 */
int myipc_register_service(myipc_ctx_t *ctx, const char *service_name);

/**
 * @brief 添加本地方法
 * @param ctx IPC 句柄
 * @param method_name 方法名
 * @param handler 回调函数
 * @return 0 成功, <0 失败
 */
int myipc_add_method(myipc_ctx_t *ctx, const char *method_name, 
                     myipc_method_handler_t handler);

/* --- 客户端 API --- */

/**
 * @brief 查找服务对应的 Port ID
 * @param ctx IPC 句柄
 * @param service_name 服务名称
 * @return >0 为 Port ID, <0 失败
 */
int32_t myipc_lookup_service(myipc_ctx_t *ctx, const char *service_name);

/**
 * @brief 同步 RPC 调用 (阻塞等待结果)
 * @param ctx IPC 句柄
 * @param target_pid 目标服务的 Port ID
 * @param method 方法名
 * @param arg 参数数据
 * @param arg_len 参数长度
 * @param ret_buf [out] 接收结果的缓冲区
 * @param max_ret_len 缓冲区大小
 * @param timeout_ms 超时时间(毫秒)
 * @return >=0 实际返回长度, <0 错误码
 */
int myipc_call_sync(myipc_ctx_t *ctx, int32_t target_pid, const char *method,
                    const void *arg, size_t arg_len,
                    void *ret_buf, size_t max_ret_len, int timeout_ms);

/**
 * @brief 异步 RPC 调用 (非阻塞)
 * @param ctx IPC 句柄
 * @param target_pid 目标服务的 Port ID
 * @param method 方法名
 * @param arg 参数数据
 * @param arg_len 参数长度
 * @param cb 完成时的回调函数
 * @param priv 用户私有数据
 * @return 0 成功(请求已发送), <0 失败
 */
int myipc_call_async(myipc_ctx_t *ctx, int32_t target_pid, const char *method,
                     const void *arg, size_t arg_len,
                     myipc_async_cb_t cb, void *priv);

/* --- 发布/订阅 API --- */

/**
 * @brief 订阅事件
 * @param ctx IPC 句柄
 * @param cb 事件回调函数
 * @return 0 成功, <0 失败
 */
int myipc_subscribe(myipc_ctx_t *ctx, myipc_event_cb_t cb);

/**
 * @brief 发布事件
 * @param ctx IPC 句柄
 * @param topic 主题名
 * @param data 数据
 * @param len 数据长度
 * @return 0 成功, <0 失败
 */
int myipc_publish(myipc_ctx_t *ctx, const char *topic, const void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* _LIB_MYIPC_H_ */
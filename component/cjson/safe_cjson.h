#ifndef SAFE_CJSON_H
#define SAFE_CJSON_H

#include <stdbool.h>
#include "cJSON.h"

// 定义错误码
typedef enum {
    SCJ_SUCCESS = 0,
    SCJ_ERR_NULL_PTR,
    SCJ_ERR_INVALID_JSON,
    SCJ_ERR_TYPE_MISMATCH,
    SCJ_ERR_NOT_FOUND,
    SCJ_ERR_MEMORY,
    SCJ_ERR_IO
} SCJ_ErrorCode;

// 封装后的 JSON 对象句柄
typedef struct SCJ_Handle SCJ_Handle;

// 创建/销毁
SCJ_Handle* SCJ_CreateObject(void);
SCJ_Handle* SCJ_Parse(const char* jsonStr);
void SCJ_Delete(SCJ_Handle** handle);

// 基础类型操作
SCJ_ErrorCode SCJ_AddNumber(SCJ_Handle* handle, const char* key, double value);
SCJ_ErrorCode SCJ_AddString(SCJ_Handle* handle, const char* key, const char* value);
SCJ_ErrorCode SCJ_AddBool(SCJ_Handle* handle, const char* key, bool value);

SCJ_ErrorCode SCJ_GetNumber(const SCJ_Handle* handle, const char* key, double* outValue);
SCJ_ErrorCode SCJ_GetString(const SCJ_Handle* handle, const char* key, char** outValue);
SCJ_ErrorCode SCJ_GetBool(const SCJ_Handle* handle, const char* key, bool* outValue);

// 数组操作
SCJ_ErrorCode SCJ_AddArray(SCJ_Handle* handle, const char* key);
SCJ_ErrorCode SCJ_AppendToArray(SCJ_Handle* arrayHandle, SCJ_Handle* element);
SCJ_ErrorCode SCJ_GetArraySize(const SCJ_Handle* handle, const char* key, int* outSize);
SCJ_ErrorCode SCJ_GetArrayItem(const SCJ_Handle* handle, const char* key, int index, SCJ_Handle** outItem);

// 实用功能
SCJ_ErrorCode SCJ_ToString(const SCJ_Handle* handle, char** outJsonStr, bool formatted);
SCJ_ErrorCode SCJ_SaveToFile(const SCJ_Handle* handle, const char* filename);
SCJ_Handle* SCJ_LoadFromFile(const char* filename);

// 错误处理
const char* SCJ_GetErrorString(SCJ_ErrorCode code);
SCJ_ErrorCode SCJ_GetLastError(const SCJ_Handle* handle);

#endif // SAFE_CJSON_H
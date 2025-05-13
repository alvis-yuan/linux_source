#include "safe_cjson.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// 结构体定义
struct SCJ_Handle {
    cJSON* json;            // 实际的 cJSON 对象
    SCJ_ErrorCode lastError; // 最后一次错误码
    bool isArray;           // 标记是否为数组
};

// 内部函数声明
static bool SCJ_ValidateHandle(const SCJ_Handle* handle);
static bool SCJ_ValidateKey(const char* key);
static void SCJ_SetError(SCJ_Handle* handle, SCJ_ErrorCode code);

// 创建新对象
SCJ_Handle* SCJ_CreateObject(void) {
    SCJ_Handle* handle = (SCJ_Handle*)malloc(sizeof(SCJ_Handle));
    if (!handle) return NULL;
    
    handle->json = cJSON_CreateObject();
    if (!handle->json) {
        free(handle);
        return NULL;
    }
    
    handle->lastError = SCJ_SUCCESS;
    handle->isArray = false;
    return handle;
}

// 解析 JSON 字符串
SCJ_Handle* SCJ_Parse(const char* jsonStr) {
    if (!jsonStr) return NULL;
    
    SCJ_Handle* handle = (SCJ_Handle*)malloc(sizeof(SCJ_Handle));
    if (!handle) return NULL;
    
    handle->json = cJSON_Parse(jsonStr);
    if (!handle->json) {
        free(handle);
        return NULL;
    }
    
    handle->lastError = SCJ_SUCCESS;
    handle->isArray = cJSON_IsArray(handle->json);
    return handle;
}

// 释放资源
void SCJ_Delete(SCJ_Handle** handle) {
    if (!handle || !*handle) return;
    
    cJSON_Delete((*handle)->json);
    free(*handle);
    *handle = NULL;
}

// 添加数字
SCJ_ErrorCode SCJ_AddNumber(SCJ_Handle* handle, const char* key, double value) {
    if (!SCJ_ValidateHandle(handle) || !SCJ_ValidateKey(key)) {
        return handle ? handle->lastError : SCJ_ERR_NULL_PTR;
    }
    
    if (handle->isArray) {
        SCJ_SetError(handle, SCJ_ERR_TYPE_MISMATCH);
        return SCJ_ERR_TYPE_MISMATCH;
    }
    
    cJSON* item = cJSON_CreateNumber(value);
    if (!item) {
        SCJ_SetError(handle, SCJ_ERR_MEMORY);
        return SCJ_ERR_MEMORY;
    }
    
    cJSON_AddItemToObject(handle->json, key, item);
    return SCJ_SUCCESS;
}

// 获取字符串（调用者需要释放返回的内存）
SCJ_ErrorCode SCJ_GetString(const SCJ_Handle* handle, const char* key, char** outValue) {
    if (!outValue) return SCJ_ERR_NULL_PTR;
    *outValue = NULL;
    
    if (!SCJ_ValidateHandle(handle) || !SCJ_ValidateKey(key)) {
        return handle ? handle->lastError : SCJ_ERR_NULL_PTR;
    }
    
    cJSON* item = cJSON_GetObjectItemCaseSensitive(handle->json, key);
    if (!item || !cJSON_IsString(item)) {
        SCJ_SetError((SCJ_Handle*)handle, item ? SCJ_ERR_TYPE_MISMATCH : SCJ_ERR_NOT_FOUND);
        return item ? SCJ_ERR_TYPE_MISMATCH : SCJ_ERR_NOT_FOUND;
    }
    
    *outValue = strdup(item->valuestring);
    if (!*outValue) {
        SCJ_SetError((SCJ_Handle*)handle, SCJ_ERR_MEMORY);
        return SCJ_ERR_MEMORY;
    }
    
    return SCJ_SUCCESS;
}

// 保存到文件
SCJ_ErrorCode SCJ_SaveToFile(const SCJ_Handle* handle, const char* filename) {
    if (!filename) return SCJ_ERR_NULL_PTR;
    if (!SCJ_ValidateHandle(handle)) return handle->lastError;
    
    char* jsonStr = cJSON_Print(handle->json);
    if (!jsonStr) {
        SCJ_SetError((SCJ_Handle*)handle, SCJ_ERR_MEMORY);
        return SCJ_ERR_MEMORY;
    }
    
    FILE* fp = fopen(filename, "w");
    if (!fp) {
        free(jsonStr);
        SCJ_SetError((SCJ_Handle*)handle, SCJ_ERR_IO);
        return SCJ_ERR_IO;
    }
    
    fputs(jsonStr, fp);
    fclose(fp);
    free(jsonStr);
    return SCJ_SUCCESS;
}

// 内部验证函数
static bool SCJ_ValidateHandle(const SCJ_Handle* handle) {
    if (!handle || !handle->json) {
        if (handle) SCJ_SetError((SCJ_Handle*)handle, SCJ_ERR_NULL_PTR);
        return false;
    }
    return true;
}

static bool SCJ_ValidateKey(const char* key) {
    if (!key || !*key) {
        return false;
    }
    return true;
}

static void SCJ_SetError(SCJ_Handle* handle, SCJ_ErrorCode code) {
    if (handle) {
        handle->lastError = code;
    }
}

// 其他函数实现...
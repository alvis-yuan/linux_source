#include "safe_cjson.h"
#include <stdio.h>

int main() {
    // 创建JSON对象
    SCJ_Handle* json = SCJ_CreateObject();
    if (!json) {
        printf("Failed to create JSON object\n");
        return -1;
    }
    
    // 添加数据
    SCJ_AddNumber(json, "age", 25);
    SCJ_AddString(json, "name", "John Doe");
    SCJ_AddBool(json, "is_student", true);
    
    // 创建嵌套数组
    SCJ_Handle* scores = SCJ_CreateObject();
    SCJ_AddNumber(scores, "math", 90);
    SCJ_AddNumber(scores, "english", 85);
    SCJ_AddItemToObject(json, "scores", scores);
    
    // 输出JSON
    char* jsonStr;
    if (SCJ_ToString(json, &jsonStr, true) == SCJ_SUCCESS) {
        printf("Generated JSON:\n%s\n", jsonStr);
        free(jsonStr);
    }
    
    // 错误处理示例
    char* invalidKey = NULL;
    SCJ_ErrorCode err = SCJ_AddString(json, invalidKey, "value");
    if (err != SCJ_SUCCESS) {
        printf("Error: %s\n", SCJ_GetErrorString(err));
    }
    
    // 清理
    SCJ_Delete(&json);
    return 0;
}
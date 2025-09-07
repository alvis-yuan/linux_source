#include <openssl/evp.h>  
#include <openssl/bio.h>  
#include <openssl/buffer.h>  
#include <string.h>  
#include <stdio.h>  
  
/**  
 * Base64编码函数  
 * @param input 需要编码的数据  
 * @param length 数据长度  
 * @param output 输出的编码结果  
 * @param output_length 输出的编码结果长度  
 * @return 成功返回1，失败返回0  
 */  
int base64_encode(const unsigned char *input, size_t length,  
                  char **output, size_t *output_length) {  
    BIO *bio, *b64;  
    BUF_MEM *bufferPtr;  
      
    // 创建一个新的BIO链，包含base64过滤器和内存BIO  
    b64 = BIO_new(BIO_f_base64());  
    bio = BIO_new(BIO_s_mem());  
    bio = BIO_push(b64, bio);  
      
    // 禁用换行符  
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);  
      
    // 写入数据  
    BIO_write(bio, input, length);  
    BIO_flush(bio);  
      
    // 获取结果  
    BIO_get_mem_ptr(bio, &bufferPtr);  
      
    // 分配输出缓冲区  
    *output = (char *)malloc(bufferPtr->length + 1);  
    if (!*output) {  
        BIO_free_all(bio);  
        return 0;  
    }  
      
    // 复制结果  
    memcpy(*output, bufferPtr->data, bufferPtr->length);  
    (*output)[bufferPtr->length] = '\0';  
      
    // 设置输出长度  
    if (output_length) {  
        *output_length = bufferPtr->length;  
    }  
      
    // 释放资源  
    BIO_free_all(bio);  
      
    return 1;  
}  
  
/**  
 * Base64解码函数  
 * @param input 需要解码的数据  
 * @param length 数据长度  
 * @param output 输出的解码结果  
 * @param output_length 输出的解码结果长度  
 * @return 成功返回1，失败返回0  
 */  
int base64_decode(const char *input, size_t length,  
                  unsigned char **output, size_t *output_length) {  
    BIO *bio, *b64;  
      
    // 计算解码后的最大长度（保守估计）  
    size_t max_len = length / 4 * 3 + 1;  
      
    // 分配输出缓冲区  
    *output = (unsigned char *)malloc(max_len);  
    if (!*output) {  
        return 0;  
    }  
      
    // 创建一个新的BIO链，包含base64过滤器和内存BIO  
    b64 = BIO_new(BIO_f_base64());  
    bio = BIO_new_mem_buf(input, length);  
    bio = BIO_push(b64, bio);  
      
    // 禁用换行符  
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);  
      
    // 读取解码后的数据  
    int decoded_len = BIO_read(bio, *output, length);  
    if (decoded_len <= 0) {  
        free(*output);  
        *output = NULL;  
        BIO_free_all(bio);  
        return 0;  
    }  
      
    // 设置输出长度  
    if (output_length) {  
        *output_length = decoded_len;  
    }  
      
    // 添加字符串结束符  
    (*output)[decoded_len] = '\0';  
      
    // 释放资源  
    BIO_free_all(bio);  
      
    return 1;  
}  
  
/**  
 * 使用EVP_EncodeBlock和EVP_DecodeBlock的替代实现  
 * 这些函数更简单，但不支持流式处理  
 */  
  
/**  
 * 使用EVP_EncodeBlock进行Base64编码  
 * @param input 需要编码的数据  
 * @param length 数据长度  
 * @param output 输出的编码结果（需要预先分配足够空间）  
 * @return 编码后的长度  
 */  
int base64_encode_block(const unsigned char *input, size_t length, char *output) {  
    return EVP_EncodeBlock((unsigned char *)output, input, length);  
}  
  
/**  
 * 使用EVP_DecodeBlock进行Base64解码  
 * @param input 需要解码的数据  
 * @param length 数据长度  
 * @param output 输出的解码结果（需要预先分配足够空间）  
 * @return 解码后的长度，失败返回-1  
 */  
int base64_decode_block(const char *input, size_t length, unsigned char *output) {  
    int ret = EVP_DecodeBlock(output, (const unsigned char *)input, length);  
      
    // 处理填充问题  
    if (length > 0) {  
        // 检查输入末尾的填充字符数量  
        int padding = 0;  
        if (input[length-1] == '=') padding++;  
        if (length > 1 && input[length-2] == '=') padding++;  
          
        // 调整输出长度  
        if (ret > 0) {  
            ret -= padding;  
        }  
    }  
      
    return ret;  
}

/**  
 * 流式Base64编码示例  
 * @param in_file 输入文件  
 * @param out_file 输出文件  
 * @return 成功返回1，失败返回0  
 */  
int base64_encode_file(const char *in_file, const char *out_file) {  
    FILE *in = NULL, *out = NULL;  
    BIO *bio, *b64;  
    unsigned char buffer[1024];  
    int bytes_read;  
      
    // 打开文件  
    in = fopen(in_file, "rb");  
    if (!in) {  
        return 0;  
    }  
      
    out = fopen(out_file, "wb");  
    if (!out) {  
        fclose(in);  
        return 0;  
    }  
      
    // 创建BIO链  
    b64 = BIO_new(BIO_f_base64());  
    bio = BIO_new_fp(out, BIO_NOCLOSE);  
    bio = BIO_push(b64, bio);  
      
    // 禁用换行符  
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);  
      
    // 读取输入文件并编码  
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), in)) > 0) {  
        BIO_write(bio, buffer, bytes_read);  
    }  
      
    // 刷新并关闭BIO  
    BIO_flush(bio);  
    BIO_free_all(bio);  
      
    // 关闭文件  
    fclose(in);  
    fclose(out);  
      
    return 1;  
}  
  
/**  
 * 流式Base64解码示例  
 * @param in_file 输入文件  
 * @param out_file 输出文件  
 * @return 成功返回1，失败返回0  
 */  
int base64_decode_file(const char *in_file, const char *out_file) {  
    FILE *in = NULL, *out = NULL;  
    BIO *bio, *b64;  
    unsigned char buffer[1024];  
    int bytes_read;  
      
    // 打开文件  
    in = fopen(in_file, "rb");  
    if (!in) {  
        return 0;  
    }  
      
    out = fopen(out_file, "wb");  
    if (!out) {  
        fclose(in);  
        return 0;  
    }  
      
    // 创建BIO链  
    b64 = BIO_new(BIO_f_base64());  
    bio = BIO_new_fp(in, BIO_NOCLOSE);  
    bio = BIO_push(b64, bio);  
      
    // 禁用换行符  
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);  
      
    // 读取输入文件并解码  
    while ((bytes_read = BIO_read(bio, buffer, sizeof(buffer))) > 0) {  
        fwrite(buffer, 1, bytes_read, out);  
    }  
      
    // 关闭BIO和文件  
    BIO_free_all(bio);  
    fclose(in);  
    fclose(out);  
      
    return 1;  
}

/**  
 * 使用EVP_EncodeUpdate进行流式Base64编码  
 * @param data 需要编码的数据  
 * @param data_len 数据长度  
 * @param output_file 输出文件  
 * @return 成功返回1，失败返回0  
 */  
int base64_encode_stream(const unsigned char *data, size_t data_len, FILE *output_file) {  
    EVP_ENCODE_CTX *ctx = EVP_ENCODE_CTX_new();  
    unsigned char out_buf[1024];  
    int out_len;  
      
    // 初始化编码上下文  
    EVP_EncodeInit(ctx);  
      
    // 编码数据  
    EVP_EncodeUpdate(ctx, out_buf, &out_len, data, data_len);  
    fwrite(out_buf, 1, out_len, output_file);  
      
    // 完成编码  
    EVP_EncodeFinal(ctx, out_buf, &out_len);  
    fwrite(out_buf, 1, out_len, output_file);  
      
    // 清理  
    EVP_ENCODE_CTX_free(ctx);  
      
    return 1;  
}  
  
/**  
 * 使用EVP_DecodeUpdate进行流式Base64解码  
 * @param data 需要解码的数据  
 * @param data_len 数据长度  
 * @param output_file 输出文件  
 * @return 成功返回1，失败返回0  
 */  
int base64_decode_stream(const char *data, size_t data_len, FILE *output_file) {  
    EVP_ENCODE_CTX *ctx = EVP_ENCODE_CTX_new();  
    unsigned char out_buf[1024];  
    int out_len;  
      
    // 初始化解码上下文  
    EVP_DecodeInit(ctx); 

    // 解码数据  
    if (EVP_DecodeUpdate(ctx, out_buf, &out_len, (const unsigned char *)data, data_len) < 0) {  
        EVP_ENCODE_CTX_free(ctx);  
        return 0;  
    }  
    fwrite(out_buf, 1, out_len, output_file);  
      
    // 完成解码  
    if (EVP_DecodeFinal(ctx, out_buf, &out_len) < 0) {  
        EVP_ENCODE_CTX_free(ctx);  
        return 0;  
    }  
    fwrite(out_buf, 1, out_len, output_file);  
      
    // 清理  
    EVP_ENCODE_CTX_free(ctx);  
      
    return 1;  
}

#if 0
#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
  
int main() {  
    // 测试数据  
    const char *test_data = "Hello, OpenSSL Base64!";  
    size_t test_data_len = strlen(test_data);  
      
    printf("原始数据: %s\n", test_data);  
      
    // 使用BIO方法进行编码  
    char *encoded = NULL;  
    size_t encoded_len = 0;  
      
    if (!base64_encode((const unsigned char *)test_data, test_data_len,   
                       &encoded, &encoded_len)) {  
        fprintf(stderr, "Base64编码失败\n");  
        return 1;  
    }  
      
    printf("Base64编码 (BIO方法): %s\n", encoded);  
      
    // 使用BIO方法进行解码  
    unsigned char *decoded = NULL;  
    size_t decoded_len = 0;  
      
    if (!base64_decode(encoded, encoded_len, &decoded, &decoded_len)) {  
        fprintf(stderr, "Base64解码失败\n");  
        free(encoded);  
        return 1;  
    }  
      
    printf("Base64解码 (BIO方法): %s\n", decoded);  
      
    // 使用Block方法进行编码  
    size_t max_encoded_len = ((test_data_len + 2) / 3) * 4 + 1; // 计算编码后的最大长度  
    char *block_encoded = (char *)malloc(max_encoded_len);  
      
    int block_encoded_len = base64_encode_block((const unsigned char *)test_data,   
                                               test_data_len, block_encoded);  
    block_encoded[block_encoded_len] = '\0';  
      
    printf("Base64编码 (Block方法): %s\n", block_encoded);  
      
    // 使用Block方法进行解码  
    size_t max_decoded_len = (strlen(block_encoded) / 4) * 3 + 1;  
    unsigned char *block_decoded = (unsigned char *)malloc(max_decoded_len);  
      
    int block_decoded_len = base64_decode_block(block_encoded, strlen(block_encoded),   
                                               block_decoded);  
    if (block_decoded_len >= 0) {  
        block_decoded[block_decoded_len] = '\0';  
        printf("Base64解码 (Block方法): %s\n", block_decoded);  
    } else {  
        printf("Base64解码失败 (Block方法)\n");  
    }  
      
    // 清理资源  
    free(encoded);  
    free(decoded);  
    free(block_encoded);  
    free(block_decoded);  
      
    return 0;  
}

#else
#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <openssl/evp.h>  
#include <openssl/bio.h>  
#include <openssl/buffer.h>  
  
// 包含前面定义的所有函数  
  
int main(int argc, char *argv[]) {  
    if (argc < 2) {  
        printf("用法: %s <操作> [参数...]\n", argv[0]);  
        printf("操作:\n");  
        printf("  encode <字符串>       - 编码字符串\n");  
        printf("  decode <字符串>       - 解码字符串\n");  
        printf("  encode_file <输入文件> <输出文件> - 编码文件\n");  
        printf("  decode_file <输入文件> <输出文件> - 解码文件\n");  
        return 1;  
    }  
  
    if (strcmp(argv[1], "encode") == 0) {  
        if (argc < 3) {  
            printf("错误: 缺少要编码的字符串\n");  
            return 1;  
        }  
          
        char *encoded = NULL;  
        size_t encoded_len = 0;  
          
        if (base64_encode((const unsigned char *)argv[2], strlen(argv[2]),   
                          &encoded, &encoded_len)) {  
            printf("编码结果: %s\n", encoded);  
            free(encoded);  
        } else {  
            printf("编码失败\n");  
            return 1;  
        }  
    }   
    else if (strcmp(argv[1], "decode") == 0) {  
        if (argc < 3) {  
            printf("错误: 缺少要解码的字符串\n");  
            return 1;  
        }  
          
        unsigned char *decoded = NULL;  
        size_t decoded_len = 0;  
          
        if (base64_decode(argv[2], strlen(argv[2]), &decoded, &decoded_len)) {  
            printf("解码结果: %s\n", decoded);  
            free(decoded);  
        } else {  
            printf("解码失败\n");  
            return 1;  
        }  
    }  
    else if (strcmp(argv[1], "encode_file") == 0) {  
        if (argc < 4) {  
            printf("错误: 缺少输入或输出文件名\n");  
            return 1;  
        }  
          
        if (base64_encode_file(argv[2], argv[3])) {  
            printf("文件编码成功\n");  
        } else {  
            printf("文件编码失败\n");  
            return 1;  
        }  
    }  
    else if (strcmp(argv[1], "decode_file") == 0) {  
        if (argc < 4) {  
            printf("错误: 缺少输入或输出文件名\n");  
            return 1;  
        }  
          
        if (base64_decode_file(argv[2], argv[3])) {  
            printf("文件解码成功\n");  
        } else {  
            printf("文件解码失败\n");  
            return 1;  
        }  
    }  
    else {  
        printf("未知操作: %s\n", argv[1]);  
        return 1;  
    }  
      
    return 0;  
}
#endif
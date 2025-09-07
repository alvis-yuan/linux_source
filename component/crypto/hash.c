#include <openssl/evp.h>  
#include <openssl/err.h>  
#include <string.h>  
  
/**  
 * 通用哈希函数  
 * @param data 需要计算哈希的数据  
 * @param data_len 数据长度  
 * @param hash_type 哈希算法类型 (如 "sha256", "md5" 等)  
 * @param digest 输出的哈希值  
 * @param digest_len 输出的哈希值长度  
 * @return 成功返回1，失败返回0  
 */  
int generic_hash(const unsigned char *data, size_t data_len,  
                 const char *hash_type,  
                 unsigned char *digest, unsigned int *digest_len) {  
    EVP_MD_CTX *mdctx;  
    const EVP_MD *md;  
    int ret = 0;  
  
    /* 初始化OpenSSL库 */  
    OpenSSL_add_all_digests();  
  
    /* 根据传入的算法类型获取对应的哈希算法 */  
    md = EVP_get_digestbyname(hash_type);  
    if (md == NULL) {  
        return 0;  
    }  
  
    /* 创建并初始化上下文 */  
    if ((mdctx = EVP_MD_CTX_new()) == NULL) {  
        return 0;  
    }  
  
    /* 初始化哈希操作 */  
    if (1 != EVP_DigestInit_ex(mdctx, md, NULL)) {  
        goto cleanup;  
    }  
  
    /* 提供数据 */  
    if (1 != EVP_DigestUpdate(mdctx, data, data_len)) {  
        goto cleanup;  
    }  
  
    /* 完成哈希操作并获取结果 */  
    if (1 != EVP_DigestFinal_ex(mdctx, digest, digest_len)) {  
        goto cleanup;  
    }  
  
    ret = 1;  
  
cleanup:  
    /* 清理 */  
    EVP_MD_CTX_free(mdctx);  
    return ret;  
}  
  
/**  
 * 通用哈希文件函数  
 * @param filename 需要计算哈希的文件名  
 * @param hash_type 哈希算法类型 (如 "sha256", "md5" 等)  
 * @param digest 输出的哈希值  
 * @param digest_len 输出的哈希值长度  
 * @return 成功返回1，失败返回0  
 */  
int generic_hash_file(const char *filename, const char *hash_type,  
                      unsigned char *digest, unsigned int *digest_len) {  
    FILE *file;  
    EVP_MD_CTX *mdctx;  
    const EVP_MD *md;  
    unsigned char buffer[4096];  
    size_t bytes_read;  
    int ret = 0;  
  
    /* 初始化OpenSSL库 */  
    OpenSSL_add_all_digests();  
  
    /* 根据传入的算法类型获取对应的哈希算法 */  
    md = EVP_get_digestbyname(hash_type);  
    if (md == NULL) {  
        return 0;  
    }  
  
    /* 打开文件 */  
    file = fopen(filename, "rb");  
    if (file == NULL) {  
        return 0;  
    }  
  
    /* 创建并初始化上下文 */  
    if ((mdctx = EVP_MD_CTX_new()) == NULL) {  
        fclose(file);  
        return 0;  
    }  
  
    /* 初始化哈希操作 */  
    if (1 != EVP_DigestInit_ex(mdctx, md, NULL)) {  
        goto cleanup;  
    }  
  
    /* 读取文件并更新哈希 */  
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {  
        if (1 != EVP_DigestUpdate(mdctx, buffer, bytes_read)) {  
            goto cleanup;  
        }  
    }  
  
    /* 完成哈希操作并获取结果 */  
    if (1 != EVP_DigestFinal_ex(mdctx, digest, digest_len)) {  
        goto cleanup;  
    }  
  
    ret = 1;  
  
cleanup:  
    /* 清理 */  
    EVP_MD_CTX_free(mdctx);  
    fclose(file);  
    return ret;  
}

/**  
 * 通用HMAC函数  
 * @param data 需要计算HMAC的数据  
 * @param data_len 数据长度  
 * @param key 密钥  
 * @param key_len 密钥长度  
 * @param hash_type 哈希算法类型 (如 "sha256", "md5" 等)  
 * @param hmac 输出的HMAC值  
 * @param hmac_len 输出的HMAC值长度  
 * @return 成功返回1，失败返回0  
 */  
int generic_hmac(const unsigned char *data, size_t data_len,  
                 const unsigned char *key, size_t key_len,  
                 const char *hash_type,  
                 unsigned char *hmac, unsigned int *hmac_len) {  
    HMAC_CTX *ctx;  
    const EVP_MD *md;  
    int ret = 0;  
  
    /* 初始化OpenSSL库 */  
    OpenSSL_add_all_digests();  
  
    /* 根据传入的算法类型获取对应的哈希算法 */  
    md = EVP_get_digestbyname(hash_type);  
    if (md == NULL) {  
        return 0;  
    }  
  
    /* 创建并初始化上下文 */  
    if ((ctx = HMAC_CTX_new()) == NULL) {  
        return 0;  
    }  
  
    /* 初始化HMAC操作 */  
    if (1 != HMAC_Init_ex(ctx, key, key_len, md, NULL)) {  
        goto cleanup;  
    }  
  
    /* 提供数据 */  
    if (1 != HMAC_Update(ctx, data, data_len)) {  
        goto cleanup;  
    }  
  
    /* 完成HMAC操作并获取结果 */  
    if (1 != HMAC_Final(ctx, hmac, hmac_len)) {  
        goto cleanup;  
    }  
  
    ret = 1;  
  
cleanup:  
    /* 清理 */  
    HMAC_CTX_free(ctx);  
    return ret;  
}

/**  
 * 打印所有支持的哈希算法  
 */  
void print_supported_hash_algorithms() {  
    OpenSSL_add_all_digests();  
      
    EVP_MD_do_all_sorted(print_md_fn, NULL);  
      
    EVP_cleanup();  
}  
  
/**  
 * 打印单个哈希算法的回调函数  
 */  
static void print_md_fn(const EVP_MD *md, const char *from, const char *to, void *arg) {  
    printf("%s\n", EVP_MD_name(md));  
}

#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <openssl/evp.h>  
#include <openssl/err.h>  
  
void print_hex(const unsigned char *data, unsigned int len) {  
    for (unsigned int i = 0; i < len; i++) {  
        printf("%02x", data[i]);  
    }  
    printf("\n");  
}  
  
void handle_errors() {  
    ERR_print_errors_fp(stderr);  
    abort();  
}  
  
int main() {  
    /* 测试数据 */  
    unsigned char data[] = "这是一段需要计算哈希的测试文本";  
    unsigned char digest[EVP_MAX_MD_SIZE];  
    unsigned int digest_len;  
      
    /* 使用SHA-256计算哈希 */  
    if (!generic_hash(data, strlen((char*)data), "sha256", digest, &digest_len)) {  
        handle_errors();  
    }  
      
    printf("SHA-256哈希值: ");  
    print_hex(digest, digest_len);  
      
    /* 使用MD5计算哈希 */  
    if (!generic_hash(data, strlen((char*)data), "md5", digest, &digest_len)) {  
        handle_errors();  
    }  
      
    printf("MD5哈希值: ");  
    print_hex(digest, digest_len);  
      
    /* 使用SHA-1计算哈希 */  
    if (!generic_hash(data, strlen((char*)data), "sha1", digest, &digest_len)) {  
        handle_errors();  
    }  
      
    printf("SHA-1哈希值: ");  
    print_hex(digest, digest_len);  
      
    /* 计算文件哈希 */  
    const char *filename = "test.txt";  
    printf("\n计算文件 %s 的哈希值:\n", filename);  
      
    if (!generic_hash_file(filename, "sha256", digest, &digest_len)) {  
        printf("无法计算文件哈希，请确保文件存在\n");  
    } else {  
        printf("文件SHA-256哈希值: ");  
        print_hex(digest, digest_len);  
    }  
      
    /* 清理 */  
    EVP_cleanup();  
    ERR_free_strings();  
      
    return 0;  
}
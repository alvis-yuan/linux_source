#include <openssl/evp.h>  
#include <openssl/err.h>  
#include <string.h>  
  
/**  
 * 通用对称加密函数  
 * @param plaintext 明文数据  
 * @param plaintext_len 明文长度  
 * @param key 密钥  
 * @param key_len 密钥长度  
 * @param iv 初始化向量  
 * @param iv_len 初始化向量长度  
 * @param cipher_type 加密算法类型 (如 "aes-256-cbc", "des-ede3-cbc" 等)  
 * @param ciphertext 输出的密文  
 * @param ciphertext_len 输出的密文长度  
 * @return 成功返回1，失败返回0  
 */  
int symmetric_encrypt(const unsigned char *plaintext, int plaintext_len,  
                      const unsigned char *key, int key_len,  
                      const unsigned char *iv, int iv_len,  
                      const char *cipher_type,  
                      unsigned char *ciphertext, int *ciphertext_len) {  
    EVP_CIPHER_CTX *ctx;  
    const EVP_CIPHER *cipher;  
    int len;  
    int ret = 0;  
  
    /* 创建并初始化上下文 */  
    if (!(ctx = EVP_CIPHER_CTX_new()))  
        return 0;  
  
    /* 根据传入的算法类型获取对应的cipher */  
    cipher = EVP_get_cipherbyname(cipher_type);  
    if (!cipher) {  
        EVP_CIPHER_CTX_free(ctx);  
        return 0;  
    }  
  
    /* 初始化加密操作 */  
    if (1 != EVP_EncryptInit_ex(ctx, cipher, NULL, key, iv)) {  
        EVP_CIPHER_CTX_free(ctx);  
        return 0;  
    }  
  
    /* 提供明文并获取密文 */  
    if (1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len)) {  
        EVP_CIPHER_CTX_free(ctx);  
        return 0;  
    }  
    *ciphertext_len = len;  
  
    /* 完成加密操作 */  
    if (1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len)) {  
        EVP_CIPHER_CTX_free(ctx);  
        return 0;  
    }  
    *ciphertext_len += len;  
  
    /* 清理 */  
    EVP_CIPHER_CTX_free(ctx);  
    return 1;  
}  
  
/**  
 * 通用对称解密函数  
 * @param ciphertext 密文数据  
 * @param ciphertext_len 密文长度  
 * @param key 密钥  
 * @param key_len 密钥长度  
 * @param iv 初始化向量  
 * @param iv_len 初始化向量长度  
 * @param cipher_type 加密算法类型 (如 "aes-256-cbc", "des-ede3-cbc" 等)  
 * @param plaintext 输出的明文  
 * @param plaintext_len 输出的明文长度  
 * @return 成功返回1，失败返回0  
 */  
int symmetric_decrypt(const unsigned char *ciphertext, int ciphertext_len,  
                      const unsigned char *key, int key_len,  
                      const unsigned char *iv, int iv_len,  
                      const char *cipher_type,  
                      unsigned char *plaintext, int *plaintext_len) {  
    EVP_CIPHER_CTX *ctx;  
    const EVP_CIPHER *cipher;  
    int len;  
    int ret = 0;  
  
    /* 创建并初始化上下文 */  
    if (!(ctx = EVP_CIPHER_CTX_new()))  
        return 0;  
  
    /* 根据传入的算法类型获取对应的cipher */  
    cipher = EVP_get_cipherbyname(cipher_type);  
    if (!cipher) {  
        EVP_CIPHER_CTX_free(ctx);  
        return 0;  
    }  
  
    /* 初始化解密操作 */  
    if (1 != EVP_DecryptInit_ex(ctx, cipher, NULL, key, iv)) {  
        EVP_CIPHER_CTX_free(ctx);  
        return 0;  
    }  
  
    /* 提供密文并获取明文 */  
    if (1 != EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len)) {  
        EVP_CIPHER_CTX_free(ctx);  
        return 0;  
    }  
    *plaintext_len = len;  
  
    /* 完成解密操作 */  
    if (1 != EVP_DecryptFinal_ex(ctx, plaintext + len, &len)) {  
        EVP_CIPHER_CTX_free(ctx);  
        return 0;  
    }  
    *plaintext_len += len;  
  
    /* 清理 */  
    EVP_CIPHER_CTX_free(ctx);  
    return 1;  
}


#include <stdio.h>  
#include <stdlib.h>  
#include <string.h>  
#include <openssl/evp.h>  
#include <openssl/err.h>  
  
void print_hex(const unsigned char *data, int len) {  
    for (int i = 0; i < len; i++) {  
        printf("%02x", data[i]);  
    }  
    printf("\n");  
}  
  
void handle_errors() {  
    ERR_print_errors_fp(stderr);  
    abort();  
}  
  
int main() {  
    /* 初始化OpenSSL库 */  
    OpenSSL_add_all_algorithms();  
    ERR_load_crypto_strings();  
  
    /* 测试数据 */  
    unsigned char plaintext[] = "这是一段需要加密的测试文本";  
    unsigned char key[32];  
    unsigned char iv[16];  
    unsigned char ciphertext[1024];  
    unsigned char decrypted[1024];  
    int ciphertext_len, decrypted_len;  
      
    /* 生成随机密钥和IV */  
    RAND_bytes(key, sizeof(key));  
    RAND_bytes(iv, sizeof(iv));  
      
    /* 使用AES-256-CBC加密 */  
    if (!symmetric_encrypt(plaintext, strlen((char*)plaintext),  
                          key, sizeof(key),  
                          iv, sizeof(iv),  
                          "aes-256-cbc",  
                          ciphertext, &ciphertext_len)) {  
        handle_errors();  
    }  
      
    printf("加密后的密文: ");  
    print_hex(ciphertext, ciphertext_len);  
      
    /* 解密 */  
    if (!symmetric_decrypt(ciphertext, ciphertext_len,  
                          key, sizeof(key),  
                          iv, sizeof(iv),  
                          "aes-256-cbc",  
                          decrypted, &decrypted_len)) {  
        handle_errors();  
    }  
      
    /* 添加字符串结束符 */  
    decrypted[decrypted_len] = '\0';  
    printf("解密后的明文: %s\n", decrypted);  
      
    /* 使用不同的算法 - 3DES */  
    if (!symmetric_encrypt(plaintext, strlen((char*)plaintext),  
                          key, 24, /* 3DES使用24字节密钥 */  
                          iv, 8,   /* 3DES使用8字节IV */  
                          "des-ede3-cbc",  
                          ciphertext, &ciphertext_len)) {  
        handle_errors();  
    }  
      
    printf("使用3DES加密后的密文: ");  
    print_hex(ciphertext, ciphertext_len);  
      
    /* 解密 */  
    if (!symmetric_decrypt(ciphertext, ciphertext_len,  
                          key, sizeof(key),  
                          iv, sizeof(iv),  
                          "des-ede3-cbc",  
                          decrypted, &decrypted_len)) {  
        handle_errors();  
    }  
    /* 添加字符串结束符 */  
    decrypted[decrypted_len] = '\0';  
    printf("解密后的明文: %s\n", decrypted); 
 
    /* 清理 */  
    EVP_cleanup();  
    ERR_free_strings();  
      
    return 0;  
}
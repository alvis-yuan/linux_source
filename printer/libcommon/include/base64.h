#ifndef __BASE64_H__
#define __BASE64_H__

enum {
    BASE64_STD = 0,
    BASE64_URL
};

int base64_encode(const void *source,int len,char *out,int type);
int base64_decode(const char *source,void *out,int type);

#endif

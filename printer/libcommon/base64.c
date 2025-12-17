/****************************************************************************
* FILE NAME:   bas64.c                                                      *
* MODULE NAME: BASE64                                                       *
* PROGRAMMER:                                                               *
* DESCRIPTION:                                                              *
* REVISION:                                                                 *
****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

static const char *BASE64_CHARSETS[2] = {
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
};

int base64_encode(const void *source, int len, char *out, int type)
{
    int i;
    const char *b64chars = BASE64_CHARSETS[type];
    const unsigned char *src = (unsigned char *)source;
    char *dst = out;

    for( i=0;i<len;i+=3 )
    {
        unsigned char idx = src[i]>>2;
        *dst++ = b64chars[idx];
        idx = (src[i]<<4)&0x30;
        if ( i + 1 >= len )
        {
            *dst++ = b64chars[idx];
            *dst++ = '=';
            *dst++ = '=';
            break;
        }
        idx |= (src[i+1]>>4);
        *dst++ = b64chars[idx];
        idx = (src[i+1]<<2)&0x3c;
        if ( i + 2 >= len )
        {
            *dst++ = b64chars[idx];
            *dst++ = '=';
            break;
        }
        idx |= ((src[i+2]>>6)&0x03);
        *dst++ = b64chars[idx];
        *dst++ = b64chars[src[i+2]&0x3f];
    }
    *dst = 0;
    return dst-out;
}

int base64_decode(const char *src,void *dst, int type)
{
    const char *b64chars = BASE64_CHARSETS[type],*p,*s = src;
    unsigned char *d = (unsigned char *)dst;
    int i,n,len=strlen(src);
    for ( i = 0; i<len ; i += 4 )
    {
        unsigned char t[4] = {0xff,0xff,0xff,0xff};
        for(n=0;n<4;n++) {
            if( i+n>=len || s[i+n] == '=' ) break;
            p = strchr(b64chars,s[i+n]);
            if( p )	t[n] = p-b64chars;
        }
        *d++ = (((t[0]<<2))&0xFC) | ((t[1]>>4)&0x03);
        if ( s[i+2] == '=' ) break;
        *d++ = (((t[1]<<4))&0xF0) |  ((t[2]>>2)&0x0F);
        if ( s[i+3] == '=' ) break;
        *d++ = (((t[2]<<6))&0xF0) | (t[3]&0x3F);
    }
    return d-(unsigned char *)dst;
}


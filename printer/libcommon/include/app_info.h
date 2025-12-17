#ifndef __APP_INFO_H__
#define __APP_INFO_H__

enum
{
    PACKAGE_TYPE_FW,
    PACKAGE_TYPE_HOMEFS,
    PACKAGE_TYPE_RESOURCE,
    PACKAGE_TYPE_MIXED,
    PACKAGE_TYPE_MAX,
};

typedef struct
{
    char name[64];
    int type;
    char version[32];
    char model[32];
} app_info_t;

int parse_appinfo_zipfile(const char *zipfile, app_info_t *info);

#endif /* __APP_INFO_H__ */
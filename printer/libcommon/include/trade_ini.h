#ifndef __TRADE_INI_H__
#define __TRADE_INI_H__

char *trade_ini_get(const char *key,char *value,int value_len);
int trade_ini_get_int(const char *key, int def_value);
int trade_ini_set(const char *key, const char *value);
int trade_ini_set_int(const char *key, int value);

#define TRADE_KEY_VEN  "vendor"
#define TRADE_KEY_BID  "bind"
#define TRADE_KEY_TTT  "cnt"     //audio play times
#define TRADE_KEY_TTM  "money"

#endif /* __TRADE_INI_H__ */

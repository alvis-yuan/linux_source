#ifndef __TSPL_PARSER_H__
#define __TSPL_PARSER_H__

#include <stddef.h>
#include <stdint.h>

#define TSPLPARSER_MEM(buf, len)   tspl_parser(buf, len, NULL)
#define TSPLPARSER_FILE(file)      tspl_parser(NULL, 0, file)

#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>

#include <libcommon.h>

static int match_field(char *buf, const char *key, char *value, int value_len)
{
	if (!strncmp(buf, key, strlen(key)))
	{
		char *pos = buf + strlen(key);
		while (isspace(*pos))
			pos++;
		while (strlen(pos) && isspace(pos[strlen(pos) - 1]))
			pos[strlen(pos) - 1] = 0;
		snprintf(value, value_len, "%s", pos);
		return 1;
	}
	return 0;
}

static int parse_app_info(FILE *fp, app_info_t *appinfo)
{
	char stype[32]={};
	char buf[256] = {};

	memset(appinfo, 0, sizeof(*appinfo));
	while (fgets(buf, sizeof(buf), fp))
	{
		if (match_field(buf, "name:", appinfo->name, sizeof(appinfo->name)) ||
			match_field(buf, "type:", stype, sizeof(stype)) ||
			match_field(buf, "version:", appinfo->version, sizeof(appinfo->version)) ||
			match_field(buf, "model:", appinfo->model, sizeof(appinfo->model)))
			;
	}
	if ( !strcasecmp(stype, "homefs"))
		appinfo->type = PACKAGE_TYPE_HOMEFS;
	else if (!strcasecmp(stype, "resource"))
		appinfo->type = PACKAGE_TYPE_RESOURCE;
	else if (!strcasecmp(stype, "firmware"))
		appinfo->type = PACKAGE_TYPE_FW;
	else if (!strcasecmp(stype, "mixed"))
		appinfo->type = PACKAGE_TYPE_MIXED;
	else {
		LogError("type:%s not support!",stype);
		return -1;
	}
	return 0;
}

int parse_appinfo_zipfile(const char *zipfile, app_info_t *info)
{
	char cmd[256];

	snprintf(cmd, sizeof(cmd), "unzip -p %s `info=$(unzip -l %s"
							   "|grep APP/APPINFO$)&&echo ${info##* }`",
			 zipfile, zipfile);
	FILE *fp = popen(cmd, "r");
	if (!fp)
		return -1;

	int ret = parse_app_info(fp, info);
	pclose(fp);
	return ret;
}

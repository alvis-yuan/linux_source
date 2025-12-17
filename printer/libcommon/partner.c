#include <libcommon.h>
#include <ctype.h>


#define VALUE_IN_SHAREMEM(KEY)       sys_global_var()->KEY
#define PARTNER_PLATFORM_PARAM_FILE  "/data/PUB/partnerParam"    //第三方合作伙伴配置参数文件

static int _match_field(char *buf,const char *key,char *value,int value_len)
{
	if( !strncmp(buf,key,strlen(key)) ) {
		char *pos = buf + strlen(key);
		while( isspace(*pos) ) pos++;
		while( strlen(pos) && isspace(pos[strlen(pos)-1]) ) pos[strlen(pos)-1] = 0;
		snprintf(value,value_len,"%s",pos);
		return 1;
	}
	return 0;
}

int GetPartnerParam(T_PartnerParam *parterParam)
{
	char buff[256];
	memset(parterParam, 0, sizeof(T_PartnerParam));
	FILE *fp = fopen(PARTNER_PLATFORM_PARAM_FILE, "r");

	if (!fp) return -1;

	while( fgets(buff, sizeof(buff), fp) ) {
		if( _match_field(buff,"appNo:",    parterParam->appNo,    sizeof(parterParam->appNo))  ||
		    _match_field(buff,"appID:",    parterParam->appID,    sizeof(parterParam->appID))  ||
		    _match_field(buff,"appKey:",   parterParam->appKey,   sizeof(parterParam->appKey)) ||
		    _match_field(buff,"parterURL:",parterParam->parterURL,sizeof(parterParam->parterURL))
		);
	}
	fclose(fp);
	strcpy(parterParam->model,         VALUE_IN_SHAREMEM(project));
	strcpy(parterParam->msn,           VALUE_IN_SHAREMEM(sn));
	strcpy(parterParam->hardwareModel, VALUE_IN_SHAREMEM(model));

	LogDbg("appNo=%s, appID=%s, appKey=%s, parterURL=%s, model=%s, msn=%s, hardwareModel=%s",
			parterParam->appNo, parterParam->appID, parterParam->appKey,
			parterParam->parterURL, parterParam->model, parterParam->msn, parterParam->hardwareModel);

	return 0;
}

int SavePartnerParams(const char *appNo,const char *appID,const char *appKey,const char *url)
{
	FILE *fp = fopen(PARTNER_PLATFORM_PARAM_FILE, "w");
	if( !fp ) {
		LogError("fopen(PARTNER_PLATFORM_PARAM_FILE) error(%d),%s",errno,strerror(errno));
		return -1;
	}

	fprintf(fp,"appNo:%s\n", appNo);
	fprintf(fp,"appID:%s\n", appID);
	fprintf(fp,"appKey:%s\n", appKey);
	fprintf(fp,"parterURL:%s\n", url);
	fclose(fp);

	return 0;
}
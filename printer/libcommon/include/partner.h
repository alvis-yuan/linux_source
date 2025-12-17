#ifndef __PARTNER_H__
#define __PARTNER_H__

typedef struct {
	char appNo[64];       //第三方在商米申请注册的应用编号
	char appID[64];       //第三方在商米注册用于生成签名的appId
	char appKey[64];      //第三方在商米注册用于生成签名的appKey
	char parterURL[256];  //第三方提供的请求订单相关数据的url地址

	char msn[64];
	char model[64];
	char hardwareModel[64];
}T_PartnerParam, *PT_PartnerParam;

int GetPartnerParam(T_PartnerParam *parterParam);
int SavePartnerParams(const char *appNo,const char *appID,const char *appKey,const char *url);

#endif

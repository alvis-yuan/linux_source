#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <ctype.h>
#include "zip_utils.h"
#include <openssl/md5.h>
#include <libgen.h>

#define dw_dir "./fw/"
#define VERSION_FILE "version.txt"
#define VER_LEN	 64
#define REL_PATH_LEN (1024 - 256)
#define ABS_PATH_LEN (1024)
#define MAX_LINE_LEN 256

static	bool mixed_is_4g = false;

static const char *fw_inner_files[] = {
	"PrinterImage/",
	"PrinterImage/APP/",
	"PrinterImage/APP/APPINFO",
	"PrinterImage/APP/rootfs.squashfs",
	"PrinterImage/APP/rootfs.squashfs.md5file",
	"PrinterImage/APP/xImage",
	"PrinterImage/APP/xImage.md5file",
};


static const char *app_inner_files[] = {
	"app_package/",
	"app_package/APP/",
	"app_package/APP/APPINFO",
	"app_package/APP/homefs.squashfs",
	"app_package/APP/homefs.squashfs.md5file",
};

static const char *resource_inner_files[] = {
	"app_package/",
	"app_package/APP/",
	"app_package/APP/APPINFO",
	"app_package/RESOURCE/",
	"app_package/RESOURCE/mixed_version",
	"app_package/RESOURCE/postinstall.sh",
	"app_package/RESOURCE/preinstall.sh",
};
#define LTE_DIR "app_package/RESOURCE/wnet_fota/"

static	char firmware_zipfile[REL_PATH_LEN];
static	char application_zipfile[REL_PATH_LEN];
static	char resource_zipfile[REL_PATH_LEN];

static char *abs_path = NULL;

static int util_md5file(const char *file, char md5[33])
{
	int len = 0;
	char buf[1024 * 4];
	unsigned char md5buf[16]={};

	FILE *fp = fopen(file, "rb");
	if (!fp)
		return -1;
	MD5_CTX context;
	MD5_Init(&context);
	while ((len = fread(buf, 1, sizeof(buf), fp)) > 0)
		MD5_Update(&context, buf, len);
	MD5_Final(md5buf, &context);
	fclose(fp);
	for(len=0;len<16;len++)
		snprintf(md5+len*2,3,"%02x",md5buf[len]);
	return 0;
}

static inline char *get_abs_path(const char *rel_path)
{
	if (rel_path && rel_path[0] == '/') {
		return (char *)rel_path;
	}

	if (abs_path == NULL) {
		abs_path = malloc(ABS_PATH_LEN);
		if (abs_path == NULL) {
			LogError("malloc failed");
			return NULL;
		}
	}
	snprintf(abs_path, ABS_PATH_LEN, "%s/%s", dw_dir, rel_path);

	return abs_path;
}

static inline void put_abs_path(void)
{
	if (abs_path) {
		free(abs_path);
		abs_path = NULL;
	}
}

static void release_file(const char *file)
{
	if (!access(get_abs_path(file), F_OK)) {
		unlink(get_abs_path(file));
	}

	put_abs_path();
}

static int match_field(char *buf, const char *key, char *value, int value_len)
{
	if (!strncmp(buf, key, strlen(key))) {
		char *pos = buf + strlen(key);
		while (isspace(*pos)) {
			pos++;
		}
		while (strlen(pos) && (isspace(pos[strlen(pos) - 1]) || pos[strlen(pos) - 1] == ';')) {
			pos[strlen(pos) - 1] = 0;
		}
		snprintf(value, value_len, "%s", pos);

		return 1;
	}
	return 0;
}

static int _validate_md5(const char *zipfile, const char *fname)
{
	char md5file[REL_PATH_LEN] = { };
	char md5_cxt[64] = { };
	char md5txt[64] = { };
	int ret = -1;

	snprintf(md5file, sizeof(md5file), "%s.md5file", fname);

	// 提取md5文件到内存
	ret = unzip_extract_to_buf(get_abs_path(zipfile), md5file, md5_cxt, sizeof(md5_cxt));
	if (ret < 0) {
		LogError("unzip %s failed", md5file);
		goto out;
	}
	LogInfo("%s md5: %s", fname, md5_cxt);

	// 提取指定文件 fname
	ret = unzip_extract_to_file(get_abs_path(zipfile), dw_dir, fname, NULL);
	if (ret < 0) {
		LogError("unzip %s failed", fname);
		goto out;
	}

	if (util_md5file(get_abs_path(basename((char *)fname)), md5txt) < 0 || strcmp(md5txt, md5_cxt)) {
		LogError("%s md5 check fail", get_abs_path(basename((char *)fname)));
		goto out;
	}

	ret = 0;
out:
	release_file(basename((char *)fname));
	return ret;
}

static int get_outer_version(char *zipfile, char *version, int len)
{
	int ret = -1;
	FILE *fp = NULL;
	char buf[MAX_LINE_LEN] = { };

    ret = unzip_extract_to_file(zipfile, dw_dir, VERSION_FILE, NULL);  
	if (ret < 0) {
		LogError("unzip %s failed", VERSION_FILE);
		return -1;
	}

	fp = fopen(get_abs_path(VERSION_FILE), "r");
	if (!fp) {
		LogError("popen failed %s", strerror(errno));
		goto out;
	}

	while (fgets(buf, sizeof(buf), fp)) {
		if (match_field(buf, "version=", version, len)) {
			LogInfo("outer version: %s", version);
			ret = 0;
			break;
		}
	}

out:
	release_file(VERSION_FILE);
	if (fp) {
		fclose(fp);
	}
	return ret;
}

static int get_appinfo_version(char *zipfile, const char *verfile, char *version, int len)
{
	int ret = -1;
	FILE *fp = NULL;
	char buf[MAX_LINE_LEN] = { };

	ret = unzip_extract_to_file(get_abs_path(zipfile), dw_dir, verfile, NULL);  
	if (ret < 0) {
		LogError("unzip %s failed", verfile);
		return -1;
	}

	fp = fopen(get_abs_path("APPINFO"), "r");
	if (!fp) {
		LogError("popen failed %s", strerror(errno));
		goto out;
	}

	while (fgets(buf, sizeof(buf), fp)) {
		if (match_field(buf, "version:", version, len)) {
			LogInfo("inner version: %s", version);
			ret = 0;
			break;
		}
	}

out:
	release_file("APPINFO");
	if (fp) {
		fclose(fp);
	}
	return ret;	
}

static int validate_file_list(char *file, const char *inner_files[], int num)
{
	struct unzip_files_info *files_info = NULL;
	int num_files = 0;
	int ret = -1;
	int i = 0, j = 0;

	/* 获取ZIP文件信息 */  
    num_files = unzip_get_files_info(get_abs_path(file), &files_info);  
    if (num_files == 0) {  
        LogError("Failed to get information from %s or the file is empty", file);  
		goto out;
    }

	if (num_files != num) {
		LogError("zip file has %d files, not %d", num_files, num);
		goto out;
	}

	// 查看文件名
	for (i = 0; i < num; i++) {
		for (j = 0; j < num_files; j++) {
			if (strcmp(files_info[j].filename, inner_files[i]) == 0) {
				break;
			}
		}
		if (j == num_files) {
			LogError("file %s no exist", inner_files[i]);
			goto out;
		}
	}

	// 判断是否是4G升级包
	for (i = 0; i < num_files; i++) {
		if (strcmp(files_info[i].filename, LTE_DIR) == 0) {
			mixed_is_4g = true;
		}
	}

	ret = 0;
out:
	put_abs_path();
	if (files_info) {
		free(files_info);
	}
	return ret;
}

static int extract_inner_zipfile(char *outfile, char *type)
{
	char *zipfile = NULL;

	if (strcmp(type, "firmware") == 0) {
		zipfile = firmware_zipfile;
	} else if (strcmp(type, "application") == 0) {
		zipfile = application_zipfile;
	} else if (strcmp(type, "resource") == 0) {
		zipfile = resource_zipfile;
	}

	if (unzip_extract_to_file(outfile, dw_dir, zipfile, NULL) < 0) {
		LogError("unzip %s failed", outfile);
		return -1;
	}

	return 0;
}

static int validate_outer_file(char *file, char *type)
{
	struct unzip_files_info *files_info = NULL;
	int num_files = 0;
	int ret = -1;
	int i = 0;
	int inner_zipfile_index = 0;
	bool has_version = false;

    /* 获取ZIP文件信息 */  
    num_files = unzip_get_files_info(file, &files_info);  
    if (num_files == 0) {  
        LogError("Failed to get information from %s or the file is empty", file);  
		goto out;
    }

	if (num_files != 2) {
		LogError("zip file has %d files, not 2", num_files);
		goto out;
	}

	for (i = 0; i < num_files; i++) {
		if (strcmp(files_info[i].filename, "version.txt") == 0 && !files_info[i].is_dir) {
			has_version = true;
			inner_zipfile_index = !i;
			break;
		}
	}

	if (!has_version) {
		LogError("version.txt file no exist");
		goto out;
	}

	if (strcmp(type, "firmware") == 0) {
		snprintf(firmware_zipfile, sizeof(firmware_zipfile), "%s", files_info[inner_zipfile_index].filename);
	} else if (strcmp(type, "application") == 0) {
		snprintf(application_zipfile, sizeof(application_zipfile), "%s", files_info[inner_zipfile_index].filename);
	} else if (strcmp(type, "resource") == 0) {
		snprintf(resource_zipfile, sizeof(resource_zipfile), "%s", files_info[inner_zipfile_index].filename);
	}

	ret = 0;
out:
	if (files_info) {
		free(files_info);
	}
	return ret;
}

static int validate_upgrade_firmware_file(char *file)
{
	int ret = -1;
	char outer_version[VER_LEN] = { };
	char inner_version[VER_LEN] = { };

	if (validate_outer_file(file, "firmware")) {
		LogError("validate outer file failed");
		goto out;
	}

	if (extract_inner_zipfile(file, "firmware")) {
		LogError("extract inner zipfile failed");
		goto out;
	}
	LogInfo("inner zipfile %s", firmware_zipfile);

	// 校验内层文件
	ret = validate_file_list(firmware_zipfile, fw_inner_files, sizeof(fw_inner_files) / sizeof(fw_inner_files[0]));
	if (ret) {
		LogError("validate inner zipfile failed");
		goto out;
	}

	ret = get_outer_version(file, outer_version, sizeof(outer_version));
	if (ret) {
		LogError("get outer version failed");
		goto out;
	}
	ret = get_appinfo_version(firmware_zipfile, fw_inner_files[2], inner_version, sizeof(inner_version));
	if (ret) {
		LogError("get inner version failed");
		goto out;
	}
	LogInfo("outer version: %s", outer_version);
	LogInfo("inner version: %s", inner_version);
	// 校验版本号
	if (strcmp(outer_version, inner_version)) {
		LogError("inner and outer version unmatch");
		goto out;
	}

	//解压成功后，删除原始的压缩包
	unlink(file);

	ret = _validate_md5(firmware_zipfile, fw_inner_files[5]);
	if (ret != 0) {
		goto out;
	}

	ret = _validate_md5(firmware_zipfile, fw_inner_files[3]);

out:
	unlink(file);
	
	if (ret) {
		release_file(firmware_zipfile);
	}
	return ret;
}

/***************************************************/
static int validate_upgrade_application_file(char *file)
{
	int ret = -1;
	char outer_version[VER_LEN] = { };
	char inner_version[VER_LEN] = { };

	if (validate_outer_file(file, "application")) {
		LogError("validate outer file failed");
		goto out;
	}

	if (extract_inner_zipfile(file, "application")) {
		LogError("extract inner zipfile failed");
		goto out;
	}
	LogInfo("inner zipfile %s", application_zipfile);

	// 校验内层文件
	ret = validate_file_list(application_zipfile, app_inner_files, sizeof(app_inner_files) / sizeof(app_inner_files[0]));
	if (ret) {
		LogError("validate inner zipfile failed");
		goto out;
	}

	ret = get_outer_version(file, outer_version, sizeof(outer_version));
	if (ret) {
		LogError("get outer version failed");
		goto out;
	}
	ret = get_appinfo_version(application_zipfile, app_inner_files[2], inner_version, sizeof(inner_version));
	if (ret) {
		LogError("get inner version failed");
		goto out;
	}
	LogInfo("outer version: %s", outer_version);
	LogInfo("inner version: %s", inner_version);
	// 校验版本号
	if (strcmp(outer_version, inner_version)) {
		LogError("inner and outer version unmatch");
		goto out;
	}

	//解压成功后，删除原始的压缩包
	unlink(file);

	ret = _validate_md5(application_zipfile, app_inner_files[3]);

out:
	unlink(file);
	
	if (ret) {
		release_file(application_zipfile);
	}
	return ret;
}

static int get_mix_version(char *zipfile, const char *verfile, char *version, int len)
{
	char mix_ver[VER_LEN] = { };
	int ret = -1;

	// 提取版本号到 mix_ver
	ret = unzip_extract_to_buf(get_abs_path(zipfile), (char *)verfile, mix_ver, sizeof(mix_ver));
	if (ret < 0) {
		LogError("unzip %s failed", "mixed_version");
		return -1;
	}

	if (match_field(mix_ver, "mixed_version:", version, len)) {
		ret = 0;
	}
	LogInfo("mixed version: %s", mix_ver);

	put_abs_path();
	return ret;
}

static int validate_upgrade_resource_file(char *file)
{
	int ret = -1;
	char outer_version[VER_LEN] = { };
	char inner_version[VER_LEN] = { };
	char mixed_version[VER_LEN] = { };

	if (validate_outer_file(file, "resource")) {
		LogError("validate outer file failed");
		goto out;
	}

	if (extract_inner_zipfile(file, "resource")) {
		LogError("extract inner zipfile failed");
		goto out;
	}
	LogInfo("inner zipfile %s", resource_zipfile);

	// 校验内层文件
	ret = validate_file_list(resource_zipfile, resource_inner_files, sizeof(resource_inner_files) / sizeof(resource_inner_files[0]));
	if (ret) {
		LogError("validate inner zipfile failed");
		goto out;
	}

	ret = get_outer_version(file, outer_version, sizeof(outer_version));
	if (ret) {
		LogError("get outer version failed");
		goto out;
	}
	ret = get_appinfo_version(resource_zipfile, resource_inner_files[2], inner_version, sizeof(inner_version));
	if (ret) {
		LogError("get inner version failed");
		goto out;
	}
	ret = get_mix_version(resource_zipfile, resource_inner_files[4], mixed_version, sizeof(mixed_version));
	if (ret) {
		LogError("get mixed version failed");
		goto out;
	}

	// 校验版本号
	if (strcmp(outer_version, inner_version)) {
		LogError("inner and outer version unmatch");
		goto out;
	}
	if (strcmp(mixed_version, inner_version)) {
		LogError("mixed and inner version unmatch");
		goto out;
	}

out:
	unlink(file);
	
	if (ret) {
		release_file(resource_zipfile);
	}
	return ret;
}

int main(int argc, char **argv)
{
	int ret;
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <firmware_file> <type>\n", argv[0]);
		return -1;
	}

	char *file = argv[1];

	switch (atoi(argv[2])) {
		case 1:
			ret = validate_upgrade_firmware_file(file);
			if (ret) {
				printf("Firmware validation failed\n");
				return -1;
			}
			LogInfo("Firmware validation succeeded");
			break;
		case 2:
			ret = validate_upgrade_application_file(file);
			if (ret) {
				printf("application validation failed\n");
				return -1;
			}
			LogInfo("application validation succeeded");
			break;
		case 3:
			ret = validate_upgrade_resource_file(file);
			if (ret) {
				printf("resource validation failed\n");
				return -1;
			}
			LogInfo("resource validation succeeded");
			break;
		default:
			fprintf(stderr, "Invalid type: %s\n", argv[2]);
			return -1;
	}


	return 0;
}
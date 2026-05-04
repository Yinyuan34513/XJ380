#include "../xapi/include/x3api.h"
#include "../../include/xapi_json.h"

static int single_metadata(const char *packname)
{
    if (packname == NULL || packname[0] == '\0') {
        xapi_Printf((char *)"  Error invalid package name.\n");
        return -1;
    }

    char url[512];
    snprintf(url, sizeof(url), "nut.cn-nb1.rains3.com/%s/metas.json", packname);

    char *body = get(url, 443, true);
    if (body == NULL) {
        xapi_Printf((char *)"  Failed to fetch metadata for package %s.\n", packname);
        return -1;
    }

    xapi_JsonDocument document;
    memset(&document, 0, sizeof(document));

    int parse_result = xapi_parseJson(body, &document);
    free(body);
    if (parse_result != XAPI_JSON_PARSE_OK) {
        xapi_Printf((char *)"  Error json parse failed: %d\n", parse_result);
        return -1;
    }

    xapi_JsonValue *root = document.root;
    if (root == NULL || root->type != XAPI_JSON_VALUE_OBJECT) {
        xapi_Printf((char *)"  Error root is not object\n");
        xapi_freeJsonDocument(&document);
        return -1;
    }

    xapi_JsonValue *name = xapi_jsonObjectGet(root, "name");
    if (name == NULL || name->type != XAPI_JSON_VALUE_STRING || name->as.stringValue == NULL ||
        strcmp(name->as.stringValue, packname) != 0) {
        xapi_Printf((char *)"  Error invalid package name in metadata.\n");
        xapi_freeJsonDocument(&document);
        return -1;
    }

    xapi_JsonValue *author = xapi_jsonObjectGet(root, "author");
    if (author != NULL && author->type == XAPI_JSON_VALUE_STRING && author->as.stringValue != NULL) {
        xapi_Printf((char *)"  Author: %s\n", author->as.stringValue);
    }

    xapi_JsonValue *versions = xapi_jsonObjectGet(root, "versions");
    if (versions == NULL || versions->type != XAPI_JSON_VALUE_OBJECT) {
        xapi_Printf((char *)"  Error versions is not object.\n");
        xapi_freeJsonDocument(&document);
        return -1;
    }

    xapi_JsonValue *latest = xapi_jsonObjectGet(versions, "latest");
    if (latest == NULL || latest->type != XAPI_JSON_VALUE_STRING || latest->as.stringValue == NULL) {
        xapi_Printf((char *)"  Error versions.latest is invalid.\n");
        xapi_freeJsonDocument(&document);
        return -1;
    }

    xapi_Printf((char *)"  Latest: %s\n", latest->as.stringValue);

    xapi_JsonValue *available = xapi_jsonObjectGet(versions, "available");
    if (available == NULL || available->type != XAPI_JSON_VALUE_OBJECT) {
        xapi_Printf((char *)"  Error versions.available is not object.\n");
        xapi_freeJsonDocument(&document);
        return -1;
    }

    xapi_JsonValue *latest_files = xapi_jsonObjectGet(available, latest->as.stringValue);
    if (latest_files == NULL || latest_files->type != XAPI_JSON_VALUE_ARRAY) {
        xapi_Printf((char *)"  Error available[latest] is not array.\n");
        xapi_freeJsonDocument(&document);
        return -1;
    }

    int file_count = xapi_jsonArraySize(latest_files);
    for (int i = 0; i < file_count; ++i) {
        xapi_JsonValue *file_item = xapi_jsonArrayAt(latest_files, i);
        if (file_item == NULL || file_item->type != XAPI_JSON_VALUE_OBJECT) {
            xapi_Printf((char *)"  Error invalid file item at %d.\n", i);
            xapi_freeJsonDocument(&document);
            return -1;
        }

        xapi_JsonValue *arch = xapi_jsonObjectGet(file_item, "arch");
        xapi_JsonValue *filename = xapi_jsonObjectGet(file_item, "filename");
        if (arch == NULL || arch->type != XAPI_JSON_VALUE_STRING || arch->as.stringValue == NULL ||
            filename == NULL || filename->type != XAPI_JSON_VALUE_STRING || filename->as.stringValue == NULL) {
            xapi_Printf((char *)"  Error invalid file metadata at %d.\n", i);
            xapi_freeJsonDocument(&document);
            return -1;
        }

        xapi_Printf((char *)"  File: arch=%s, name=%s\n", arch->as.stringValue, filename->as.stringValue);
    }

    xapi_freeJsonDocument(&document);
    return 0;
}

static int nut_print_packages_json(const char *json_text)
{
    xapi_JsonDocument document;
    memset(&document, 0, sizeof(document));

    int parse_result = xapi_parseJson(json_text, &document);
    if (parse_result != XAPI_JSON_PARSE_OK) {
        xapi_Printf((char *)"Error json parse failed: %d\n", parse_result);
        return -1;
    }

    xapi_JsonValue *root = document.root;
    if (root == NULL || root->type != XAPI_JSON_VALUE_OBJECT) {
        xapi_Printf((char *)"Error root is not object\n");
        xapi_freeJsonDocument(&document);
        return -1;
    }

    xapi_JsonValue *packages = xapi_jsonObjectGet(root, "packages");
    if (packages == NULL || packages->type != XAPI_JSON_VALUE_ARRAY) {
        xapi_Printf((char *)"Error packages is not array\n");
        xapi_freeJsonDocument(&document);
        return -1;
    }

    xapi_Printf((char *)"packages:\n");
    int count = xapi_jsonArraySize(packages);
    for (int i = 0; i < count; ++i) {
        xapi_JsonValue *item = xapi_jsonArrayAt(packages, i);
        if (item != NULL && item->type == XAPI_JSON_VALUE_STRING && item->as.stringValue != NULL) {
            xapi_Printf((char *)"%d. %s\n", i + 1, item->as.stringValue);
            xapi_Printf((char *)"  Fetching metadata for %s...\n", item->as.stringValue);
            single_metadata(item->as.stringValue);
        } else {
            xapi_Printf((char *)"%d. <invalid>\n", i + 1);
        }
    }

    xapi_freeJsonDocument(&document);
    return 0;
}

static int nut_main_impl(int argc, char *argv[], char *envp[])
{
    (void)envp;
    xapi_OutputSerial("nut is running");

    if (argc < 2 || argv == NULL || argv[1] == NULL || strcmp(argv[1], "fetch") != 0) {
        xapi_Printf((char *)"Hello world\n");
        return 0;
    }

    xapi_OutputSerial("nut fetch, right?");
    xapi_Printf((char *)"Fetching packages.json\n");
    char *body = get("nut.cn-nb1.rains3.com/packages.json", 443, true);
    if (body == NULL) {
        xapi_Printf((char *)"Error request failed\n");
        return 1;
    }

    int result = nut_print_packages_json(body);

    free(body);
    return result == 0 ? 0 : 1;
}

extern "C" int nut_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int nut_main_cpp(int argc, char *argv[], char *envp[])
{
    return nut_main_impl(argc, argv, envp);
}

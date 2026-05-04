#include "../xapi/include/x3api.h"
#include "../include/httpclient.h"
#include "../include/https_client.h"
#include "../../include/xapi_json.h"

typedef struct nut_response_buffer
{
    char   *data;
    size_t  size;
    size_t  capacity;
    int     oom;
} nut_response_buffer_t;

static void nut_serial_text(const char *text)
{
    if (text == NULL) {
        return;
    }
    xapi_OutputSerial((char *)text);
}

static void nut_serialf(const char *format, ...)
{
    char    buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    xapi_OutputSerial(buffer);
}

static int nut_response_callback(const char *data, int len, void *user_data)
{
    nut_response_buffer_t *buffer = (nut_response_buffer_t *)user_data;
    if (buffer == NULL || data == NULL || len <= 0) {
        return 0;
    }

    if (buffer->size + (size_t)len + 1 > buffer->capacity) {
        size_t target_capacity = buffer->capacity == 0 ? 1024 : buffer->capacity;
        while (target_capacity < buffer->size + (size_t)len + 1) {
            target_capacity *= 2;
        }

        char *new_data = (char *)realloc(buffer->data, target_capacity);
        if (new_data == NULL) {
            buffer->oom = 1;
            return 1;
        }

        buffer->data     = new_data;
        buffer->capacity = target_capacity;
    }

    memcpy(buffer->data + buffer->size, data, (size_t)len);
    buffer->size += (size_t)len;
    buffer->data[buffer->size] = '\0';
    return 0;
}

static const char *nut_find_http_body(const char *response)
{
    const char *body = NULL;

    if (response == NULL) {
        return NULL;
    }

    body = strstr(response, "\r\n\r\n");
    if (body != NULL) {
        return body + 4;
    }

    body = strstr(response, "\n\n");
    if (body != NULL) {
        return body + 2;
    }

    return response;
}

static int nut_print_packages_json(const char *json_text)
{
    xapi_JsonDocument document;
    memset(&document, 0, sizeof(document));

    int parse_result = xapi_parseJson(json_text, &document);
    if (parse_result != XAPI_JSON_PARSE_OK) {
        nut_serialf("nut: json parse failed: %d\n", parse_result);
        return -1;
    }

    xapi_JsonValue *root = document.root;
    if (root == NULL || root->type != XAPI_JSON_VALUE_OBJECT) {
        nut_serial_text("nut: root is not object\n");
        xapi_freeJsonDocument(&document);
        return -1;
    }

    xapi_JsonValue *packages = xapi_jsonObjectGet(root, "packages");
    if (packages == NULL || packages->type != XAPI_JSON_VALUE_ARRAY) {
        nut_serial_text("nut: packages is not array\n");
        xapi_freeJsonDocument(&document);
        return -1;
    }

    nut_serial_text("packages:\n");
    int count = xapi_jsonArraySize(packages);
    for (int i = 0; i < count; ++i) {
        xapi_JsonValue *item = xapi_jsonArrayAt(packages, i);
        if (item != NULL && item->type == XAPI_JSON_VALUE_STRING && item->as.stringValue != NULL) {
            nut_serialf("%d. %s\n", i + 1, item->as.stringValue);
        } else {
            nut_serialf("%d. <invalid>\n", i + 1);
        }
    }

    xapi_freeJsonDocument(&document);
    return 0;
}

static int nut_main_impl(int argc, char *argv[], char *envp[])
{
    (void)argc;
    (void)argv;
    (void)envp;

    xhttp_request_t request;
    request.method        = "GET";
    request.host          = "nut.cn-nb1.rains3.com";
    request.port          = 443;
    request.path          = "/packages.json";
    request.body          = NULL;
    request.content_type  = NULL;
    request.extra_headers = "Accept: application/json\r\nConnection: close\r\n";

    nut_response_buffer_t response;
    memset(&response, 0, sizeof(response));

    nut_serial_text("nut: fetching packages.json\n");
    int status = xtls_request(&request, nut_response_callback, &response);
    if (status != XHTTP_OK) {
        nut_serialf("nut: request failed: %d\n", status);
        free(response.data);
        return 1;
    }
    if (response.oom) {
        nut_serial_text("nut: out of memory while receiving response\n");
        free(response.data);
        return 1;
    }
    if (response.data == NULL || response.size == 0) {
        nut_serial_text("nut: empty response\n");
        free(response.data);
        return 1;
    }

    const char *body = nut_find_http_body(response.data);
    int result = nut_print_packages_json(body);
    free(response.data);
    return result == 0 ? 0 : 1;
}

extern "C" int nut_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int nut_main_cpp(int argc, char *argv[], char *envp[])
{
    return nut_main_impl(argc, argv, envp);
}

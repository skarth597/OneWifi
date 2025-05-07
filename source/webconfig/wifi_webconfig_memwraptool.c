/************************************************************************************
  If not stated otherwise in this file or this component's LICENSE file the
  following copyright and licenses apply:
  Copyright 2018 RDK Management
  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at
  http://www.apache.org/licenses/LICENSE-2.0
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
**************************************************************************/

#include "collection.h"
#include "wifi_webconfig.h"
#include "wifi_util.h"
#include "wifi_ctrl.h"
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

webconfig_subdoc_object_t wifi_memwraptool_objects[3] = {
    { webconfig_subdoc_object_type_version, "Version"    },
    { webconfig_subdoc_object_type_subdoc,  "SubDocName" },
    { webconfig_subdoc_object_type_config,  "Parameters" }
};

webconfig_error_t init_memwraptool_subdoc(webconfig_subdoc_t *doc)
{
    doc->num_objects = sizeof(wifi_memwraptool_objects) / sizeof(webconfig_subdoc_object_t);
    memcpy((unsigned char *)doc->objects, (unsigned char *)&wifi_memwraptool_objects,
        sizeof(wifi_memwraptool_objects));
    return webconfig_error_none;
}

webconfig_error_t access_memwraptool_subdoc(webconfig_t *config, webconfig_subdoc_data_t *data)
{
    return webconfig_error_none;
}

webconfig_error_t translate_from_memwraptool_subdoc(webconfig_t *config,
    webconfig_subdoc_data_t *data)
{
    return webconfig_error_none;
}

webconfig_error_t translate_to_memwraptool_subdoc(webconfig_t *config,
    webconfig_subdoc_data_t *data)
{
    return webconfig_error_none;
}

webconfig_error_t encode_memwraptool_subdoc(webconfig_t *config, webconfig_subdoc_data_t *data)
{
    cJSON *json;
    cJSON *obj;
    char *str;
    webconfig_subdoc_decoded_data_t *params;

    if (data == NULL) {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d: NULL data pointer\n", __func__, __LINE__);
        return webconfig_error_encode;
    }

    params = &data->u.decoded;
    if (params == NULL) {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d: NULL pointer\n", __func__, __LINE__);
        return webconfig_error_encode;
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d: Failed to create JSON object\n", __func__,
            __LINE__);
        return webconfig_error_encode;
    }

    data->u.encoded.json = json;
    cJSON_AddStringToObject(json, "Version", "1.0");
    cJSON_AddStringToObject(json, "SubDocName", "memwraptool config");
    obj = cJSON_CreateObject();
    cJSON_AddItemToObject(json, "Parameters", obj);

    if (encode_memwraptool_object(&params->config.global_parameters.memwraptool, obj) != webconfig_error_none) {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d: Failed to encode memwraptool config\n",
            __func__, __LINE__);
        cJSON_Delete(json);
        return webconfig_error_encode;
    }
    str = cJSON_Print(json);

    data->u.encoded.raw = (webconfig_subdoc_encoded_raw_t)calloc(strlen(str) + 1, sizeof(char));
    if (data->u.encoded.raw == NULL) {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d: Failed to allocate memory\n", __func__,
            __LINE__);
        cJSON_free(str);
        cJSON_Delete(json);
        return webconfig_error_encode;
    }

    memcpy(data->u.encoded.raw, str, strlen(str));
    wifi_util_info_print(WIFI_WEBCONFIG, "%s:%d: Encoded success %s\n", __func__, __LINE__, str);
    cJSON_free(str);
    cJSON_Delete(json);
    return webconfig_error_none;
}

webconfig_error_t decode_memwraptool_subdoc(webconfig_t *config, webconfig_subdoc_data_t *data)
{
    webconfig_subdoc_decoded_data_t *params;
    cJSON *obj_config;
    cJSON *json;
    params = &data->u.decoded;
    if (params == NULL) {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d: NULL pointer\n", __func__, __LINE__);
        return webconfig_error_decode;
    }
    json = data->u.encoded.json;
    if (json == NULL) {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d: NULL json pointer\n", __func__, __LINE__);
        return webconfig_error_decode;
    }

    memset(params->config.global_parameters.memwraptool, 0, sizeof(memwraptool_config_t));
    obj_config = cJSON_GetObjectItem(json, "Parameters");
    if (decode_memwraptool_object(obj_config, &params->config.global_parameters.memwraptool) !=
        webconfig_error_none) {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d: Config object validation failed\n", __func__,
            __LINE__);
        wifi_util_error_print(WIFI_WEBCONFIG, "%s\n", (char *)data->u.encoded.raw);
        cJSON_Delete(json);
        return webconfig_error_invalid_subdoc;
    }
    cJSON_Delete(json);
    wifi_util_info_print(WIFI_WEBCONFIG, "%s:%d: Decoded success\n", __func__, __LINE__);
    return webconfig_error_none;
}
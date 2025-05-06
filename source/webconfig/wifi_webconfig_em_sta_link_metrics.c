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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include "collection.h"
#include "wifi_webconfig.h"
#include "wifi_monitor.h"
#include "wifi_util.h"
#include "wifi_ctrl.h"

#ifdef EM_APP
webconfig_subdoc_object_t   em_sta_link_metrics_objects[3] = {
    { webconfig_subdoc_object_type_version, "Version" },
    { webconfig_subdoc_object_type_subdoc, "SubDocName" },
    { webconfig_subdoc_object_type_em_sta_link_metrics, "WifiStaLinkMetrics" },
};

webconfig_error_t init_em_sta_link_subdoc(webconfig_subdoc_t *doc)
{
    doc->num_objects = sizeof(em_sta_link_metrics_objects)/sizeof(webconfig_subdoc_object_t);
    memcpy((unsigned char *)doc->objects, (unsigned char *)&em_sta_link_metrics_objects, sizeof(em_sta_link_metrics_objects));

    return webconfig_error_none;
}

webconfig_error_t access_check_em_sta_link_subdoc(webconfig_t *config, webconfig_subdoc_data_t *data)
{
    return webconfig_error_none;
}

webconfig_error_t translate_from_em_sta_link_subdoc(webconfig_t *config, webconfig_subdoc_data_t *data)
{
    if (((data->descriptor & webconfig_data_descriptor_translate_to_ovsdb) == webconfig_data_descriptor_translate_to_ovsdb) ||  
        ((data->descriptor & webconfig_data_descriptor_translate_to_easymesh) == webconfig_data_descriptor_translate_to_easymesh)) {
        if (config->proto_desc.translate_to(webconfig_subdoc_type_em_sta_link_metrics, data) != webconfig_error_none) {
            if ((data->descriptor & webconfig_data_descriptor_translate_to_ovsdb) == webconfig_data_descriptor_translate_to_ovsdb) {
                return webconfig_error_translate_to_ovsdb;
            } else {
                return webconfig_error_translate_to_easymesh;
            }
        }
    } else if ((data->descriptor & webconfig_data_descriptor_translate_to_tr181) == webconfig_data_descriptor_translate_to_tr181) {

    } else {
        // no translation required
    }
    //no translation required
    return webconfig_error_none;
}

webconfig_error_t translate_to_em_sta_link_subdoc(webconfig_t *config, webconfig_subdoc_data_t *data)
{
    return webconfig_error_none;
}

webconfig_error_t encode_em_sta_link_subdoc(webconfig_t *config, webconfig_subdoc_data_t *data)
{
    cJSON *json, *obj_emstalink, *device_list, *device_obj, *sta_link_obj;
    char *str;
    webconfig_subdoc_decoded_data_t *params;

    if (data == NULL) {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d: NULL data Pointer\n", __func__, __LINE__);
        return webconfig_error_encode;
    }

    params = &data->u.decoded;
    if (params == NULL) {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d: NULL Pointer\n", __func__, __LINE__);
        return webconfig_error_encode;
    }

    json = cJSON_CreateObject();
    if (json == NULL) {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d: json create object failed\n", __func__, __LINE__);
        return webconfig_error_encode;
    }

    data->u.encoded.json = json;

    cJSON_AddStringToObject(json, "Version", "1.0");
    cJSON_AddStringToObject(json, "SubDocName", "Easymesh STA link metrics");
    cJSON_AddNumberToObject(json, "Vap Index", params->em_sta_link_metrics_rsp.vap_index);

    obj_emstalink = cJSON_CreateArray();
    cJSON_AddItemToObject(json, "Associated STA Link Metrics Report", obj_emstalink);
    
    if (encode_em_sta_link_metrics_object(&params->em_sta_link_metrics_rsp, obj_emstalink) != webconfig_error_none) {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d: Failed to encode wifi easymesh config\n", __func__, __LINE__);
        return webconfig_error_encode;
    }

    str = cJSON_Print(json);

    data->u.encoded.raw = (webconfig_subdoc_encoded_raw_t)calloc(strlen(str) + 1, sizeof(char));
    if (data->u.encoded.raw == NULL) {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d Failed to allocate memory.\n", __func__, __LINE__);
        cJSON_free(str);
        cJSON_Delete(json);
        return webconfig_error_encode;
    }

    memcpy(data->u.encoded.raw, str, strlen(str));
    wifi_util_info_print(WIFI_WEBCONFIG, "%s:%d: encode success %s\n", __func__, __LINE__, str);
    cJSON_free(str);
    cJSON_Delete(json);
    return webconfig_error_none;
}

webconfig_error_t decode_em_sta_link_subdoc(webconfig_t *config, webconfig_subdoc_data_t *data)
{
    webconfig_subdoc_decoded_data_t *params;
    cJSON *json;
    const cJSON *rsp_obj, *vap_index_item;
    em_assoc_sta_link_metrics_rsp_t *sta_link_metrics;

    params = &data->u.decoded;
    if (params == NULL) {
        return webconfig_error_decode;
    }

    json = data->u.encoded.json;
    if (json == NULL) {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d: NULL json pointer\n", __func__, __LINE__);
        return webconfig_error_decode;
    }

    sta_link_metrics = &params->em_sta_link_metrics_rsp;

    vap_index_item = cJSON_GetObjectItem(json, "Vap Index");
    if (cJSON_IsNumber(vap_index_item)) {
        sta_link_metrics->vap_index = vap_index_item->valueint;
    } else {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d: Vap Index is invalid\n", __func__, __LINE__);
        return webconfig_error_decode;
    }

    rsp_obj = cJSON_GetObjectItem(json, "Associated STA Link Metrics Report");
    if (rsp_obj == NULL) {
        wifi_util_error_print(WIFI_WEBCONFIG,"%s:%d: cjson object is NULL\n", __func__, __LINE__);
        return webconfig_error_decode;
    }

    if (decode_em_sta_link_metrics_object(rsp_obj, &params->em_sta_link_metrics_rsp) != webconfig_error_none) {
        wifi_util_error_print(WIFI_WEBCONFIG, "%s:%d: STA Metrics object Validation Failed\n", __func__, __LINE__);
        cJSON_Delete(json);
        if(params->em_sta_link_metrics_rsp.per_sta_metrics != NULL)
            free(params->em_sta_link_metrics_rsp.per_sta_metrics);
        wifi_util_error_print(WIFI_WEBCONFIG, "%s\n", (char *)data->u.encoded.raw);
        return webconfig_error_decode;
    }

    cJSON_Delete(json);
    wifi_util_info_print(WIFI_WEBCONFIG, "%s:%d: decode success\n", __func__, __LINE__);
    return webconfig_error_none;
}
#endif
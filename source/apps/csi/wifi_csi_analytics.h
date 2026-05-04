/************************************************************************************
  If not stated otherwise in this file or this component's LICENSE file the
  following copyright and licenses apply:
  
  Copyright 2025 RDK Management
  
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

#ifndef WIFI_CSI_ANALYTICS_H
#define WIFI_CSI_ANALYTICS_H

#include "bus.h"
#include "collection.h"
#include <pthread.h>
#include <cjson/cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LOG_MSG_PRINT_TIME_SEC 10
#define MAX_MACLIST_SIZE 512
#define MAX_MAC_STR_SIZE 18
#define STA_MAC_LIST_DELIMITER ','
#define CSI_ENABLE_TRIGGER_SEC 2
#define CSI_STA_MACLIST_SET_SEC 3
#define CSI_ANALYTICS_INTERVAL 300

#define CSI_CLIENT_MACLIST "Device.WiFi.X_RDK_CSI.%d.ClientMaclist"
#define CSI_ENABLE_NAME "Device.WiFi.X_RDK_CSI.%d.Enable"
#define CSI_SUB_DATA "Device.WiFi.X_RDK_CSI.%d.data"
#define CSI_STREAM "Device.WiFi.X_RDK_CSI.%d.Stream"

typedef struct csi_analytics_data {
    uint32_t num_sc;
    uint32_t decimation;
    uint32_t skip_mismatch_data_num;
    long long int csi_data_capture_time_sec;
} csi_analytics_data_t;

typedef struct csi_analytics_info {
    int32_t pipe_read_fd;
    bool is_read_oper_thread_enabled;
    uint32_t csi_session_index;
    bool is_csi_capture_enabled;
    bool stream;
    pthread_mutex_t maclist_lock;
    char sta_mac[MAX_MACLIST_SIZE];
    int sta_maclist_sched_id;
    int csi_analytics_enable_sched_id;
    hash_map_t *csi_analytics_map;
} csi_analytics_info_t;

/**
 * @brief JSON state for a single CSI analytics streaming session.
 *
 * Mirrors csi_data_json_obj_t from wifievents_consumer_sample.c so the
 * analytics module can follow the same current_sample_obj pattern.
 */
typedef struct csi_analytics_json_obj {
    cJSON *main_json_obj;
    cJSON *json_csi_obj;
    cJSON *json_sounding_devices;
    cJSON *current_sample_obj;  /* points to the single sample added in the latest call */
} csi_analytics_json_obj_t;

/**
 * @brief Return the module-level csi_analytics_json_obj_t singleton.
 */
csi_analytics_json_obj_t *get_csi_analytics_json_obj(void);

/**
 * @brief Add CSI frame_info fields to a cJSON object.
 *
 * Duplicated from wifievents_consumer_sample.c for use in the CSI analytics
 * streaming path, so the analytics module does not depend on the sample app.
 */
void csi_analytics_json_add_frame_info(cJSON *sta_obj, wifi_frame_info_t *frame_info);

/**
 * @brief Add CSI matrix data to a cJSON object.
 */
void csi_analytics_json_add_matrix_info(cJSON *csi_matrix_obj_wrapper, wifi_csi_data_t *csi);

/**
 * @brief Build a complete per-sample cJSON object with frame_info and csi_matrix.
 */
void csi_analytics_client_csi_data_json_elem_add(cJSON *sta_obj, wifi_csi_data_t *csi, char *str_sta_mac);

/**
 * @brief Build a per-sample cJSON object and track it in the analytics
 *        JSON state (current_sample_obj) so save_json_data_to_hermes_file()
 *        can serialize it in isolation.
 */
void csi_analytics_data_in_json_format(wifi_csi_dev_t *csi_dev_data);

/**
 * @brief Serialize the current_sample_obj as a structured JSON envelope and
 *        write it to HermesFS (/tmp/simple_file and /tmp/hermes/simple_file).
 *
 * Uses get_csi_analytics_json_obj()->current_sample_obj, matching the original
 * hermes_last pattern from wifievents_consumer_sample.c.
 */
void save_json_data_to_hermes_file(void);

#ifdef __cplusplus
}
#endif
#endif

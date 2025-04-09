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

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <inttypes.h>
#include "wifi_events.h"
#include "wifi_mgr.h"
#include "wifi_util.h"

extern bool monitor_initialization_done;

const char *wifi_event_type_to_string(wifi_event_type_t type)
{
#define DOC2S(x) \
    case x:      \
        return #x;
    switch (type) {
        DOC2S(wifi_event_type_exec)
        DOC2S(wifi_event_type_webconfig)
        DOC2S(wifi_event_type_hal_ind)
        DOC2S(wifi_event_type_command)
        DOC2S(wifi_event_type_monitor)
        DOC2S(wifi_event_type_net)
        DOC2S(wifi_event_type_wifiapi)
        DOC2S(wifi_event_type_analytic)
        DOC2S(wifi_event_type_csi)
        DOC2S(wifi_event_type_speed_test)
        DOC2S(wifi_event_type_max)
    default:
        wifi_util_error_print(WIFI_CTRL, "%s:%d: event not handle[%d]\r\n", __func__, __LINE__,
            type);
        break;
    }

    return "unknown event";
}

const char *wifi_event_subtype_to_string(wifi_event_subtype_t type)
{
#define DOC2S(x) \
    case x:      \
        return #x;
    switch (type) {
        DOC2S(wifi_event_exec_start)
        DOC2S(wifi_event_exec_stop)
        DOC2S(wifi_event_exec_timeout)
        DOC2S(wifi_event_exec_max)
        DOC2S(wifi_event_webconfig_set_data)
        DOC2S(wifi_event_webconfig_set_status)
        DOC2S(wifi_event_webconfig_hal_result)
        DOC2S(wifi_event_webconfig_get_data)
        DOC2S(wifi_event_webconfig_set_data_tunnel)
        DOC2S(wifi_event_webconfig_set_data_dml)
        DOC2S(wifi_event_webconfig_set_data_webconfig)
        DOC2S(wifi_event_webconfig_set_data_ovsm)
        DOC2S(wifi_event_webconfig_data_resched_to_ctrl_queue)
        DOC2S(wifi_event_webconfig_data_to_apply_pending_queue)
        DOC2S(wifi_event_webconfig_data_to_hal_apply)
        DOC2S(wifi_event_webconfig_set_data_sta_bssid)
        DOC2S(wifi_event_webconfig_data_req_from_dml)
        DOC2S(wifi_event_webconfig_set_data_force_apply)
        DOC2S(wifi_event_webconfig_max)
        DOC2S(wifi_event_hal_unknown_frame)
        DOC2S(wifi_event_hal_mgmt_frames)
        DOC2S(wifi_event_hal_probe_req_frame)
        DOC2S(wifi_event_hal_probe_rsp_frame)
        DOC2S(wifi_event_hal_auth_frame)
        DOC2S(wifi_event_hal_deauth_frame)
        DOC2S(wifi_event_hal_assoc_req_frame)
        DOC2S(wifi_event_hal_assoc_rsp_frame)
        DOC2S(wifi_event_hal_reassoc_req_frame)
        DOC2S(wifi_event_hal_reassoc_rsp_frame)
        DOC2S(wifi_event_hal_dpp_public_action_frame)
        DOC2S(wifi_event_hal_dpp_config_req_frame)
        DOC2S(wifi_event_hal_anqp_gas_init_frame)
        DOC2S(wifi_event_hal_sta_conn_status)
        DOC2S(wifi_event_hal_assoc_device)
        DOC2S(wifi_event_hal_disassoc_device)
        DOC2S(wifi_event_scan_results)
        DOC2S(wifi_event_hal_channel_change)
        DOC2S(wifi_event_radius_greylist)
        DOC2S(wifi_event_hal_potential_misconfiguration)
        DOC2S(wifi_event_hal_analytics)
        DOC2S(wifi_event_radius_eap_failure)
        DOC2S(wifi_event_radius_fallback_and_failover)
        DOC2S(wifi_event_type_csi_data)
        DOC2S(wifi_event_hal_max)
        DOC2S(wifi_event_type_active_gw_check)
        DOC2S(wifi_event_type_command_factory_reset)
        DOC2S(wifi_event_type_radius_grey_list_rfc)
        DOC2S(wifi_event_type_wifi_passpoint_rfc)
        DOC2S(wifi_event_type_wifi_interworking_rfc)
        DOC2S(wifi_event_type_wpa3_rfc)
        DOC2S(wifi_event_type_dfs_rfc)
        DOC2S(wifi_event_type_dfs_atbootup_rfc)
        DOC2S(wifi_event_type_command_kickmac)
        DOC2S(wifi_event_type_command_kick_assoc_devices)
        DOC2S(wifi_event_type_command_wps)
        DOC2S(wifi_event_type_command_wps_pin)
        DOC2S(wifi_event_type_command_wps_cancel)
        DOC2S(wifi_event_type_command_wifi_host_sync)
        DOC2S(wifi_event_type_device_network_mode)
        DOC2S(wifi_event_type_twoG80211axEnable_rfc)
        DOC2S(wifi_event_type_command_wifi_neighborscan)
        DOC2S(wifi_event_type_command_mesh_status)
        DOC2S(wifi_event_type_normalized_rssi)
        DOC2S(wifi_event_type_snr)
        DOC2S(wifi_event_type_cli_stat)
        DOC2S(wifi_event_type_txrx_rate)
        DOC2S(wifi_event_type_prefer_private_rfc)
        DOC2S(wifi_event_type_mgmt_frame_bus_rfc)
        DOC2S(wifi_event_type_sta_connect_in_progress)
        DOC2S(wifi_event_type_udhcp_ip_fail)
        DOC2S(wifi_event_type_trigger_disconnection)
        DOC2S(wifi_event_type_trigger_disconnection_analytics)
        DOC2S(wifi_event_type_new_bssid)
        DOC2S(wifi_event_type_xfinity_enable)
        DOC2S(wifi_event_type_wifi_offchannelscan_app_rfc)
        DOC2S(wifi_event_type_wifi_offchannelscan_sm_rfc)
        DOC2S(wifi_event_type_levl_rfc)
        DOC2S(wifi_event_type_eth_bh_status)
        DOC2S(wifi_event_type_managed_wifi_disable)
        DOC2S(wifi_event_type_notify_monitor_done)
        DOC2S(wifi_event_type_start_inst_msmt)
        DOC2S(wifi_event_type_stop_inst_msmt)
        DOC2S(wifi_event_type_xfinity_rrm)
        DOC2S(wifi_event_type_collect_stats)
        DOC2S(wifi_event_type_send_action_frame)
        DOC2S(wifi_event_type_start_channel_scan)
        DOC2S(wifi_event_type_rsn_override_rfc)
        DOC2S(wifi_event_type_sta_client_info)
        DOC2S(wifi_event_command_max)
        DOC2S(wifi_event_monitor_diagnostics)
        DOC2S(wifi_event_monitor_connect)
        DOC2S(wifi_event_monitor_disconnect)
        DOC2S(wifi_event_monitor_deauthenticate)
        DOC2S(wifi_event_monitor_start_inst_msmt)
        DOC2S(wifi_event_monitor_stop_inst_msmt)
        DOC2S(wifi_event_monitor_started_active_msmt)
        DOC2S(wifi_event_monitor_stop_active_msmt)
        DOC2S(wifi_event_monitor_stats_flag_change)
        DOC2S(wifi_event_monitor_radio_stats_flag_change)
        DOC2S(wifi_event_monitor_vap_stats_flag_change)
        DOC2S(wifi_event_monitor_process_active_msmt)
        DOC2S(wifi_event_monitor_csi)
        DOC2S(wifi_event_monitor_csi_pinger)
        DOC2S(wifi_event_monitor_clientdiag_update_config)
        DOC2S(wifi_event_monitor_data_collection_config)
        DOC2S(wifi_event_monitor_provider_response)
        DOC2S(wifi_event_monitor_assoc_req)
        DOC2S(wifi_event_monitor_clear_sta_counters)
        DOC2S(wifi_event_monitor_get_radiostats_onchan)
        DOC2S(wifi_event_monitor_get_radiostats_offchan)
        DOC2S(wifi_event_monitor_get_radiostats_fullchan)
        DOC2S(wifi_event_monitor_get_neighborstats_onchan)
        DOC2S(wifi_event_monitor_get_neighborstats_offchan)
        DOC2S(wifi_event_monitor_get_neighborstats_fullchan)
        DOC2S(wifi_event_monitor_get_assocdevice_stats)
        DOC2S(wifi_event_monitor_get_radiodiag_stats)
        DOC2S(wifi_event_monitor_get_radio_temperature)
        DOC2S(wifi_event_monitor_set_subscribe)
        DOC2S(wifi_event_monitor_action_frame)
        DOC2S(wifi_event_monitor_max)
        DOC2S(wifi_event_type_xfinity_tunnel_up)
        DOC2S(wifi_event_type_xfinity_tunnel_down)
        DOC2S(wifi_event_type_xfinity_tunnel_max)
        DOC2S(wifi_event_type_wifiapi_execution)
        DOC2S(wifi_event_type_wifiapi_max)
        DOC2S(wifi_event_webconfig_br_report)
        DOC2S(wifi_event_br_report)
    default:
        wifi_util_error_print(WIFI_CTRL, "%s:%d: event not handle[%d]\r\n", __func__, __LINE__,
            type);
        break;
    }

    return "unknown event";
}

void free_cloned_event(wifi_event_t *clone)
{
    destroy_wifi_event(clone);
}

int clone_wifi_event(wifi_event_t *event, wifi_event_t **clone)
{
    wifi_event_t *cloned;
    wifi_event_type_t event_type;
    wifi_event_subtype_t sub_type;
    void *msg = NULL;
    unsigned int msg_len = 0;

    event_type = event->event_type;
    sub_type = event->sub_type;

    switch (event_type) {
    case wifi_event_type_exec:
    case wifi_event_type_webconfig:
    case wifi_event_type_hal_ind:
    case wifi_event_type_command:
    case wifi_event_type_net:
    case wifi_event_type_wifiapi:
    case wifi_event_type_speed_test:
        msg_len = event->u.core_data.len;
        msg = event->u.core_data.msg;
        break;
    case wifi_event_type_monitor:
        if (event->sub_type == wifi_event_monitor_provider_response) {
            msg = event->u.provider_response;
            msg_len = sizeof(wifi_provider_response_t);
        } else {
            msg = event->u.mon_data;
            msg_len = sizeof(wifi_monitor_data_t);
        }
        break;
    case wifi_event_type_analytic:
        break;
    default:
        wifi_util_error_print(WIFI_CTRL, "%s %d Invalid event type : %s\n", __FUNCTION__, __LINE__,
            wifi_event_type_to_string(event->event_type));
        return RETURN_ERR;
    }

    if ((event_type == wifi_event_type_monitor) &&
        (sub_type == wifi_event_monitor_provider_response)) {
        cloned = (wifi_event_t *)create_wifi_monitor_response_event(msg, msg_len, event_type,
            sub_type);
    } else {
        cloned = (wifi_event_t *)create_wifi_event(msg_len, event_type, sub_type);
    }
    if (cloned == NULL) {
        wifi_util_error_print(WIFI_CTRL, "%s %d data malloc null for event_type : %d\n",
            __FUNCTION__, __LINE__, event_type);
        return RETURN_ERR;
    }
    if (copy_msg_to_event(msg, msg_len, event_type, sub_type, &event->route, cloned) != RETURN_OK) {
        wifi_util_error_print(WIFI_CTRL, "%s %d unable to copy msg to event : %d \n", __FUNCTION__,
            __LINE__, event_type);
        destroy_wifi_event(cloned);
        return RETURN_ERR;
    }

    *clone = cloned;

    return RETURN_OK;
}

wifi_event_t *create_wifi_event(unsigned int msg_len, wifi_event_type_t type,
    wifi_event_subtype_t sub_type)
{
    wifi_event_t *event;
    if (type >= wifi_event_type_max) {
        wifi_util_error_print(WIFI_CTRL, "%s %d Invalid event type %d\n", __FUNCTION__, __LINE__,
            type);
        return NULL;
    }
    event = (wifi_event_t *)calloc(1, sizeof(wifi_event_t));
    if (event == NULL) {
        wifi_util_error_print(WIFI_CTRL,
            "%s %d memory allocation failed for event type : %s subtype : %s\n", __FUNCTION__,
            __LINE__, wifi_event_type_to_string(type), wifi_event_subtype_to_string(sub_type));
        return NULL;
    }

    switch (type) {
    case wifi_event_type_exec:
    case wifi_event_type_webconfig:
    case wifi_event_type_hal_ind:
    case wifi_event_type_command:
    case wifi_event_type_net:
    case wifi_event_type_wifiapi:
    case wifi_event_type_speed_test:
        if (msg_len != 0) {
            event->u.core_data.msg = calloc(1, (msg_len + 1));
            if (event->u.core_data.msg == NULL) {
                wifi_util_error_print(WIFI_CTRL, "%s %d data message malloc null for type : %s\n",
                    __FUNCTION__, __LINE__, wifi_event_type_to_string(type));
                free(event);
                event = NULL;
                return NULL;
            }
            event->u.core_data.len = msg_len;
        } else {
            event->u.core_data.len = 0;
        }
        break;
    case wifi_event_type_monitor:
        if ((sub_type == wifi_event_monitor_provider_response) ||
            (sub_type == wifi_event_type_collect_stats)) {
            event->u.provider_response = calloc(1, (msg_len));
            if (event->u.provider_response == NULL) {
                wifi_util_error_print(WIFI_CTRL,
                    "%s %d data message malloc null for type : %s subtype : %s\n", __FUNCTION__,
                    __LINE__, wifi_event_type_to_string(type),
                    wifi_event_subtype_to_string(sub_type));
                free(event);
                event = NULL;
                return NULL;
            }
        } else {
            event->u.mon_data = calloc(1, (msg_len));
            if (event->u.mon_data == NULL) {
                wifi_util_error_print(WIFI_CTRL, "%s %d data message malloc null\n", __FUNCTION__,
                    __LINE__);
                free(event);
                event = NULL;
                return NULL;
            }
        }
        break;
    case wifi_event_type_csi:
        if (sub_type == wifi_event_type_csi_data) {
            event->u.csi = calloc(1, (msg_len));
            if (event->u.csi == NULL) {
                wifi_util_error_print(WIFI_CTRL, "%s %d data message malloc null\n", __FUNCTION__,
                    __LINE__);
                free(event);
                event = NULL;
                return NULL;
            }
        }
        break;
    case wifi_event_type_analytic:
        break;
    default:
        wifi_util_error_print(WIFI_CTRL, "%s %d Invalid event type : %s\n", __FUNCTION__, __LINE__,
            wifi_event_type_to_string(type));
        free(event);
        event = NULL;
        return NULL;
    }

    event->event_type = type;
    event->sub_type = sub_type;

    return event;
}

wifi_event_t *create_wifi_monitor_response_event(const void *msg, unsigned int msg_len,
    wifi_event_type_t type, wifi_event_subtype_t sub_type)
{
    wifi_event_t *event;

    if (type >= wifi_event_type_max) {
        wifi_util_error_print(WIFI_CTRL, "%s %d Invalid event : %s\n", __FUNCTION__, __LINE__,
            wifi_event_type_to_string(type));
        return NULL;
    }

    if (msg == NULL) {
        wifi_util_error_print(WIFI_CTRL, "%s %d Input msg is NULL\n", __FUNCTION__, __LINE__);
        return NULL;
    }

    if (type != wifi_event_type_monitor) {
        wifi_util_error_print(WIFI_CTRL, "%s %d Invalid type : %s\n", __FUNCTION__, __LINE__,
            wifi_event_type_to_string(type));
        return NULL;
    }

    if (sub_type != wifi_event_monitor_provider_response &&
        sub_type != wifi_event_type_collect_stats) {
        wifi_util_error_print(WIFI_CTRL, "%s %d Invalid sub_type: %s\n", __FUNCTION__, __LINE__,
            wifi_event_subtype_to_string(sub_type));
        return NULL;
    }
    event = (wifi_event_t *)create_wifi_event(msg_len, type, sub_type);
    if (event == NULL) {
        wifi_util_error_print(WIFI_CTRL,
            "%s %d create wifi event failed for type : %s sub_type : %s \n", __FUNCTION__, __LINE__,
            wifi_event_type_to_string(type), wifi_event_subtype_to_string(sub_type));
        return NULL;
    }

    const wifi_provider_response_t *response = msg;

    switch (response->data_type) {
    case mon_stats_type_radio_channel_stats:
        if (response->stat_array_size > 0) {
            event->u.provider_response->stat_pointer = calloc(response->stat_array_size,
                sizeof(radio_chan_data_t));
            if (event->u.provider_response->stat_pointer == NULL) {
                wifi_util_error_print(WIFI_CTRL, "%s %d response allocation failed for %d\n",
                    __FUNCTION__, __LINE__, response->data_type);
                free(event->u.provider_response);
                event->u.provider_response = NULL;
                free(event);
                event = NULL;
                return NULL;
            }
        }
        break;
    case mon_stats_type_neighbor_stats:
        if (response->stat_array_size > 0) {
            event->u.provider_response->stat_pointer = calloc(response->stat_array_size,
                sizeof(wifi_neighbor_ap2_t));
            if (event->u.provider_response->stat_pointer == NULL) {
                wifi_util_error_print(WIFI_CTRL, "%s %d response allocation failed for %d\n",
                    __FUNCTION__, __LINE__, response->data_type);
                free(event->u.provider_response);
                event->u.provider_response = NULL;
                free(event);
                event = NULL;
                return NULL;
            }
        }
        break;
    case mon_stats_type_associated_device_stats:
        if (response->stat_array_size > 0) {
            event->u.provider_response->stat_pointer = calloc(response->stat_array_size,
                sizeof(sta_data_t));
            if (event->u.provider_response->stat_pointer == NULL) {
                wifi_util_error_print(WIFI_CTRL, "%s %d response allocation failed for %d\n",
                    __FUNCTION__, __LINE__, response->data_type);
                free(event->u.provider_response);
                event->u.provider_response = NULL;
                free(event);
                event = NULL;
                return NULL;
            }
        }
        break;
    case mon_stats_type_radio_diagnostic_stats:
        if (response->stat_array_size > 0) {
            event->u.provider_response->stat_pointer = calloc(response->stat_array_size,
                sizeof(radio_data_t));
            if (event->u.provider_response->stat_pointer == NULL) {
                wifi_util_error_print(WIFI_CTRL, "%s %d response allocation failed for %d\n",
                    __FUNCTION__, __LINE__, response->data_type);
                free(event->u.provider_response);
                event->u.provider_response = NULL;
                free(event);
                event = NULL;
                return NULL;
            }
        }
        break;
    case mon_stats_type_radio_temperature:
        if (response->stat_array_size > 0) {
            event->u.provider_response->stat_pointer = calloc(response->stat_array_size,
                sizeof(radio_data_t));
            if (event->u.provider_response->stat_pointer == NULL) {
                wifi_util_error_print(WIFI_CTRL, "%s %d response allocation failed for %d\n",
                    __FUNCTION__, __LINE__, response->data_type);
                free(event->u.provider_response);
                event->u.provider_response = NULL;
                free(event);
                event = NULL;
                return NULL;
            }
        }
        break;
    default:
        wifi_util_error_print(WIFI_CTRL, "%s %d default response type : %d\n", __FUNCTION__,
            __LINE__, response->data_type);
        free(event->u.provider_response);
        event->u.provider_response = NULL;
        free(event);
        event = NULL;
        return NULL;
    }

    event->event_type = type;
    event->sub_type = sub_type;

    return event;
}

void destroy_wifi_event(wifi_event_t *event)
{
    if (event == NULL) {
        wifi_util_error_print(WIFI_CTRL,"%s %d input args are NULL\n",__FUNCTION__, __LINE__);
        return;
    }

    switch(event->event_type) {
        case wifi_event_type_analytic:
            break;
        case wifi_event_type_exec:
        case wifi_event_type_webconfig:
        case wifi_event_type_hal_ind:
        case wifi_event_type_command:
        case wifi_event_type_net:
        case wifi_event_type_wifiapi:
        case wifi_event_type_speed_test:
            if(event->u.core_data.msg != NULL) {
                free(event->u.core_data.msg);
                event->u.core_data.msg = NULL;
            }
        break;
        case wifi_event_type_monitor:
            if ((event->sub_type == wifi_event_monitor_provider_response) || (event->sub_type == wifi_event_type_collect_stats)) {
                if (event->u.provider_response != NULL) {
                    if (event->u.provider_response->stat_pointer != NULL) {
                        free(event->u.provider_response->stat_pointer);
                        event->u.provider_response->stat_pointer = NULL;
                    }
                    free(event->u.provider_response);
                    event->u.provider_response = NULL;
                }
            } else {
                if (event->u.mon_data != NULL) {
                    free(event->u.mon_data);
                    event->u.mon_data = NULL;
                }

            }
        break;
        case wifi_event_type_csi:
            if (event->sub_type == wifi_event_type_csi_data) {
                free(event->u.csi);
                event->u.csi = NULL;
            }
        break;
        default:
        break;
    }

    if (event != NULL) {
        free(event);
        event = NULL;
    }

    return;
}

int copy_msg_to_event(const void *data, unsigned int msg_len, wifi_event_type_t type,
    wifi_event_subtype_t sub_type, wifi_event_route_t *rt, wifi_event_t *event)
{
    if ((data == NULL) || (event == NULL)) {
        wifi_util_error_print(WIFI_CTRL, "%s %d Input arguements are NULL data : %p event : %p\n",
            __FUNCTION__, __LINE__, data, event);
        return RETURN_ERR;
    }
    switch (type) {
    case wifi_event_type_exec:
    case wifi_event_type_webconfig:
    case wifi_event_type_hal_ind:
    case wifi_event_type_command:
    case wifi_event_type_net:
    case wifi_event_type_wifiapi:
    case wifi_event_type_speed_test:
        if (msg_len != 0) {
            memcpy(event->u.core_data.msg, data, msg_len);
            event->u.core_data.len = msg_len;
        } else {
            event->u.core_data.len = 0;
        }
        break;
    case wifi_event_type_monitor:
        if ((sub_type == wifi_event_monitor_provider_response) ||
            (sub_type == wifi_event_type_collect_stats)) {
            const wifi_provider_response_t *response = data;
            switch (response->data_type) {
            case mon_stats_type_radio_channel_stats:
                if ((event->u.provider_response->stat_pointer == NULL) ||
                    (response->stat_pointer == NULL)) {
                    wifi_util_error_print(WIFI_CTRL,
                        "%s %d data_type %d stat_pointer is NULL : %p, %p\n", __FUNCTION__,
                        __LINE__, response->data_type, event->u.provider_response->stat_pointer,
                        response->stat_pointer);
                    return RETURN_ERR;
                }
                memcpy(event->u.provider_response->stat_pointer, response->stat_pointer,
                    (response->stat_array_size * sizeof(radio_chan_data_t)));
                break;
            case mon_stats_type_neighbor_stats:
                if (event->u.provider_response->stat_pointer != NULL) {
                    if (response->stat_pointer == NULL) {
                        wifi_util_error_print(WIFI_CTRL,
                            "%s %d data_type %d stat_pointer is NULL : %p\n", __FUNCTION__,
                            __LINE__, response->data_type, response->stat_pointer);
                        return RETURN_ERR;
                    }
                    memcpy(event->u.provider_response->stat_pointer, response->stat_pointer,
                        (response->stat_array_size * sizeof(wifi_neighbor_ap2_t)));
                } else {
                    event->u.provider_response->stat_pointer = NULL;
                }
                break;
            case mon_stats_type_associated_device_stats:
                if (event->u.provider_response->stat_pointer != NULL) {
                    if (response->stat_pointer == NULL) {
                        wifi_util_error_print(WIFI_CTRL,
                            "%s %d data_type %d stat_pointer is NULL : %p\n", __FUNCTION__,
                            __LINE__, response->data_type, response->stat_pointer);
                        return RETURN_ERR;
                    }
                    memcpy(event->u.provider_response->stat_pointer, response->stat_pointer,
                        (response->stat_array_size * sizeof(sta_data_t)));
                } else {
                    event->u.provider_response->stat_pointer = NULL;
                }
                break;
            case mon_stats_type_radio_diagnostic_stats:
                if ((event->u.provider_response->stat_pointer == NULL) ||
                    (response->stat_pointer == NULL)) {
                    wifi_util_error_print(WIFI_CTRL,
                        "%s %d data_type %d stat_pointer is NULL : %p, %p\n", __FUNCTION__,
                        __LINE__, response->data_type, event->u.provider_response->stat_pointer,
                        response->stat_pointer);
                    return RETURN_ERR;
                }
                memcpy(event->u.provider_response->stat_pointer, response->stat_pointer,
                    (response->stat_array_size * sizeof(radio_data_t)));
                break;
            case mon_stats_type_radio_temperature:
                if ((event->u.provider_response->stat_pointer == NULL) ||
                    (response->stat_pointer == NULL)) {
                    wifi_util_error_print(WIFI_CTRL,
                        "%s %d data_type %d stat_pointer is NULL : %p, %p\n", __FUNCTION__,
                        __LINE__, response->data_type, event->u.provider_response->stat_pointer,
                        response->stat_pointer);
                    return RETURN_ERR;
                }
                memcpy(event->u.provider_response->stat_pointer, response->stat_pointer,
                    (response->stat_array_size * sizeof(radio_data_t)));
                break;
            default:
                wifi_util_error_print(WIFI_CTRL, "%s %d default response type : %d\n", __FUNCTION__,
                    __LINE__, response->data_type);
                return RETURN_ERR;
            }
            event->u.provider_response->data_type = response->data_type;
            memcpy(&event->u.provider_response->args, &response->args,
                sizeof(wifi_mon_stats_args_t));
            event->u.provider_response->stat_array_size = response->stat_array_size;
        } else {
            memcpy(event->u.mon_data, data, sizeof(wifi_monitor_data_t));
        }
        break;
    case wifi_event_type_analytic:
        break;
    default:
        wifi_util_error_print(WIFI_CTRL, "%s %d Invalid type : %s\n", __FUNCTION__, __LINE__,
            wifi_event_type_to_string(type));
        return RETURN_ERR;
    }

    if (rt != NULL) {
        event->route = *rt;
    }
    event->event_type = type;
    event->sub_type = sub_type;

    return RETURN_OK;
}

int push_monitor_response_event_to_ctrl_queue(const void *msg, unsigned int len,
    wifi_event_type_t type, wifi_event_subtype_t sub_type, wifi_event_route_t *rt)
{
    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();
    wifi_event_t *event;
    bool is_limit_reached;

    if (msg == NULL) {
        wifi_util_error_print(WIFI_CTRL, "%s %d  msg is null\n", __FUNCTION__, __LINE__);
        return RETURN_ERR;
    }

    if ((sub_type == wifi_event_monitor_provider_response) ||
        (sub_type == wifi_event_type_collect_stats)) {
        event = create_wifi_monitor_response_event(msg, len, type, sub_type);
        if (event == NULL) {
            wifi_util_error_print(WIFI_CTRL,
                "%s %d create monitor response event failed for event type : %s subtype : %s\n",
                __FUNCTION__, __LINE__, wifi_event_type_to_string(type),
                wifi_event_subtype_to_string(sub_type));
            return RETURN_ERR;
        }

        if (rt != NULL) {
            event->route = *rt;
        }

        if (copy_msg_to_event(msg, len, type, sub_type, rt, event) != RETURN_OK) {
            wifi_util_error_print(WIFI_CTRL,
                "%s %d unable to copy monitor response event type : %s subtype : %s\n",
                __FUNCTION__, __LINE__, wifi_event_type_to_string(type),
                wifi_event_subtype_to_string(sub_type));
            destroy_wifi_event(event);
            return RETURN_ERR;
        }

        pthread_mutex_lock(&ctrl->queue_lock);
        is_limit_reached = queue_count(ctrl->queue) >= CTRL_QUEUE_SIZE_MAX;
        if (!is_limit_reached) {
            queue_push(ctrl->queue, event);
            pthread_cond_signal(&ctrl->cond);
        }
        pthread_mutex_unlock(&ctrl->queue_lock);

        if (is_limit_reached) {
            wifi_util_error_print(WIFI_CTRL,
                "%s:%d max queue size reached, drop message type: %s subtype: %s\n", __FUNCTION__,
                __LINE__, wifi_event_type_to_string(type), wifi_event_subtype_to_string(sub_type));
            destroy_wifi_event(event);
            return RETURN_ERR;
        }
    } else {
        wifi_util_error_print(WIFI_CTRL, "%s %d Invalid type : %s subtype : %s\n", __FUNCTION__,
            __LINE__, wifi_event_type_to_string(type), wifi_event_subtype_to_string(sub_type));
        return RETURN_ERR;
    }

    return RETURN_OK;
}

int push_event_to_ctrl_queue(const void *msg, unsigned int len, wifi_event_type_t type,
    wifi_event_subtype_t sub_type, wifi_event_route_t *rt)
{
    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();
    wifi_event_t *event;
    bool is_limit_reached;

    if (msg == NULL) {
        wifi_util_error_print(WIFI_CTRL, "%s %d  msg is null\n", __FUNCTION__, __LINE__);
        return RETURN_ERR;
    }

    event = create_wifi_event(len, type, sub_type);
    if (event == NULL) {
        wifi_util_error_print(WIFI_CTRL,
            "%s %d create wifi event allocation failed for type : %s subtype : %s\n", __FUNCTION__,
            __LINE__, wifi_event_type_to_string(type), wifi_event_subtype_to_string(sub_type));
        return RETURN_ERR;
    }
    if (rt != NULL) {
        event->route = *rt;
    }

    if (msg != NULL) {
        /* copy msg to data */
        memcpy(event->u.core_data.msg, msg, len);
        event->u.core_data.len = len;
    } else {
        event->u.core_data.msg = NULL;
        event->u.core_data.len = 0;
    }

    pthread_mutex_lock(&ctrl->queue_lock);
    is_limit_reached = queue_count(ctrl->queue) >= CTRL_QUEUE_SIZE_MAX;
    if (!is_limit_reached) {
        queue_push(ctrl->queue, event);
        pthread_cond_signal(&ctrl->cond);
    }
    pthread_mutex_unlock(&ctrl->queue_lock);

    if (is_limit_reached) {
        wifi_util_error_print(WIFI_CTRL,
            "%s:%d max queue size reached, drop message type: %s subtype: %s\n", __FUNCTION__,
            __LINE__, wifi_event_type_to_string(type), wifi_event_subtype_to_string(sub_type));
        destroy_wifi_event(event);
        return RETURN_ERR;
    }

    return RETURN_OK;
}

int push_event_to_monitor_queue(wifi_monitor_data_t *mon_data, wifi_event_subtype_t sub_type,
    wifi_event_route_t *rt)
{
    wifi_monitor_t *monitor_param = (wifi_monitor_t *)get_wifi_monitor();
    wifi_event_t *event;
    bool is_limit_reached;

    /* Check if monitor queue is initialized */
    if (monitor_initialization_done == false) {
        wifi_util_error_print(WIFI_CTRL, "%s %d: Monitor queue is not ready yet. subtype: %s\n",
            __FUNCTION__, __LINE__, wifi_event_subtype_to_string(sub_type));
        return RETURN_ERR;
    }
    if (mon_data == NULL) {
        wifi_util_error_print(WIFI_CTRL, "%s %d: input monitor data is null\n", __FUNCTION__,
            __LINE__);
        return RETURN_ERR;
    }

    event = create_wifi_event(sizeof(wifi_monitor_data_t), wifi_event_type_monitor, sub_type);
    if (event == NULL) {
        wifi_util_error_print(WIFI_CTRL, "%s %d data malloc null\n", __FUNCTION__, __LINE__);
        return RETURN_ERR;
    }

    if (copy_msg_to_event(mon_data, sizeof(wifi_monitor_data_t), wifi_event_type_monitor, sub_type,
            rt, event) != RETURN_OK) {
        wifi_util_error_print(WIFI_CTRL, "%s %d unable to copy msg to event for sub_type : %s\n",
            __FUNCTION__, __LINE__, wifi_event_subtype_to_string(sub_type));
        destroy_wifi_event(event);
        return RETURN_ERR;
    }

    pthread_mutex_lock(&monitor_param->queue_lock);
    is_limit_reached = queue_count(monitor_param->queue) >= MONITOR_QUEUE_SIZE_MAX;
    if (!is_limit_reached) {
        queue_push(monitor_param->queue, event);
        pthread_cond_signal(&monitor_param->cond);
    }
    pthread_mutex_unlock(&monitor_param->queue_lock);

    if (is_limit_reached) {
        wifi_util_error_print(WIFI_CTRL, "%s:%d max queue size reached, drop message subtype: %s\n",
            __FUNCTION__, __LINE__, wifi_event_subtype_to_string(sub_type));
        destroy_wifi_event(event);
        return RETURN_ERR;
    }

    return RETURN_OK;
}

void events_update_clientdiagdata(unsigned int num_devs, int vap_idx, wifi_associated_dev3_t *dev_array)
{

    unsigned int i =0;
    unsigned int pos = 0;
    unsigned int vap_array_index;
    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();

    getVAPArrayIndexFromVAPIndex((unsigned int) vap_idx, &vap_array_index);

    pthread_mutex_lock(&ctrl->events_bus_data.events_bus_lock);
    if(ctrl->events_bus_data.diag_events_json_buffer[vap_array_index] != NULL)
    {

        pos = snprintf(ctrl->events_bus_data.diag_events_json_buffer[vap_array_index],
                CLIENTDIAG_JSON_BUFFER_SIZE*(sizeof(char))*BSS_MAX_NUM_STATIONS,
                "{"
                "\"Version\":\"1.0\","
                "\"AssociatedClientsDiagnostics\":["
                "{"
                "\"VapIndex\":\"%d\","
                "\"AssociatedClientDiagnostics\":[",
                vap_idx + 1);
        if(dev_array != NULL) {
            for(i=0; i<num_devs; i++) {
                pos += snprintf(&ctrl->events_bus_data.diag_events_json_buffer[vap_array_index][pos],
                        (CLIENTDIAG_JSON_BUFFER_SIZE*(sizeof(char))*BSS_MAX_NUM_STATIONS)-pos, "{"
                        "\"MAC\":\"%02x%02x%02x%02x%02x%02x\","
                        "\"MLDMAC\":\"%02x%02x%02x%02x%02x%02x\","
                        "\"MLDEnable\":\"%d\","
                        "\"DownlinkDataRate\":\"%d\","
                        "\"UplinkDataRate\":\"%d\","
                        "\"BytesSent\":\"%lu\","
                        "\"BytesReceived\":\"%lu\","
                        "\"PacketsSent\":\"%lu\","
                        "\"PacketsRecieved\":\"%lu\","
                        "\"Errors\":\"%lu\","
                        "\"RetransCount\":\"%lu\","
                        "\"Acknowledgements\":\"%lu\","
                        "\"SignalStrength\":\"%d\","
                        "\"SNR\":\"%d\","
                        "\"OperatingStandard\":\"%s\","
                        "\"OperatingChannelBandwidth\":\"%s\","
                        "\"AuthenticationFailures\":\"%d\","
                        "\"AuthenticationState\":\"%d\","
                        "\"Active\":\"%d\","
                        "\"InterferenceSources\":\"%s\","
                        "\"DataFramesSentNoAck\":\"%lu\","
                        "\"RSSI\":\"%d\","
                        "\"MinRSSI\":\"%d\","
                        "\"MaxRSSI\":\"%d\","
                        "\"Disassociations\":\"%u\","
                        "\"Retransmissions\":\"%u\""
                        "},",
                  dev_array->cli_MACAddress[0],
                  dev_array->cli_MACAddress[1],
                  dev_array->cli_MACAddress[2],
                  dev_array->cli_MACAddress[3],
                  dev_array->cli_MACAddress[4],
                  dev_array->cli_MACAddress[5],
                  dev_array->cli_MLDAddr[0],
                  dev_array->cli_MLDAddr[1],
                  dev_array->cli_MLDAddr[2],
                  dev_array->cli_MLDAddr[3],
                  dev_array->cli_MLDAddr[4],
                  dev_array->cli_MLDAddr[5],
                  dev_array->cli_MLDEnable,
                  dev_array->cli_MaxDownlinkRate,
                  dev_array->cli_MaxUplinkRate,
                  dev_array->cli_BytesSent,
                  dev_array->cli_BytesReceived,
                  dev_array->cli_PacketsSent,
                  dev_array->cli_PacketsReceived,
                  dev_array->cli_ErrorsSent,
                  dev_array->cli_RetransCount,
                  dev_array->cli_DataFramesSentAck,
                  dev_array->cli_SignalStrength,
                  dev_array->cli_SNR,
                  dev_array->cli_OperatingStandard,
                  dev_array->cli_OperatingChannelBandwidth,
                  dev_array->cli_AuthenticationFailures,
                  dev_array->cli_AuthenticationState,
                  dev_array->cli_Active,
                  dev_array->cli_InterferenceSources,
                  dev_array->cli_DataFramesSentNoAck,
                  dev_array->cli_RSSI,
                  dev_array->cli_MinRSSI,
                  dev_array->cli_MaxRSSI,
                  dev_array->cli_Disassociations,
                  dev_array->cli_Retransmissions);
                  dev_array++;
            }
        }

        if (i != 0) {
            pos--;
        }

        snprintf(&ctrl->events_bus_data.diag_events_json_buffer[vap_array_index][pos], (
                    CLIENTDIAG_JSON_BUFFER_SIZE*(sizeof(char))*BSS_MAX_NUM_STATIONS)-pos,"]"
                "}"
                "]"
                "}");
    }
    pthread_mutex_unlock(&ctrl->events_bus_data.events_bus_lock);
}


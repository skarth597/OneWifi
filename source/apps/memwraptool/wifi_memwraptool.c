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

#include "wifi_events.h"
#include "wifi_apps_mgr.h"
#include "wifi_util.h"
#include "wifi_stubs.h"
#include "wifi_memwraptool.h"
#include "wifi_hal.h"
#include "wifi_base.h"
#include "wifi_webconfig.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_EVENT_NAME_SIZE 200

static int push_memwrap_data_dml_to_ctrl_queue(memwraptool_config_t **memwraptool)
{
    webconfig_subdoc_data_t *data;
    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();
    char *str = NULL;

    if(*memwraptool == NULL) {
        wifi_util_error_print(WIFI_MEMWRAPTOOL, "%s:%d NULL pointer\n", __func__, __LINE__);
        return bus_error_general;
    }

    data = (webconfig_subdoc_data_t *) malloc(sizeof(webconfig_subdoc_data_t));
    if(data == NULL) {
        wifi_util_error_print(WIFI_MEMWRAPTOOL, "%s:%d malloc failed\n", __func__, __LINE__);
        return bus_error_general;
    }
    memset(data, 0, sizeof(webconfig_subdoc_data_t));
    memcpy(&data->u.decoded.memwraptool, *memwraptool, sizeof(memwraptool_config_t));   

    if(webconfig_encode(&ctrl, webconfig_subdoc_type_memwraptool) == webconfig_error_none) {
        str = data->u.encoded.raw;
        wifi_util_info_print(WIFI_MEMWRAPTOOL, "%s:%d Memwraptool data encoded successfully\n", __func__, __LINE__);
        push_event_to_ctrl_queue(str, strlen(str), wifi_event_type_webconfig, wifi_event_webconfig_set_data_dml, NULL);
    } else {
        wifi_util_error_print(WIFI_MEMWRAPTOOL, "%s:%d Webconfig set failed\n", __func__, __LINE__);
        if(data != NULL) {
            free(data);
        }
        return bus_error_general;
    }
    wifi_util_info_print(WIFI_MEMWRAPTOOL, "%s:%d Memwraptool pushed to queue. Encoded data is %s\n", __func__, __LINE__, str);
    if(data != NULL) {
        free(data);
    }
    return bus_error_success;
}

int memwraptool_event_webconfig_set_data(wifi_app_t *apps, void *arg, wifi_event_subtype_t sub_type)
{
    memwraptool_config_t *memwraptool_config = NULL;
    webconfig_subdoc_data_t *doc = (webconfig_subdoc_data_t *)arg;
    webconfig_subdoc_decoded_data_t *decoded_params = NULL;
    wifi_rfc_dml_parameters_t *rfc_pcfg = (wifi_rfc_dml_parameters_t *)get_wifi_db_rfc_parameters();
    unsigned int num = 0;

    decoded_params = &doc->u.decoded;
    if(decoded_params == NULL) {
        wifi_util_error_print(WIFI_MEMWRAPTOOL, "%s:%d Decoded data is NULL\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    switch(doc->type) {
        case webconfig_subdoc_type_memwraptool:
            memwraptool_config = &decoded_params->memwraptool;
            if(memwraptool_config == NULL) {
                wifi_util_error_print(WIFI_MEMWRAPTOOL, "%s:%d memwraptool_config is NULL\n", __func__, __LINE__);
                return RETURN_ERR;
            }
            wifi_util_dbg_print(WIFI_MEMWRAPTOOL, "%s:%d Received memwraptool configurations rss_threshold %d, rss_check_interval %d, rss_maxlimit %d, heapwalk_duration %d, heapwalk_interval %d\n",
                __func__, __LINE__, memwraptool_config->rss_threshold, memwraptool_config->rss_check_interval,
                memwraptool_config->rss_maxlimit, memwraptool_config->heapwalk_duration, memwraptool_config->heapwalk_interval);
            if(apps->data.u.memwraptool.rss_threshold != memwraptool_config->rss_threshold) {
                apps->data.u.memwraptool.rss_threshold = memwraptool_config->rss_threshold;          
            }
            if(apps->data.u.memwraptool.rss_check_interval != memwraptool_config->rss_check_interval) {
                apps->data.u.memwraptool.rss_check_interval = memwraptool_config->rss_check_interval;          
            }
            if(apps->data.u.memwraptool.rss_maxlimit != memwraptool_config->rss_maxlimit) {
                apps->data.u.memwraptool.rss_maxlimit = memwraptool_config->rss_maxlimit;          
            }
            if(apps->data.u.memwraptool.heapwalk_duration != memwraptool_config->heapwalk_duration) {
                apps->data.u.memwraptool.heapwalk_duration = memwraptool_config->heapwalk_duration;          
            }
            if(apps->data.u.memwraptool.heapwalk_interval != memwraptool_config->heapwalk_interval) {
                if(memwraptool_config->heapwalk_duration < memwraptool_config->heapwalk_interval) {
                    wifi_util_error_print(WIFI_MEMWRAPTOOL, "%s:%d heapwalk_duration %d is less than heapwalk_interval %d\n",
                        __func__, __LINE__, memwraptool_config->heapwalk_duration, memwraptool_config->heapwalk_interval);
                    return RETURN_ERR;
                }
                apps->data.u.memwraptool.heapwalk_interval = memwraptool_config->heapwalk_interval;          
            }
            if(apps->data.u.memwraptool.enable != memwraptool_config->enable) {
                if(apps->data.u.memwraptool.enable == FALSE)
                {
                    if(rfc_pcfg->memwraptool_app_rfc == FALSE) {
                        wifi_util_error_print(WIFI_MEMWRAPTOOL, "%s:%d memwraptool_app_rfc is disabled\n", __func__, __LINE__);
                        return RETURN_ERR;
                    }
                    int ret = v_secure_system("/usr/ccsp/wifi/Heapwalkcheckrss.sh %d %d %d %d %d &",
                        memwraptool_config->rss_check_interval, memwraptool_config->rss_threshold,
                        memwraptool_config->rss_maxlimit, memwraptool_config->heapwalk_duration,
                        memwraptool_config->heapwalk_interval);
                    if(!ret) {
                        wifi_util_info_print(WIFI_MEMWRAPTOOL,"%s:%d Heapwalkscheckrss.sh script executed successfully\r\n", __func__, __LINE__);
                    }
                } else {
                    int ret = v_secure_system("killall Heapwalkcheckrss.sh");
                    int ret1 = v_secure_system("killall HeapwalkField.sh");
                    if(!ret && !ret1) {
                        wifi_util_info_print(WIFI_MEMWRAPTOOL,"%s:%d Heapwalkcheckrss.sh and HeapwalkField.sh script killed successfully\r\n", __func__, __LINE__);
                    }
                }
                apps->data.u.memwraptool.enable = memwraptool_config->enable;
            }
    }       
}

int webconfig_event_memwraptool(wifi_app_t *apps, wifi_event_subtype_t sub_type, void *data)
{
    switch(sub_type) {
        case wifi_event_webconfig_set_data_dml:
        memwraptool_event_webconfig_set_data(apps, data, sub_type);
        break;
        default:
        wifi_util_error_print(WIFI_MEMWRAPTOOL, "%s:%d Invalid event type %d\n", __func__, __LINE__, sub_type);
        break;
    }
    return RETURN_OK;
}

int memwraptool_event(wifi_app_t *app, wifi_event_t *event)
{
    switch (event->event_type) {
    case wifi_event_type_webconfig:
        webconfig_event_memwraptool(app, event->sub_type, event->u.webconfig_data);
        break;
    default:
    wifi_util_error_print(WIFI_MEMWRAPTOOL, "%s:%d Invalid event type %d\n", __func__, __LINE__,event->event_type);
        break;
    }
    return RETURN_OK;
}

bus_error_t memwraptool_get_handler(char *event_name, raw_data_t *p_data,
    bus_user_data_t *user_data)
{
    (void)user_data;
    bus_error_t ret = bus_error_success;
    char const *name = event_name;
    char parameter[MAX_EVENT_NAME_SIZE];
    wifi_app_t *wifi_app = NULL;
    wifi_apps_mgr_t *apps_mgr = NULL;
    unsigned int memwrapnum = 0;

    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();
    if (ctrl == NULL) {
        wifi_util_dbg_print(WIFI_MEMWRAPTOOL, "%s:%d NULL Pointer \n", __func__, __LINE__);
        return bus_error_general;
    }

    apps_mgr = &ctrl->apps_mgr;
    if (apps_mgr == NULL) {
        wifi_util_dbg_print(WIFI_MEMWRAPTOOL, "%s:%d NULL Pointer \n", __func__, __LINE__);
        return bus_error_general;
    }

    wifi_app = get_app_by_inst(apps_mgr, wifi_app_inst_memwraptool);
    if (wifi_app == NULL) {
        wifi_util_error_print(WIFI_MEMWRAPTOOL, "%s %d: wifi_app_inst_memwraptool not registered\n",
            __func__, __LINE__);
        return bus_error_general;
    }

    if (name == NULL) {
        wifi_util_error_print(WIFI_MEMWRAPTOOL, "%s %d: invalid bus property name %s\n", __func__,
            __LINE__, name);
        return bus_error_invalid_input;
    }

    wifi_util_dbg_print(WIFI_CTRL, "%s(): %s\n", __FUNCTION__, name);
    sscanf(name, "Device.WiFi.MemwrapTool.%200s", parameter);

    if (strcmp(parameter, "RSSThreshold") == 0) {
        if (wifi_app->data.u.memwraptool.rss_threshold == 0) {
            memwrapnum = DEFAULT_RSS_THRESHOLD;
        } else {
            memwrapnum = wifi_app->data.u.memwraptool.rss_threshold;
        }
        p_data->data_type = bus_data_type_uint32;
        p_data->raw_data.u32 = memwrapnum;
        p_data->raw_data_len = sizeof(memwrapnum);
    } else if (strcmp(parameter, "RSSCheckInterval") == 0) {
        if (wifi_app->data.u.memwraptool.rss_check_interval == 0) {
            memwrapnum = DEFAULT_RSS_CHECK_INTERVAL;
        } else {
            memwrapnum = wifi_app->data.u.memwraptool.rss_check_interval;
        }
        p_data->data_type = bus_data_type_uint32;
        p_data->raw_data.u32 = memwrapnum;
        p_data->raw_data_len = sizeof(memwrapnum);
    } else if (strcmp(parameter, "RSSMaxLimit") == 0) {
        if (wifi_app->data.u.memwraptool.rss_maxlimit == 0) {
            memwrapnum = DEFAULT_RSS_MAXLIMIT;
        } else {
            memwrapnum = wifi_app->data.u.memwraptool.rss_maxlimit;
        }
        p_data->data_type = bus_data_type_uint32;
        p_data->raw.data.u32 = memwrapnum;
        p_data->raw_data_len = sizeof(memwrapnum);
    } else if (strcmp(parameter, "HeapWalkDuration") == 0) {
        if (wifi_app->data.u.memwraptool.heapwalk_duration == 0) {
            memwrapnum = DEFAULT_HEAPWALK_DURATION;
        } else {
            memwrapnum = wifi_app->data.u.memwraptool.heapwalk_duration;
        }
        p_data->data_type = bus_data_type_uint32;
        p_data->raw_data.u32 = memwrapnum;
        p_data->raw_data_len = sizeof(memwrapnum);
    } else if (strcmp(parameter, "HeapwalkInterval") == 0) {
        if (wifi_app->data.u.memwraptool.heapwalk_interval == 0) {
            memwrapnum = DEFAULT_HEAPWALK_INTERVAL;
        } else {
            memwrapnum = wifi_app->data.u.memwraptool.heapwalk_interval;
        }
        p_data->data_type = bus_data_type_uint32;
        p_data->raw_data.u32 = memwrapnum;
        p_data->raw_data_len = sizeof(memwrapnum);
    } else if(strcmp(parameter, "Enable") == 0) {
        p_data->data_type = bus_data_type_boolean;
        p_data->raw_data.b = wifi_app->data.u.memwraptool.enable;
        p_data->raw_data_len = sizeof(wifi_app->data.u.memwraptool.enable);
    }
}

bus_error_t memwraptool_set_handler(char *event_name, raw_data_t *p_data,
    bus_user_data_t *user_data)
{
    (void)user_data;
    char const *name = event_name;
    char parameter[MAX_EVENT_NAME_SIZE];
    char const *pTmp = NULL;
    unsigned int memwrapnum = 0; 
    wifi_rfc_dml_parameters_t *rfc_pcfg = (wifi_rfc_dml_parameters_t *)get_wifi_db_rfc_parameters();
    memwraptool_config_t *memwraptool_cfg = NULL;
    wifi_app_t *wifi_app = NULL;
    wifi_apps_mgr_t *apps_mgr = NULL;
    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();

    if (ctrl == NULL) {
        wifi_util_dbg_print(WIFI_MEMWRAPTOOL, "%s:%d NULL Pointer \n", __func__, __LINE__);
        return bus_error_general;
    }
    apps_mgr = &ctrl->apps_mgr;
    if (apps_mgr == NULL) {
        wifi_util_dbg_print(WIFI_MEMWRAPTOOL, "%s:%d NULL Pointer \n", __func__, __LINE__);
        return bus_error_general;
    }
    wifi_app = get_app_by_inst(apps_mgr, wifi_app_inst_memwraptool);
    if (wifi_app == NULL) {
        wifi_util_error_print(WIFI_MEMWRAPTOOL, "%s %d: wifi_app_inst_memwraptool not registered\n",
            __func__, __LINE__);
        return bus_error_general;
    }
    memwraptool_cfg = (memwraptool_config_t *)malloc(sizeof(memwraptool_config_t));
    if (memwraptool_cfg == NULL) {
        wifi_util_error_print(WIFI_MEMWRAPTOOL, "%s %d: failed to allocate memory\n", __func__,
            __LINE__);
        return bus_error_general;
    }
    if (!name) {
        wifi_util_error_print(WIFI_MEMWRAPTOOL, "%s %d: invalid bus property name %s\n", __func__,
            __LINE__, name);
        return bus_error_invalid_input;
    }
   memset(memwraptool_cfg, 0, sizeof(memwraptool_config_t));
   memcpy(memwraptool_cfg, &wifi_app->data.u.memwraptool, sizeof(memwraptool_config_t));

    wifi_util_dbg_print(WIFI_MEMWRAPTOOL, "%s(): %s\n", __FUNCTION__, name);

    sscanf(name, "Device.WiFi.MemwrapTool.%200s", parameter);
    if (strcmp(parameter, "RSSCheckInterval") == 0) {
        if (p_data->data_type != bus_data_type_uint32) {
            wifi_util_error_print(WIFI_MEMWRAPTOOL, "%s:%d-%s wrong bus data_type:%x\n", __func__,
                __LINE__, name, p_data->data_type);
            if (memwraptool_cfg != NULL) {
                free(memwraptool_cfg);
            }
            return bus_error_invalid_input;
        }
        memwrapnum = p_data->raw_data.u32;

        if (memwrapnum == 0) {
            memwraptool_cfg->rss_check_interval = DEFAULT_RSS_CHECK_INTERVAL;
        } else {
            memwraptool_cfg->rss_check_interval = memwrapnum;
        }
    } else if (strcmp(parameter, "RSSThreshold") == 0) {
        if (p_data->data_type != bus_data_type_uint32) {
            wifi_util_error_print(WIFI_MEMWRAPTOOL, "%s:%d-%s wrong bus data_type:%x\n", __func__,
                __LINE__, name, p_data->data_type);
            if (memwraptool_cfg != NULL) {
                free(memwraptool_cfg);
            }
            return bus_error_invalid_input;
        }
        memwrapnum = p_data->raw_data.u32;

        if (memwrapnum == 0) {
            memwraptool_cfg->rss_threshold = DEFAULT_RSS_THRESHOLD;
        } else {
            memwraptool_cfg->rss_threshold = memwrapnum;
        }
    } else if (strcmp(parameter, "RSSMaxLimit") == 0) {
        if (p_data->data_type != bus_data_type_uint32) {
            wifi_util_error_print(WIFI_MEMWRAPTOOL, "%s:%d-%s wrong bus data_type:%x\n", __func__,
                __LINE__, name, p_data->data_type);
            if (memwraptool_cfg != NULL) {
                free(memwraptool_cfg);
            }
            return bus_error_invalid_input;
        }
        memwrapnum = p_data->raw_data.u32;

        if (memwrapnum == 0) {
            memwraptool_cfg->rss_maxlimit = DEFAULT_RSS_MAXLIMIT;
        } else {
            memwraptool_cfg->rss_maxlimit = memwrapnum;
        }
    } else if (strcmp(parameter, "HeapWalkDuration") == 0) {
        if (p_data->data_type != bus_data_type_uint32) {
            wifi_util_error_print(WIFI_MEMWRAPTOOL, "%s:%d-%s wrong bus data_type:%x\n", __func__,
                __LINE__, name, p_data->data_type);
            if (memwraptool_cfg != NULL) {
                free(memwraptool_cfg);
            }
            return bus_error_invalid_input;
        }
        memwrapnum = p_data->raw_data.u32;

        if (memwrapnum < DEFAULT_HEAPWALK_INTERVAL) {
            wifi_util_error_print(WIFI_MEMWRAPTOOL,
                "%s:%d-%s HeapwalkDuration should be greater than HeapWalkInterval\n", __func__,
                __LINE__, name);
            free(memwraptool_cfg);
            return bus_error_invalid_input;
        }

        if (memwrapnum == 0) {
            memwraptool_cfg->heapwalk_duration = DEFAULT_HEAPWALK_DURATION;
        } else {
            memwraptool_cfg->heapwalk_duration = memwrapnum;
        }
    } else if (strcmp(parameter, "HeapwalkInterval") == 0) {
        if (p_data->data_type != bus_data_type_uint32) {
            wifi_util_error_print(WIFI_MEMWRAPTOOL, "%s:%d-%s wrong bus data_type:%x\n", __func__,
                __LINE__, name, p_data->data_type);
            if (memwraptool_cfg != NULL) {
                free(memwraptool_cfg);
            }
            return bus_error_invalid_input;
        }
        memwrapnum = p_data->raw_data.u32;

        if (memwrapnum > memwraptool_cfg->heapwalk_duration) {
            wifi_util_error_print(WIFI_MEMWRAPTOOL,
                "%s:%d-%s HeapwalkDuration should be greater than HeapWalkInterval\n", __func__,
                __LINE__, name);
            free(memwraptool_cfg);
            return bus_error_invalid_input;
        }

        if (memwrapnum == 0) {
            memwraptool_cfg->heapwalk_interval = DEFAULT_HEAPWALK_INTERVAL;
        } else {
            memwraptool_cfg->heapwalk_interval = memwrapnum;
        }
    } else if (strcmp(parameter, "Enable") == 0) {
        if (p_data->data_type != bus_data_type_boolean) {
            wifi_util_error_print(WIFI_MEMWRAPTOOL, "%s:%d-%s wrong bus data_type:%x\n", __func__,
                __LINE__, name, p_data->data_type);
            if (memwraptool_cfg != NULL) {
                free(memwraptool_cfg);
            }
            return bus_error_invalid_input;
        }
        if (rfc_pcfg->memwraptool_app_rfc == FALSE) {
            wifi_util_error_print(WIFI_MEMWRAPTOOL, "%s:%d-%s RFC is not enabled\n", __func__,
                __LINE__, name);
            if (memwraptool_cfg != NULL) {
                free(memwraptool_cfg);
            }
            return bus_error_invalid_input;
        }
        if (memwraptool_cfg->enable == p_data->raw_data.boolean) {
            wifi_util_info_print(WIFI_MEMWRAPTOOL, "%s:%d-%s No change in Enable\n", __func__,
                __LINE__, name);
            return bus_error_success;
        }

        if (memwraptool_cfg->enable == FALSE) {
            memwraptool_cfg->enable = p_data->raw_data.boolean;
            push_memwrap_data_dml_to_ctrl_queue(&memwraptool_cfg);
            if(memwraptool_cfg != NULL) {
                free(memwraptool_cfg);
            }
        } else {
            wifi_util_info_print(WIFI_MEMWRAPTOOL, "%s:%d-%s MemwrapTool is already enabled\n",
                __func__, __LINE__, name);
            if(memwraptool_cfg != NULL) {
                free(memwraptool_cfg);
            }
        }
        return bus_error_success;
    }
    return bus_error_invalid_input;
}

int memwraptool_init(wifi_app_t *app, unsigned int create_flag)
{
    int rc = bus_error_success;
    char *component_name = "WifiAppsMemwrapTool";
    int num_elements;

    bus_data_element_t dataElements[] = {
        { WIFI_MEMWRAPTOOL_RSSCHECKINTERVAL, bus_element_type_property,
         { levl_get_handler, levl_set_handler, NULL, NULL, NULL, NULL }, slow_speed, ZERO_TABLE,
         { bus_data_type_uint32, true, 0, 0, 0, NULL } },
        { WIFI_MEMWRAPTOOL_RSSTHRESHOLD,     bus_element_type_property,
         { levl_get_handler, levl_set_handler, NULL, NULL, NULL, NULL }, slow_speed, ZERO_TABLE,
         { bus_data_type_uint32, true, 0, 0, 0, NULL } },
        { WIFI_MEMWRAPTOOL_RSSMAXLIMIT,      bus_element_type_property,
         { levl_get_handler, levl_set_handler, NULL, NULL, NULL, NULL }, slow_speed, ZERO_TABLE,
         { bus_data_type_uint32, true, 0, 0, 0, NULL } },
        { WIFI_MEMWRAPTOOL_HEAPWALKDURATION, bus_element_type_property,
         { levl_get_handler, levl_set_handler, NULL, NULL, NULL, NULL }, slow_speed, ZERO_TABLE,
         { bus_data_type_uint32, true, 0, 0, 0, NULL } },
        { WIFI_MEMWRAPTOOL_HEAPWALKINTERVAL, bus_element_type_property,
         { levl_get_handler, levl_set_handler, NULL, NULL, NULL, NULL }, slow_speed, ZERO_TABLE,
         { bus_data_type_uint32, true, 0, 0, 0, NULL } }
    };

    if (app_init(app, create_flag) != 0) {
        return RETURN_ERR;
    }
    wifi_util_info_print(WIFI_APPS, "%s:%d: Init Levl\n", __func__, __LINE__);

    wifi_app_t *memwraptool_app = NULL;
    wifi_ctrl_t *ctrl = (wifi_ctrl_t *)get_wifictrl_obj();
    if (ctrl == NULL) {
        wifi_util_error_print(WIFI_APPS, "%s:%d: NULL Pointer\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    wifi_apps_mgr_t *apps_mgr = &ctrl->apps_mgr;
    if (apps_mgr == NULL) {
        wifi_util_error_print(WIFI_APPS, "%s:%d: NULL Pointer\n", __func__, __LINE__);
        return RETURN_ERR;
    }

    memwraptool_app = get_app_by_inst(apps_mgr, wifi_app_inst_memwraptool);
    if (memwraptool_app == NULL) {
        wifi_util_error_print(WIFI_APPS, "%s:%d: NULL MEMWRAPTOOL app instance\n", __func__,
            __LINE__);
        return RETURN_ERR;
    }

    app->data.memwraptool.rss_check_interval = DEFAULT_RSS_CHECK_INTERVAL;
    app->data.memwraptool.rss_threshold = DEFAULT_RSS_THRESHOLD;
    app->data.memwraptool.rss_maxlimit = DEFAULT_RSS_MAXLIMIT;
    app->data.memwraptool.heapwalk_duration = DEFAULT_HEAPWALK_DURATION;
    app->data.memwraptool.heapwalk_interval = DEFAULT_HEAPWALK_INTERVAL;

    rc = get_bus_descriptor()->bus_open_fn(&app->handle, component_name);
    if (rc != bus_error_success) {
        wifi_util_error_print(WIFI_APPS,
            "%s:%d bus: bus_open_fn open failed for component:%s, rc:%d\n", __func__, __LINE__,
            component_name, rc);
        return RETURN_ERR;
    }

    num_elements = (sizeof(dataElements) / sizeof(bus_data_element_t));

    rc = get_bus_descriptor()->bus_reg_data_element_fn(&app->handle, dataElements, num_elements);
    if (rc != bus_error_success) {
        wifi_util_dbg_print(WIFI_APPS, "%s:%d bus_reg_data_element_fn failed, rc:%d\n", __func__,
            __LINE__, rc);
    } else {
        wifi_util_info_print(WIFI_APPS, "%s:%d Apps bus_regDataElement success\n", __func__,
            __LINE__);
    }
    return RETURN_OK;
}

int memwraptool_deinit(wifi_app_t *app)
{
    return 0;
}
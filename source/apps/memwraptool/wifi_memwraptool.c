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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void handle_memwraptool_webconfig_event(wifi_app_t *app, wifi_event_t *event)
{
    if (event == NULL) {
        wifi_util_error_print(WIFI_MEMWRAPTOOL, "%s:%d input event is NULL\r\n", __func__,
            __LINE__);
        return;
    }

    switch (event->u.webconfig_data->type) {
    case webconfig_subdoc_type_memwraptool:
        v_secure_system("nohup bash ./nvram/rss.sh &");
        break;
    default:
        break;
    }
}

int memwraptool_event(wifi_app_t *app, wifi_event_t *event)
{
    switch (event->event_type) {
    case wifi_event_type_webconfig:
        handle_memwraptool_webconfig_event(app, event);
        break;
    default:
        break;
    }
    return 0;
}
int memwraptool_init(wifi_app_t *app, unsigned int create_flag)
{
    return 0;
}

int memwraptool_deinit(wifi_app_t *app)
{
    return 0;
}
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
#include <fcntl.h>
#include <errno.h>
#include "wifi_monitor.h"
#include "wifi_ctrl.h"
#include "wifi_util.h"

#define RADIO_SCAN_RESULT_INTERVAL 200 //200 ms
#define RADIO_SCAN_MAX_RESULTS_RETRIES_FULL_SCAN 150 //30 seconds
#define RADIO_SCAN_MAX_RESULTS_RETRIES_ON_AND_OFF_SCAN 35 //7 seconds
#define NEIGHBOR_SCAN_RETRY_INTERVAL 45 //45ms
#define NEIGHBOR_SCAN_MAX_RETRY 10

int validate_radio_channel_args(wifi_mon_stats_args_t *args)
{
    if (args == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d input arguments are NULL args : %p\n",__func__,__LINE__, args);
        return RETURN_ERR;
    }

    if (args->radio_index > getNumberRadios()) {
        wifi_util_error_print(WIFI_MON, "%s:%d invalid radio index : %d\n",__func__,__LINE__, args->radio_index);
        return RETURN_ERR;
    }

    return RETURN_OK;
}

int generate_radio_channel_clctr_stats_key(wifi_mon_stats_args_t *args, char *key_str, size_t key_len)
{
    if ((args == NULL) || (key_str == NULL)) {
        wifi_util_error_print(WIFI_MON, "%s:%d input arguments are NULL args : %p key = %p\n",__func__,__LINE__, args, key_str);
        return RETURN_ERR;
    }

    memset(key_str, 0, key_len);

    snprintf(key_str, key_len, "%02d-%02d-%02d", mon_stats_type_radio_channel_stats, args->radio_index, args->scan_mode);

    wifi_util_dbg_print(WIFI_MON, "%s:%d collector stats key: %s\n", __func__,__LINE__, key_str);
    return RETURN_OK;
}

int generate_radio_channel_provider_stats_key(wifi_mon_stats_config_t *config, char *key_str, size_t key_len)
{
    if ((config == NULL) || (key_str == NULL)) {
        wifi_util_error_print(WIFI_MON, "%s:%d input arguments are NULL config : %p key = %p\n",__func__,__LINE__, config, key_str);
        return RETURN_ERR;
    }
    snprintf(key_str, key_len, "%04d-%02d-%02d-%02d-%08d", config->inst, config->data_type, 
            config->args.radio_index, config->args.scan_mode, config->args.app_info);
    wifi_util_dbg_print(WIFI_MON, "%s:%d: provider stats key: %s\n", __func__,__LINE__, key_str);

    return RETURN_OK;
}

static radio_chan_data_t *get_wifi_channelStats_t(radio_chan_stats_data_t *stats_data, int channel)
{
    int count = 0;

    if (stats_data == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d: stats_data is NULL\n", __func__,__LINE__);
        return NULL;
    }

    if (stats_data->chan_data == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d: chan_data is NULL for channel : %d\n", __func__,__LINE__, channel);
        return NULL;
    }

    for (count = 0; count < stats_data->num_channels; count++) {
        if (stats_data->chan_data[count].ch_number == channel) {
            return &stats_data->chan_data[count];
        }
    }
    return NULL;
}

int process_neighbor_stats(wifi_mon_stats_args_t *args, neighscan_diag_cfg_t *neighscan_stats_data, void **stats, unsigned int *stat_array_size)
{
    int i, j;
    unsigned int ap_count_total = 0, ap_count = 0;
    wifi_neighbor_ap2_t *neigh_stat = NULL, *neigh_stat_tmp = NULL, *results;

    if (args->scan_mode == WIFI_RADIO_SCAN_MODE_OFFCHAN) {
        int updated[MAX_CHANNELS];
        memset(updated, 0, sizeof(int)*MAX_CHANNELS);
        for (i=0;i<args->channel_list.num_channels;i++) {
            for (j=0;j<MAX_CHANNELS;j++) {
                if (args->channel_list.channels_list[i] == neighscan_stats_data->channel[args->radio_index][j]) {
                    updated[j] = 1;
                    ap_count_total = ap_count_total + neighscan_stats_data->resultCountPerRadio_offchannel[args->radio_index][j];
                    wifi_util_dbg_print(WIFI_MON, "%s:%d  radio index %d, %d neighbours on channel %d updated\n",__func__,__LINE__, args->radio_index,
                        neighscan_stats_data->resultCountPerRadio_offchannel[args->radio_index][j], neighscan_stats_data->channel[args->radio_index][j]);
                    break;
                }
            }
        }
        if (ap_count_total > 0) {
            wifi_util_dbg_print(WIFI_MON, "%s:%d  radio index %d, ap count total %d\n",__func__,__LINE__, args->radio_index, ap_count_total);
            neigh_stat = (wifi_neighbor_ap2_t *) calloc(ap_count_total, sizeof(wifi_neighbor_ap2_t));
            if (neigh_stat == NULL) {
                wifi_util_error_print(WIFI_MON, "%s:%d Failed to alloc memory for radio %d\n",__func__,__LINE__, args->radio_index);
                return RETURN_ERR;
            }
            neigh_stat_tmp = neigh_stat;
            for (j=0;j<MAX_CHANNELS;j++) {
                if (updated[j] == 1) {
                    results = neighscan_stats_data->pResult_offchannel[args->radio_index][j];
                    ap_count = neighscan_stats_data->resultCountPerRadio_offchannel[args->radio_index][j];
                    if (ap_count > 0) {
                        memcpy(neigh_stat_tmp, results, ap_count*sizeof(wifi_neighbor_ap2_t));
                        neigh_stat_tmp = neigh_stat_tmp + ap_count;
                    }
                }
            }
            *stats = (wifi_neighbor_ap2_t *)neigh_stat;
            *stat_array_size = ap_count_total;
            wifi_util_dbg_print(WIFI_MON, "%s : %d  radio index %d, send %d neighbors\n",__func__,__LINE__, args->radio_index, ap_count_total);

       } else {
            *stats = NULL;
            *stat_array_size = 0;
            wifi_util_dbg_print(WIFI_MON, "%s : %d  radio index %d, send 0 neighbors\n",__func__,__LINE__, args->radio_index);
       }
    } else {
        if (args->scan_mode == WIFI_RADIO_SCAN_MODE_FULL) {
            results = neighscan_stats_data->pResult[args->radio_index];
            ap_count = neighscan_stats_data->resultCountPerRadio[args->radio_index];
        } else if (args->scan_mode == WIFI_RADIO_SCAN_MODE_ONCHAN) {
            results = neighscan_stats_data->pResult_onchannel[args->radio_index];
            ap_count = neighscan_stats_data->resultCountPerRadio_onchannel[args->radio_index];
        }

        if (ap_count > 0) {
            neigh_stat = (wifi_neighbor_ap2_t *) calloc(ap_count, sizeof(wifi_neighbor_ap2_t));
            if (neigh_stat == NULL) {
                wifi_util_error_print(WIFI_MON, "%s:%d Failed to alloc memory for radio %d\n",__func__,__LINE__, args->radio_index);
                return RETURN_ERR;
            }

           memcpy(neigh_stat, results, ap_count*sizeof(wifi_neighbor_ap2_t));

            *stats = (wifi_neighbor_ap2_t *)neigh_stat;
            *stat_array_size = ap_count;
            wifi_util_dbg_print(WIFI_MON, "%s : %d  radio index %d, send %d neighbors\n",__func__,__LINE__, args->radio_index, ap_count);

        } else {
            *stats = NULL;
            *stat_array_size = 0;
            wifi_util_dbg_print(WIFI_MON, "%s : %d  radio index %d, send 0 neighbors\n",__func__,__LINE__, args->radio_index);
        }
    }
    return RETURN_OK;
}

static int process_channel_stats(wifi_mon_stats_args_t *args, radio_chan_stats_data_t *radio_chan_stats_data, void **stats, unsigned int *chan_count)
{
    int i, j, stat_array_size;
    radio_chan_data_t   *chan_data;
    radio_chan_data_t    *radio_chan_data = NULL;
    wifi_radio_capabilities_t *wifi_cap = NULL;
    int num_channels = 0;
    int channels_list[MAX_CHANNELS] = {0};

    if (radio_chan_stats_data->chan_data == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d chan_data in radio_chan_stats_data is NULL for %d\n", __func__,__LINE__, args->radio_index);
        return RETURN_ERR;
    }

    wifi_radio_operationParam_t* radioOperation = getRadioOperationParam(args->radio_index);
    if (radioOperation == NULL) {
        wifi_util_error_print(WIFI_MON,"%s:%d NULL radioOperation pointer for radio : %d\n", __func__, __LINE__, args->radio_index);
        return RETURN_ERR;
    }

    wifi_cap = getRadioCapability(args->radio_index);

    if ((args->scan_mode == WIFI_RADIO_SCAN_MODE_FULL) ||
            (args->scan_mode == WIFI_RADIO_SCAN_MODE_OFFCHAN)) {
        if (get_allowed_channels(radioOperation->band, wifi_cap, channels_list, &num_channels,
                    radioOperation->DfsEnabled) != RETURN_OK) {
            wifi_util_error_print(WIFI_MON,
                    "%s:%d get allowed channels failed for the radio : %d\n", __func__, __LINE__,
                    args->radio_index);
            return RETURN_ERR;
        }
    }

    if (args->scan_mode == WIFI_RADIO_SCAN_MODE_ONCHAN) {
        stat_array_size= 1;

        chan_data = (radio_chan_data_t *) calloc(stat_array_size, sizeof(radio_chan_data_t));
        if (chan_data == NULL) {
            wifi_util_error_print(WIFI_MON,"%s:%d NULL chan_data pointer for radio : %d\n", __func__, __LINE__, args->radio_index);
            return RETURN_ERR;
        }
        radio_chan_data = (radio_chan_data_t *)get_wifi_channelStats_t(radio_chan_stats_data, radioOperation->channel);
        if (radio_chan_data == NULL) {
            free(chan_data);
            return RETURN_ERR;
        }
        memcpy(chan_data, radio_chan_data, sizeof(radio_chan_data_t));
        *stats = chan_data;
        *chan_count = stat_array_size;
        return RETURN_OK;
    } else {
        chan_data = (radio_chan_data_t *) calloc(num_channels, sizeof(radio_chan_data_t));
        if (chan_data == NULL) {
            wifi_util_error_print(WIFI_MON,"%s:%d NULL chan_data pointer for radio : %d\n", __func__, __LINE__, args->radio_index);
            return RETURN_ERR;
        }
        stat_array_size = 0;
        if (args->scan_mode == WIFI_RADIO_SCAN_MODE_OFFCHAN) {
            int updated[MAX_CHANNELS];
            memset(updated, 0, sizeof(int)*MAX_CHANNELS);

            for (i=0;i<num_channels;i++) {
                if (radioOperation->band == WIFI_FREQUENCY_5L_BAND
                    || radioOperation->band == WIFI_FREQUENCY_5H_BAND || radioOperation->band == WIFI_FREQUENCY_5_BAND) {
                    if (!radioOperation->DfsEnabled && is_5g_20M_channel_in_dfs(channels_list[i])) {
                        //skip dfs channel since dfs is disabled
                        continue;
                    }
                }
                // Skip the operating channel for offchannel scanning
                if (args->scan_mode == WIFI_RADIO_SCAN_MODE_OFFCHAN) {
                    if (radioOperation->channel == (unsigned int)channels_list[i]) {
                        continue;
                    }
                }
                for (j=0;j<num_channels;j++) {
                    if (channels_list[i] == radio_chan_stats_data->chan_data[j].ch_number) {
                        updated[j] = 1;
                        break;
                    }
                }
            }

            for (j=0;j<radio_chan_stats_data->num_channels;j++) {
                if (updated[j] == 1) {
                    wifi_util_dbg_print(WIFI_MON, "%s:%d  radio index %d,radio channel stats updated for %d \n",__func__,__LINE__, args->radio_index,
                            radio_chan_stats_data->chan_data[j].ch_number);
                    radio_chan_data = (radio_chan_data_t *)get_wifi_channelStats_t(radio_chan_stats_data, radio_chan_stats_data->chan_data[j].ch_number);
                    if (radio_chan_data == NULL) {
                        free(chan_data);
                        return RETURN_ERR;
                    }
                    memcpy(&chan_data[stat_array_size], radio_chan_data, sizeof(radio_chan_data_t));
                    stat_array_size++;
                }
            }

        } else {
            for (i = 0; i < num_channels; i++) {
                if (radioOperation->band == WIFI_FREQUENCY_5L_BAND
                    || radioOperation->band == WIFI_FREQUENCY_5H_BAND || radioOperation->band == WIFI_FREQUENCY_5_BAND) {
                    if (!radioOperation->DfsEnabled && is_5g_20M_channel_in_dfs(channels_list[i])) {
                        //skip dfs channel since dfs is disabled
                        continue;
                    }
                }
                radio_chan_data = (radio_chan_data_t *)get_wifi_channelStats_t(radio_chan_stats_data, channels_list[i]);
                if (radio_chan_data == NULL) {
                    free(chan_data);
                    return RETURN_ERR;
                }
                memcpy(&chan_data[stat_array_size], radio_chan_data, sizeof(radio_chan_data_t));
                stat_array_size++;
            }
        }
        *stats = chan_data;
        *chan_count = stat_array_size;
    }
    return RETURN_OK;
}

void copy_chanstats_to_chandata(radio_chan_data_t *chan_data, wifi_channelStats_t *chan_stats)
{
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);

    ULONG currentTime = tv_now.tv_sec;

    chan_data->ch_in_pool = chan_stats->ch_in_pool;
    chan_data->ch_radar_noise = chan_stats->ch_radar_noise;
    chan_data->ch_number = chan_stats->ch_number;
    chan_data->ch_noise = chan_stats->ch_noise;
    chan_data->ch_max_80211_rssi = chan_stats->ch_max_80211_rssi;
    chan_data->ch_non_80211_noise = chan_stats->ch_non_80211_noise;
    chan_data->ch_utilization = chan_stats->ch_utilization;
    chan_data->ch_utilization_busy_tx = chan_stats->ch_utilization_busy_tx;
    chan_data->ch_utilization_busy_self = chan_stats->ch_utilization_busy_self;
    chan_data->ch_utilization_total = chan_stats->ch_utilization_total;
    chan_data->ch_utilization_busy = chan_stats->ch_utilization_busy;
    chan_data->ch_utilization_busy_rx = chan_stats->ch_utilization_busy_rx;
    chan_data->ch_utilization_busy_ext = chan_stats->ch_utilization_busy_ext;
    chan_data->LastUpdatedTime = currentTime;
    chan_data->LastUpdatedTimeUsec = tv_now.tv_usec;
    return;
}

int stop_radio_channel_neighbor_scheduler_tasks(wifi_mon_collector_element_t *c_elem)
{
    wifi_monitor_t *mon_data = (wifi_monitor_t *)get_wifi_monitor();
    if (c_elem == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d input arguments are NULL args : %p\n",__func__,__LINE__, c_elem);
        return RETURN_ERR;
    }

    scheduler_cancel_timer_task(mon_data->sched, c_elem->u.radio_channel_neighbor_data.scan_complete_task_id);

    return RETURN_OK;
}

int execute_radio_channel_stats_api(wifi_mon_collector_element_t *c_elem, wifi_monitor_t *mon_data)
{
    int ret = RETURN_OK;
    wifi_channelStats_t *chan_stats = NULL;
    unsigned int i, chan_count = 0;
    wifi_radio_capabilities_t *wifi_cap = NULL;
    int   num_channels = 0;
    radio_chan_stats_data_t *radio_chan_stats_data;
    int channels[64] = {0};
    wifi_radio_operationParam_t* radioOperation = NULL;
    wifi_mon_stats_args_t *args = NULL;

    if (c_elem == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d input arguments are NULL args : %p\n",__func__,__LINE__, c_elem);
        return RETURN_ERR;
    }
 
    args = c_elem->args;
    if (args == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d input arguments are NULL args : %p\n",__func__,__LINE__, args);
        return RETURN_ERR;
    }

    if (mon_data->radio_presence[args->radio_index] == false) {
        wifi_util_info_print(WIFI_MON, "%s:%d radio_presence is false for radio : %d\n",__func__,__LINE__, args->radio_index);
        return RETURN_OK;
    }

    radioOperation = getRadioOperationParam(args->radio_index);
    if (radioOperation == NULL) {
        wifi_util_error_print(WIFI_MON,"%s:%d NULL radioOperation pointer for radio : %d\n", __func__, __LINE__, args->radio_index);
        return RETURN_ERR;
    }

    wifi_cap = getRadioCapability(args->radio_index);

    if (get_allowed_channels(radioOperation->band, wifi_cap, channels, &num_channels, radioOperation->DfsEnabled) != RETURN_OK) {
        wifi_util_error_print(WIFI_MON, "%s:%d get allowed channels failed for the radio : %d\n",__func__,__LINE__, args->radio_index);
        return RETURN_ERR;
    }


    chan_stats = (wifi_channelStats_t *) calloc(num_channels, sizeof(wifi_channelStats_t));
    if (chan_stats == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d Failed to alloc memory for the radio : %d\n",__func__,__LINE__, args->radio_index);
        return RETURN_ERR;
    }

    for (chan_count = 0; chan_count < (unsigned int)num_channels; chan_count++) {
        chan_stats[chan_count].ch_number = channels[chan_count];
        chan_stats[chan_count].ch_in_pool= TRUE;
    }

    ret = wifi_getRadioChannelStats(args->radio_index, chan_stats, chan_count);
    if (ret != RETURN_OK) {
        wifi_util_error_print(WIFI_MON, "%s : %d  Failed to get radio channel statistics for scan mode %d radio index %d\n",__func__,__LINE__, args->scan_mode, args->radio_index);
        if (chan_stats != NULL) {
            free(chan_stats);
            chan_stats = NULL;
        }
        return RETURN_ERR;
    }

    pthread_mutex_lock(&mon_data->data_lock);
    wifi_util_dbg_print(WIFI_MON, "%s:%d NL: radio channel stats for radio index: %d chan_count : %d scan_mode %d\n", __func__,
            __LINE__, args->radio_index, chan_count, args->scan_mode);
    for (i = 0; i < chan_count; i++) {
        wifi_util_dbg_print(WIFI_MON, "channel: %d noise: %d ch_radar_noise: %d "
                "ch_max_80211_rssi: %d ch_non_80211_noise:%d ch_utilization: %d "
                "ch_utilization_total: %llu ch_utilization_busy: %llu ch_utilization_busy_tx: %llu "
                "ch_utilization_busy_rx: %llu ch_utilization_busy_self: %llu "
                "ch_utilization_busy_ext: %llu\n",
                chan_stats[i].ch_number, chan_stats[i].ch_noise,
                chan_stats[i].ch_radar_noise, chan_stats[i].ch_max_80211_rssi,
                chan_stats[i].ch_non_80211_noise, chan_stats[i].ch_utilization,
                chan_stats[i].ch_utilization_total, chan_stats[i].ch_utilization_busy,
                chan_stats[i].ch_utilization_busy_tx, chan_stats[i].ch_utilization_busy_rx,
                chan_stats[i].ch_utilization_busy_self, chan_stats[i].ch_utilization_busy_ext);
    }

    radio_chan_stats_data = (radio_chan_stats_data_t *)&mon_data->radio_chan_stats_data[args->radio_index];
    if (radio_chan_stats_data == NULL) {
        wifi_util_error_print(WIFI_MON, "%s : %d radio_chan_stats_data is NULL for %d\n",
                __func__, __LINE__, args->radio_index);
        if (chan_stats != NULL) {
            free(chan_stats);
            chan_stats = NULL;
        }
        pthread_mutex_unlock(&mon_data->data_lock);
        return RETURN_ERR;
    }

    if (radio_chan_stats_data->chan_data == NULL) {
        radio_chan_stats_data->chan_data = (radio_chan_data_t *) calloc(num_channels, sizeof(radio_chan_data_t));
        if (radio_chan_stats_data->chan_data == NULL) {
            wifi_util_error_print(WIFI_MON, "%s:%d Failed to alloc memory for radio : %d\n",__func__,__LINE__, args->radio_index);
            if (chan_stats != NULL) {
                free(chan_stats);
                chan_stats = NULL;
            }
            pthread_mutex_unlock(&mon_data->data_lock);
            return RETURN_ERR;
        }
    } else if (radio_chan_stats_data->num_channels < num_channels) {
        free(radio_chan_stats_data->chan_data);
        radio_chan_stats_data->chan_data = (radio_chan_data_t *) calloc(num_channels, sizeof(radio_chan_data_t));
        if (radio_chan_stats_data->chan_data == NULL) {
            wifi_util_error_print(WIFI_MON, "%s:%d Failed to alloc memory for radio : %d\n",__func__,__LINE__, args->radio_index);
            if (chan_stats != NULL) {
                free(chan_stats);
                chan_stats = NULL;
            }
            pthread_mutex_unlock(&mon_data->data_lock);
            return RETURN_ERR;
        }
    }

    if (radio_chan_stats_data->last_update_time_offchannel == NULL) {
        radio_chan_stats_data->last_update_time_offchannel = (struct timespec *)calloc(MAX_CHANNELS, sizeof(struct timespec));
        if (radio_chan_stats_data->last_update_time_offchannel == NULL) {
            wifi_util_error_print(WIFI_MON, "%s:%d Failed to alloc memory for timespec for radio : %d\n",__func__,__LINE__, args->radio_index);
            if (chan_stats != NULL) {
                free(chan_stats);
                chan_stats = NULL;
            }
            if (radio_chan_stats_data->chan_data  != NULL) {
                free(radio_chan_stats_data->chan_data);
                radio_chan_stats_data->chan_data = NULL;
            }
            pthread_mutex_unlock(&mon_data->data_lock);
            return RETURN_ERR;
        }
    }

    for (chan_count = 0; chan_count < (unsigned int)num_channels; chan_count++) {
        copy_chanstats_to_chandata(&radio_chan_stats_data->chan_data[chan_count], &chan_stats[chan_count]);
    }

    if (args->scan_mode == WIFI_RADIO_SCAN_MODE_OFFCHAN) {
        for (chan_count = 0; chan_count < (unsigned int)num_channels; chan_count++) {
            if (radio_chan_stats_data->chan_data[chan_count].ch_number == mon_data->last_scanned_channel[args->radio_index]) {
                clock_gettime(CLOCK_MONOTONIC, &(radio_chan_stats_data->last_update_time_offchannel[chan_count]));
                break;
            }
        }
    }
    radio_chan_stats_data->num_channels = num_channels;
 
    // Fill the data to wifi_provider_response_t and send
    if (c_elem->stats_clctr.is_event_subscribed == true &&
        (c_elem->stats_clctr.stats_type_subscribed & 1 << mon_stats_type_radio_channel_stats)) {
        //Add the changes to process the channel stats
        void *chan_data = NULL;
        chan_count = 0;
        process_channel_stats(args, radio_chan_stats_data, &chan_data, &chan_count);
        if (chan_count == 0) {
            wifi_util_error_print(WIFI_MON, "%s:%d channel_count is zero\n", __func__, __LINE__);
            if (chan_stats != NULL) {
                free(chan_stats);
                chan_stats = NULL;
            }
            if (chan_data != NULL) {
                free(chan_data);
                chan_data = NULL;
            }
            pthread_mutex_unlock(&mon_data->data_lock);
            return RETURN_ERR;
        }
        // send event to ctrl queue with data
        wifi_provider_response_t *collect_stats;
        collect_stats = (wifi_provider_response_t *) malloc(sizeof(wifi_provider_response_t));
        if (collect_stats == NULL) {
            wifi_util_error_print(WIFI_MON, "%s:%d Failed to allocate memory\n", __func__, __LINE__);
            if (chan_stats != NULL) {
                free(chan_stats);
                chan_stats = NULL;
            }
            if (chan_data != NULL) {
                free(chan_data);
                chan_data = NULL;
            }
            pthread_mutex_unlock(&mon_data->data_lock);
            return RETURN_ERR;
        }
        collect_stats->data_type = mon_stats_type_radio_channel_stats;
        collect_stats->args.radio_index = args->radio_index;
        collect_stats->args.scan_mode = args->scan_mode;
        collect_stats->stat_pointer = chan_data;
        collect_stats->stat_array_size = chan_count;
        pthread_mutex_unlock(&mon_data->data_lock);
        wifi_util_dbg_print(WIFI_MON, "Sending radio stats event to core %d\n", chan_count);
        push_monitor_response_event_to_ctrl_queue(collect_stats, sizeof(wifi_provider_response_t), wifi_event_type_monitor, wifi_event_type_collect_stats, NULL);
        free(collect_stats);
        free(chan_stats);
        return RETURN_OK;
    }

    pthread_mutex_unlock(&mon_data->data_lock);

    if (chan_stats != NULL) {
        free(chan_stats);
        chan_stats = NULL;
    }

    return RETURN_OK;
}

int retrigger_neighbor_scan(void *arg)
{
    wifi_mon_collector_element_t *c_elem = (wifi_mon_collector_element_t *)arg;
    wifi_monitor_t *mon_data = (wifi_monitor_t *)get_wifi_monitor();
    wifi_mon_stats_args_t *args = NULL;
    int ret = RETURN_OK;
    int id = 0;

    args = c_elem->args;
    if (args == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d input arguments are NULL args : %p\n", __func__,
            __LINE__, args);
        return RETURN_ERR;
    }

    if (mon_data->radio_presence[args->radio_index] == false) {
        wifi_util_info_print(WIFI_MON, "%s:%d radio_presence is false for radio : %d\n", __func__,
            __LINE__, args->radio_index);
        return RETURN_OK;
    }

    if (mon_data->scan_trigger_retries[args->radio_index] < NEIGHBOR_SCAN_MAX_RETRY) {
        ret = execute_radio_channel_api(c_elem, mon_data, c_elem->collector_task_interval_ms);
        if (ret != RETURN_OK) {
            mon_data->scan_trigger_retries[args->radio_index]++;
            scheduler_add_timer_task(mon_data->sched, FALSE, &id, retrigger_neighbor_scan, c_elem,
                NEIGHBOR_SCAN_RETRY_INTERVAL, 1, FALSE);
            c_elem->u.radio_channel_neighbor_data.scan_trigger_task_id = id;
            wifi_util_dbg_print(WIFI_MON,
                "%s:%d  Retry (%d) to trigger scan for scan mode %d radio index %d\n", __func__,
                __LINE__, mon_data->scan_trigger_retries[args->radio_index], args->scan_mode,
                args->radio_index);
            return RETURN_OK;
        }
        return ret;
    }

    mon_data->scan_status[args->radio_index] = 0;
    mon_data->scan_trigger_retries[args->radio_index] = 0;
    wifi_util_error_print(WIFI_MON,
        "%s:%d Failed to trigger scan for scan mode %d radio index %d\n", __func__, __LINE__,
        args->scan_mode, args->radio_index);

    return RETURN_ERR;
}

int check_scan_complete_read_results(void *arg)
{
    wifi_neighbor_ap2_t *temp_neigh_stats = NULL;
    int ret = RETURN_OK;
    wifi_neighbor_ap2_t *neigh_stats = NULL;
    wifi_monitor_t *mon_data = (wifi_monitor_t *)get_wifi_monitor();
    wifi_mon_stats_args_t *args = NULL;
    neighscan_diag_cfg_t *neighscan_stats_data = NULL;
    int last_scanned_channel_index = 0;
    unsigned int ap_count = 0;
    int id = 0;
    wifi_mon_collector_element_t *c_elem = (wifi_mon_collector_element_t *)arg;
    args = c_elem->args;
    ret = wifi_getNeighboringWiFiStatus(args->radio_index, &neigh_stats, &ap_count);
    if (ret != RETURN_OK) {
        if (errno == EAGAIN || ret == WIFI_HAL_NOT_READY) {
            int max_retries = 0;
            if (args->scan_mode == WIFI_RADIO_SCAN_MODE_FULL || args->scan_mode == WIFI_RADIO_SCAN_MODE_SELECT_CHANNELS) {
                max_retries = RADIO_SCAN_MAX_RESULTS_RETRIES_FULL_SCAN;
            } else if (args->scan_mode == WIFI_RADIO_SCAN_MODE_ONCHAN ||
                args->scan_mode == WIFI_RADIO_SCAN_MODE_OFFCHAN) {
                max_retries = RADIO_SCAN_MAX_RESULTS_RETRIES_ON_AND_OFF_SCAN;
            }
            if (mon_data->scan_results_retries[args->radio_index] < max_retries) {
                mon_data->scan_results_retries[args->radio_index]++;
                scheduler_add_timer_task(mon_data->sched, FALSE, &id,
                    check_scan_complete_read_results, c_elem, RADIO_SCAN_RESULT_INTERVAL, 1, FALSE);
                c_elem->u.radio_channel_neighbor_data.scan_complete_task_id = id;
                wifi_util_dbg_print(WIFI_MON,
                    "%s : %d  Neighbor wifi status for index %d not ready. Retry (%d)\n", __func__,
                    __LINE__, args->radio_index, mon_data->scan_results_retries[args->radio_index]);
                return RETURN_OK;
            }
            wifi_util_error_print(WIFI_MON,
                "%s : %d  Failed to trigger scan for scan mode %d radio index %d\n", __func__,
                __LINE__, args->scan_mode, args->radio_index);
        } else {
            if (mon_data->scan_trigger_retries[args->radio_index] < NEIGHBOR_SCAN_MAX_RETRY) {
                mon_data->scan_trigger_retries[args->radio_index]++;
                scheduler_add_timer_task(mon_data->sched, FALSE, &id, retrigger_neighbor_scan,
                    c_elem, NEIGHBOR_SCAN_RETRY_INTERVAL, 1, FALSE);
                c_elem->u.radio_channel_neighbor_data.scan_trigger_task_id = id;
                wifi_util_dbg_print(WIFI_MON,
                    "%s:%d  Retry (%d) to trigger scan for scan mode %d radio index %d\n", __func__,
                    __LINE__, mon_data->scan_trigger_retries[args->radio_index], args->scan_mode,
                    args->radio_index);
                return RETURN_OK;
            }
        }
        mon_data->scan_trigger_retries[args->radio_index] = 0;
        mon_data->scan_status[args->radio_index] = 0;
        wifi_util_error_print(WIFI_MON,
            "%s : %d  Failed to get Neighbor wifi status for scan mode %d radio index %d\n",
            __func__, __LINE__, args->scan_mode, args->radio_index);
        return RETURN_ERR;
    }

    wifi_util_dbg_print(WIFI_MON, "%s : %d  Scan complete scan mode %d radio index %d\n", __func__,
        __LINE__, args->scan_mode, args->radio_index);
    mon_data->scan_status[args->radio_index] = 0;
    mon_data->scan_trigger_retries[args->radio_index] = 0;

    //Update Neighbour Cache
    pthread_mutex_lock(&mon_data->data_lock);

    for (unsigned int i = 0; i < ap_count; i++) {
        wifi_util_dbg_print(WIFI_MON,
            "%s:%d Radio: %d Count %d Scan_mode %d ap_SSID %s ap_BSSID %s ap_Mode %s ap_Channel %u "
            "ap_SignalStrength %d ap_SecurityModeEnabled %s ap_EncryptionMode %s "
            "ap_OperatingFrequencyBand %s ap_SupportedStandards %s ap_OperatingStandards %s "
            "ap_OperatingChannelBandwidth %s ap_BeaconPeriod %u ap_Noise %d "
            "ap_BasicDataTransferRates %s ap_SupportedDataTransferRates %s ap_DTIMPeriod %u "
            "ap_ChannelUtilization %u\n",
            __func__, __LINE__, args->radio_index, i, args->scan_mode, neigh_stats[i].ap_SSID,
            neigh_stats[i].ap_BSSID, neigh_stats[i].ap_Mode, neigh_stats[i].ap_Channel,
            neigh_stats[i].ap_SignalStrength, neigh_stats[i].ap_SecurityModeEnabled,
            neigh_stats[i].ap_EncryptionMode, neigh_stats[i].ap_OperatingFrequencyBand,
            neigh_stats[i].ap_SupportedStandards, neigh_stats[i].ap_OperatingStandards,
            neigh_stats[i].ap_OperatingChannelBandwidth, neigh_stats[i].ap_BeaconPeriod,
            neigh_stats[i].ap_Noise, neigh_stats[i].ap_BasicDataTransferRates,
            neigh_stats[i].ap_SupportedDataTransferRates, neigh_stats[i].ap_DTIMPeriod,
            neigh_stats[i].ap_ChannelUtilization);
    }
    wifi_util_dbg_print(WIFI_MON, "%s : %d  radio index %d scan_mode %d, found %d neighbors\n",__func__,__LINE__, args->radio_index, args->scan_mode, ap_count);
    neighscan_stats_data = (neighscan_diag_cfg_t *)&mon_data->neighbor_scan_cfg;
    if (args->scan_mode == WIFI_RADIO_SCAN_MODE_FULL || args->scan_mode == WIFI_RADIO_SCAN_MODE_SELECT_CHANNELS) {
        temp_neigh_stats = neighscan_stats_data->pResult[args->radio_index];
        neighscan_stats_data->pResult[args->radio_index] = neigh_stats;
        neighscan_stats_data->resultCountPerRadio[args->radio_index] = ap_count;
        if (temp_neigh_stats != NULL) {
            free(temp_neigh_stats);
            temp_neigh_stats = NULL;
        }
    } else if (args->scan_mode == WIFI_RADIO_SCAN_MODE_ONCHAN) {
        temp_neigh_stats = neighscan_stats_data->pResult_onchannel[args->radio_index];
        neighscan_stats_data->pResult_onchannel[args->radio_index] = neigh_stats;
        neighscan_stats_data->resultCountPerRadio_onchannel[args->radio_index] = ap_count;
        if (temp_neigh_stats != NULL) {
            free(temp_neigh_stats);
            temp_neigh_stats = NULL;
        }
    } else { //if (args->scan_mode == WIFI_RADIO_SCAN_MODE_OFFCHAN)
        int i;
        wifi_util_dbg_print(WIFI_MON, "%s:%d  last_scanned_channel %d\n",__func__,__LINE__, mon_data->last_scanned_channel[args->radio_index]);
        for (i=0;i<args->channel_list.num_channels;i++) {
            if (mon_data->last_scanned_channel[args->radio_index] == args->channel_list.channels_list[i]) {
                last_scanned_channel_index = i;
                wifi_util_dbg_print(WIFI_MON, "%s:%d  last_scanned_channel_index %d channel : %d\n",__func__,__LINE__, last_scanned_channel_index, mon_data->last_scanned_channel[args->radio_index]);
                break;
            }
        }
        temp_neigh_stats = neighscan_stats_data->pResult_offchannel[args->radio_index][last_scanned_channel_index];
        neighscan_stats_data->pResult_offchannel[args->radio_index][last_scanned_channel_index] = neigh_stats;
        neighscan_stats_data->resultCountPerRadio_offchannel[args->radio_index][last_scanned_channel_index] = ap_count;
        clock_gettime(CLOCK_MONOTONIC, &(neighscan_stats_data->last_update_time_offchannel[args->radio_index][last_scanned_channel_index]));
        neighscan_stats_data->channel[args->radio_index][last_scanned_channel_index] = mon_data->last_scanned_channel[args->radio_index];
        wifi_util_dbg_print(WIFI_MON, "%s:%d  neighscan_stats_data->channel[%d][%d] %d\n",__func__,__LINE__, args->radio_index, last_scanned_channel_index, neighscan_stats_data->channel[args->radio_index][last_scanned_channel_index]);
        wifi_util_dbg_print(WIFI_MON, "%s:%d  mon_data->last_scanned_channel[%d] %d\n",__func__,__LINE__, args->radio_index, mon_data->last_scanned_channel[args->radio_index]);
        wifi_util_dbg_print(WIFI_MON, "%s:%d  neighscan_stats_data->resultCountPerRadio_offchannel[%d][%d] %d\n",__func__,__LINE__, args->radio_index, last_scanned_channel_index, neighscan_stats_data->resultCountPerRadio_offchannel[args->radio_index][last_scanned_channel_index]);
        if (temp_neigh_stats != NULL) {
            free(temp_neigh_stats);
            temp_neigh_stats = NULL;
        }
    }
    // Fill the data to wifi_provider_response_t and send
    if (c_elem->stats_clctr.is_event_subscribed == true &&
        (c_elem->stats_clctr.stats_type_subscribed & 1 << mon_stats_type_neighbor_stats)) {
        //Add the changes to process the channel stats
        void *neighbor_data = NULL;
        ap_count = 0;
        process_neighbor_stats(args, neighscan_stats_data, &neighbor_data, &ap_count);
        if (ap_count == 0) {
            wifi_util_dbg_print(WIFI_MON, "%s:%d ap_count is %d\n", __func__, __LINE__, ap_count);
            if (neighbor_data != NULL) {
                free(neighbor_data);
                neighbor_data= NULL;
            }
            wifi_util_dbg_print(WIFI_MON, "%s:%d neighbor_data is NULL\n", __func__, __LINE__);
        }
        // send event to ctrl queue with data
        wifi_provider_response_t *collect_stats;
        collect_stats = (wifi_provider_response_t *) malloc(sizeof(wifi_provider_response_t));
        if (collect_stats == NULL) {
            wifi_util_error_print(WIFI_MON, "%s:%d Failed to allocate memory\n", __func__, __LINE__);
            if (neighbor_data != NULL) {
                free(neighbor_data);
                neighbor_data = NULL;
            }
            pthread_mutex_unlock(&mon_data->data_lock);
            return RETURN_ERR;
        }
        collect_stats->data_type = mon_stats_type_neighbor_stats;
        collect_stats->args.radio_index = args->radio_index;
        collect_stats->args.scan_mode = args->scan_mode;
        collect_stats->stat_pointer = neighbor_data;
        collect_stats->stat_array_size = ap_count;
        pthread_mutex_unlock(&mon_data->data_lock);
        wifi_util_dbg_print(WIFI_MON, "Sending neighbor stats event to core %d\n", ap_count);
        push_monitor_response_event_to_ctrl_queue(collect_stats, sizeof(wifi_provider_response_t), wifi_event_type_monitor, wifi_event_type_collect_stats, NULL);
        free(neighbor_data);
        free(collect_stats);
    }

    pthread_mutex_unlock(&mon_data->data_lock);

    //Upadte Channel Stats cache
    execute_radio_channel_stats_api(arg, mon_data);

    return RETURN_OK;
}

int execute_radio_channel_api(wifi_mon_collector_element_t *c_elem, wifi_monitor_t *mon_data,
    unsigned long task_interval_ms)
{
    int ret = RETURN_OK;
    wifi_radio_capabilities_t *wifi_cap = NULL;
    int num_channels = 0;
    int channels[64] = { 0 };
    wifi_radio_operationParam_t *radioOperation = NULL;
    int dwell_time;
    char *channel_buff;
    int bytes_written = 0;
    int count = 0;
    int id = 0;
    int on_chan_list[MAX_CHANNELS] = {0};
    int onchan_num_channels = 0;
    int new_num_channels = 0;
    int updated_channels[MAX_CHANNELS] = {0};
    wifi_mon_stats_args_t *args = NULL;

    if (c_elem == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d input arguments are NULL args : %p\n", __func__,
            __LINE__, args);
        return RETURN_ERR;
    }

    args = c_elem->args;
    if (args == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d collector arguments are null : %p\n", __func__,
            __LINE__, args);
        return RETURN_ERR;
    }

    if (mon_data->radio_presence[args->radio_index] == false) {
        wifi_util_info_print(WIFI_MON, "%s:%d radio_presence is false for radio : %d\n", __func__,
            __LINE__, args->radio_index);
        return RETURN_OK;
    }

    radioOperation = getRadioOperationParam(args->radio_index);
    if (radioOperation == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d NULL radioOperation pointer for radio : %d\n",
            __func__, __LINE__, args->radio_index);
        return RETURN_ERR;
    }

    if (args->scan_mode == WIFI_RADIO_SCAN_MODE_ONCHAN) {
        if (get_on_channel_scan_list(radioOperation->band, radioOperation->channelWidth,
                radioOperation->channel, channels, &num_channels) != 0) {
            num_channels = 1;
            channels[0] = radioOperation->channel;
        }
    } else if (args->scan_mode == WIFI_RADIO_SCAN_MODE_FULL) {

        wifi_cap = getRadioCapability(args->radio_index);

        if (get_allowed_channels(radioOperation->band, wifi_cap, channels, &num_channels,
                radioOperation->DfsEnabled) != RETURN_OK) {
            wifi_util_error_print(WIFI_MON,
                "%s:%d get allowed channels failed for the radio : %d\n", __func__, __LINE__,
                args->radio_index);
            return RETURN_ERR;
        }
        // dont run offchan scan if device current using dfs channel
        if (radioOperation->band == WIFI_FREQUENCY_5L_BAND ||
            radioOperation->band == WIFI_FREQUENCY_5H_BAND ||
            radioOperation->band == WIFI_FREQUENCY_5_BAND) {
            if (is_5g_20M_channel_in_dfs(radioOperation->channel) ||
                radioOperation->channelWidth == WIFI_CHANNELBANDWIDTH_160MHZ) {
                wifi_util_dbg_print(WIFI_MON,
                    "%s:%d  full channel scan only executed on current channel duo to DFS channel "
                    "in use for radio index %d\n",
                    __func__, __LINE__, args->radio_index);
                num_channels = 1;
                channels[0] = radioOperation->channel;
            }
        }

    } else if (args->scan_mode == WIFI_RADIO_SCAN_MODE_SELECT_CHANNELS) {
        for (int i = 0; i < args->channel_list.num_channels; i++) {
            channels[i] = args->channel_list.channels_list[i];
            num_channels++;
        }
    } else {
        int i;
        if (args->channel_list.num_channels == 0) {
            return RETURN_ERR;
        }
        // dont run offchan scan if device current using dfs channel
        if (radioOperation->band == WIFI_FREQUENCY_5L_BAND ||
            radioOperation->band == WIFI_FREQUENCY_5H_BAND ||
            radioOperation->band == WIFI_FREQUENCY_5_BAND) {
            if (is_5g_20M_channel_in_dfs(radioOperation->channel) ||
                radioOperation->channelWidth == WIFI_CHANNELBANDWIDTH_160MHZ) {
                wifi_util_dbg_print(WIFI_MON,
                    "%s:%d  off channel scan not executed duo to DFS channel in use for radio "
                    "index %d\n",
                    __func__, __LINE__, args->radio_index);
                return RETURN_OK;
            }
        }
        if (get_on_channel_scan_list(radioOperation->band, radioOperation->channelWidth,
		radioOperation->channel, on_chan_list, &onchan_num_channels) != 0) {
		onchan_num_channels = 1;
		on_chan_list[0] = radioOperation->channel;
	}
        // skip on-channel scan list
        for (int i = 0; i < args->channel_list.num_channels; i++) {
            int unmatched = 1;
            for (int j = 0; j < onchan_num_channels; j++) {
                if ((int)args->channel_list.channels_list[i] == on_chan_list[j]) {
                    unmatched = 0;
                    break;
                }
            }
            if (unmatched) {
                updated_channels[new_num_channels++] = args->channel_list.channels_list[i];
            }
        }
        channels[0] = updated_channels[0];
        for (i = 0; i < new_num_channels; i++) {
            if (mon_data->last_scanned_channel[args->radio_index] ==
                updated_channels[i]) {
                    if ((i + 1) >= new_num_channels) {
                        channels[0] = updated_channels[0];
                    } else {
                        channels[0] = updated_channels[i + 1];
                    }
                    break;
                }
            }
            num_channels = 1;
            mon_data->last_scanned_channel[args->radio_index] = channels[0];
        }

    if (num_channels == 0) {
        wifi_util_error_print(WIFI_MON, "%s:%d invalid number of channels\n", __func__, __LINE__);
        return RETURN_ERR;
    }
    if (args->scan_mode == WIFI_RADIO_SCAN_MODE_FULL || args->scan_mode == WIFI_RADIO_SCAN_MODE_SELECT_CHANNELS ) {
        dwell_time = args->dwell_time;
        if (radioOperation->band == WIFI_FREQUENCY_6_BAND) {
            if (args->dwell_time < 110) {
                dwell_time = 110;
            }
        }
    } else {
        dwell_time = args->dwell_time;
        if (dwell_time == 0) {
            dwell_time = 20;
        }
        if (args->scan_mode == WIFI_RADIO_SCAN_MODE_ONCHAN) {
            // make sure dwell time is less than 20ms if DFS channel
            if (radioOperation->band == WIFI_FREQUENCY_5L_BAND ||
                radioOperation->band == WIFI_FREQUENCY_5H_BAND ||
                radioOperation->band == WIFI_FREQUENCY_5_BAND) {
                if (is_5g_20M_channel_in_dfs(radioOperation->channel) ||
                    radioOperation->channelWidth == WIFI_CHANNELBANDWIDTH_160MHZ) {
                    dwell_time = 20;
                }
            }
        }
    }

    int buffer_size = sizeof(char) * num_channels * 5;
    channel_buff = (char *)malloc(buffer_size);
    if (channel_buff != NULL) {
        for (count = 0; count < num_channels; count++) {
            bytes_written += snprintf(&channel_buff[bytes_written], (buffer_size - bytes_written),
                "%d,", channels[count]);
        }
        channel_buff[bytes_written - 1] = '\0';
    }
    wifi_util_dbg_print(WIFI_MON,
        "%s:%d Start scan. Radio_index : %d scan_mode : %d dwell_time : %d num_channels : %d  "
        "channels : %s\n",
        __func__, __LINE__, args->radio_index, args->scan_mode, dwell_time, num_channels,
        (channel_buff != NULL ? channel_buff : "NULL"));

    if (channel_buff != NULL) {
        free(channel_buff);
    }
    mon_data->scan_status[args->radio_index] = 1;
    mon_data->scan_results_retries[args->radio_index] = 0;
    int private_vap_index = getPrivateApFromRadioIndex(args->radio_index);
    ret = wifi_startNeighborScan(private_vap_index, args->scan_mode, dwell_time, num_channels,
        (unsigned int *)channels);
    if (ret != RETURN_OK) {
        mon_data->scan_trigger_retries[args->radio_index]++;
        scheduler_add_timer_task(mon_data->sched, FALSE, &id, retrigger_neighbor_scan, c_elem,
            NEIGHBOR_SCAN_RETRY_INTERVAL, 1, FALSE);
        c_elem->u.radio_channel_neighbor_data.scan_trigger_task_id = id;
        wifi_util_dbg_print(WIFI_MON,
            "%s:%d  Retry (%d) to trigger scan for scan mode %d radio index %d\n", __func__,
            __LINE__, mon_data->scan_trigger_retries[args->radio_index], args->scan_mode,
            args->radio_index);
        return RETURN_OK;
    }
    scheduler_add_timer_task(mon_data->sched, FALSE, &id, check_scan_complete_read_results, c_elem,
        RADIO_SCAN_RESULT_INTERVAL / 2, 1, FALSE);
    c_elem->u.radio_channel_neighbor_data.scan_complete_task_id = id;

    return RETURN_OK;
}

int copy_radio_channel_stats_from_cache(wifi_mon_provider_element_t *p_elem, void **stats, unsigned int *stat_array_size, wifi_monitor_t *mon_cache)
{
    radio_chan_data_t   *chan_data;
    radio_chan_data_t    *radio_chan_data = NULL;
    radio_chan_stats_data_t *radio_chan_stats_data;
    int chan_count = 0, i, j;
    wifi_mon_stats_args_t *args;

    if ((p_elem == NULL) || (mon_cache == NULL)) {
        wifi_util_error_print(WIFI_MON, "%s:%d Invalid args p_elem : %p mon_cache = %p\n",
                __func__,__LINE__, p_elem, mon_cache);
        return RETURN_ERR;
    }
    if (p_elem->mon_stats_config == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d  p_elem->mon_stats_config NULL\n",
                __func__,__LINE__);
        return RETURN_ERR;
    }
    pthread_mutex_lock(&mon_cache->data_lock);
    args = &(p_elem->mon_stats_config->args);

    radio_chan_stats_data = (radio_chan_stats_data_t *)&mon_cache->radio_chan_stats_data[args->radio_index];
    if (radio_chan_stats_data == NULL) {
        wifi_util_error_print(WIFI_MON, "%s : %d radio_chan_stats_data is NULL\n",
                __func__, __LINE__);
        pthread_mutex_unlock(&mon_cache->data_lock);
        return RETURN_ERR;
    }
    if (radio_chan_stats_data->chan_data == NULL) {
        wifi_util_error_print(WIFI_MON, "%s:%d chan_data in radio_chan_stats_data is NULL for %d\n",
                __func__,__LINE__, args->radio_index);
        pthread_mutex_unlock(&mon_cache->data_lock);
        return RETURN_ERR;
    }

    wifi_radio_operationParam_t* radioOperation = getRadioOperationParam(args->radio_index);
    if (radioOperation == NULL) {
        wifi_util_error_print(WIFI_MON,"%s:%d NULL radioOperation pointer for radio : %d\n", __func__, __LINE__, args->radio_index);
        pthread_mutex_unlock(&mon_cache->data_lock);
        return RETURN_ERR;
    }

    if (args->channel_list.num_channels == 0) {
        chan_count = 1;

        chan_data = (radio_chan_data_t *) calloc(chan_count, sizeof(radio_chan_data_t));
        if (chan_data == NULL) {
            wifi_util_error_print(WIFI_MON,"%s:%d NULL chan_data pointer for radio : %d\n", __func__, __LINE__, args->radio_index);
            pthread_mutex_unlock(&mon_cache->data_lock);
            return RETURN_ERR;
        }
        radio_chan_data = (radio_chan_data_t *)get_wifi_channelStats_t(radio_chan_stats_data, radioOperation->channel);
        if (radio_chan_data == NULL) {
            free(chan_data);
            pthread_mutex_unlock(&mon_cache->data_lock);
            return RETURN_ERR;
        }
        memcpy(chan_data, radio_chan_data, sizeof(radio_chan_data_t));
        *stats = chan_data;
        *stat_array_size = chan_count;
        pthread_mutex_unlock(&mon_cache->data_lock);
        return RETURN_OK;
    } else {
        chan_data = (radio_chan_data_t *) calloc(args->channel_list.num_channels, sizeof(radio_chan_data_t));
        if (chan_data == NULL) {
            wifi_util_error_print(WIFI_MON,"%s:%d NULL chan_data pointer for radio : %d\n", __func__, __LINE__, args->radio_index);
            pthread_mutex_unlock(&mon_cache->data_lock);
            return RETURN_ERR;
        }
        chan_count = 0;
        if (args->scan_mode == WIFI_RADIO_SCAN_MODE_OFFCHAN) {
            int updated[MAX_CHANNELS];
            memset(updated, 0, sizeof(int)*MAX_CHANNELS);

            for (i=0;i<args->channel_list.num_channels;i++) {
                if (radioOperation->band == WIFI_FREQUENCY_5L_BAND
                    || radioOperation->band == WIFI_FREQUENCY_5H_BAND || radioOperation->band == WIFI_FREQUENCY_5_BAND) {
                    if (!radioOperation->DfsEnabled && is_5g_20M_channel_in_dfs(args->channel_list.channels_list[i])) {
                        //skip dfs channel since dfs is disabled
                        continue;
                    }
                }
                for (j=0;j<radio_chan_stats_data->num_channels;j++) {
                    if (args->channel_list.channels_list[i] == radio_chan_stats_data->chan_data[j].ch_number) {
                        if (radio_chan_stats_data->last_update_time_offchannel[j].tv_sec != p_elem->u.radio_channel_data.last_update_time_offchannel[j].tv_sec) {
                            p_elem->u.radio_channel_data.last_update_time_offchannel[j].tv_sec = radio_chan_stats_data->last_update_time_offchannel[j].tv_sec;
                            updated[j] = 1;
                        }
                        break;
                    }
                }
            }

            for (j=0;j<radio_chan_stats_data->num_channels;j++) {
                if (updated[j] == 1) {
                    wifi_util_dbg_print(WIFI_MON, "%s:%d  radio index %d,radio channel stats updated for %d for app : %d \n",__func__,__LINE__, args->radio_index,
                            radio_chan_stats_data->chan_data[j].ch_number, p_elem->mon_stats_config->inst);
                    radio_chan_data = (radio_chan_data_t *)get_wifi_channelStats_t(radio_chan_stats_data, radio_chan_stats_data->chan_data[j].ch_number);
                    if (radio_chan_data == NULL) {
                        free(chan_data);
                        pthread_mutex_unlock(&mon_cache->data_lock);
                        return RETURN_ERR;
                    }
                    memcpy(&chan_data[chan_count], radio_chan_data, sizeof(radio_chan_data_t));
                    chan_count++;
                }
            }

        } else {
            for (i = 0; i < args->channel_list.num_channels; i++) {
                if (radioOperation->band == WIFI_FREQUENCY_5L_BAND
                    || radioOperation->band == WIFI_FREQUENCY_5H_BAND || radioOperation->band == WIFI_FREQUENCY_5_BAND) {
                    if (!radioOperation->DfsEnabled && is_5g_20M_channel_in_dfs(args->channel_list.channels_list[i])) {
                        //skip dfs channel since dfs is disabled
                        continue;
                    }
                }
                radio_chan_data = (radio_chan_data_t *)get_wifi_channelStats_t(radio_chan_stats_data, args->channel_list.channels_list[i]);
                if (radio_chan_data == NULL) {
                    free(chan_data);
                    pthread_mutex_unlock(&mon_cache->data_lock);
                    return RETURN_ERR;
                }
                memcpy(&chan_data[chan_count], radio_chan_data, sizeof(radio_chan_data_t));
                chan_count++;
            }
        }
        *stats = chan_data;
        *stat_array_size = chan_count;
        pthread_mutex_unlock(&mon_cache->data_lock);

        return RETURN_OK;
    }
}

int update_radio_channels_collector_args(void *ce)
{
    wifi_radio_operationParam_t* radioOperation = NULL;
    wifi_radio_capabilities_t *wifi_cap = NULL;
    int   num_channels = 0;
    int channels[64] = {0};
    unsigned int is_used[64] = {0};
    int i, j;
    int new_dwell_time = 0;
    wifi_mon_provider_element_t *provider_elem = NULL;
    wifi_mon_collector_element_t *collector_elem = (wifi_mon_collector_element_t *) ce;

    if (collector_elem == NULL || collector_elem->args == NULL) {
        wifi_util_error_print(WIFI_MON,"%s:%d NULL arguments \n", __func__, __LINE__);
        return RETURN_ERR;
    }

    // Traverse through the providers
    provider_elem = hash_map_get_first(collector_elem->provider_list);
    while (provider_elem != NULL) {
        if (provider_elem->mon_stats_config->args.dwell_time > new_dwell_time) {
            new_dwell_time = provider_elem->mon_stats_config->args.dwell_time;
        }
        provider_elem = hash_map_get_next(collector_elem->provider_list, provider_elem);
    }
    collector_elem->args->dwell_time = new_dwell_time;

    if (collector_elem->args->scan_mode == WIFI_RADIO_SCAN_MODE_OFFCHAN) {
        return RETURN_OK;
    }
    radioOperation = getRadioOperationParam(collector_elem->args->radio_index);
    if (radioOperation == NULL) {
        wifi_util_error_print(WIFI_MON,"%s:%d NULL radioOperation pointer for radio : %d\n", __func__, __LINE__, collector_elem->args->radio_index);
        return RETURN_ERR;
    }

    wifi_cap = getRadioCapability(collector_elem->args->radio_index);

    if (get_allowed_channels(radioOperation->band, wifi_cap, channels, &num_channels, radioOperation->DfsEnabled) != RETURN_OK) {
        wifi_util_error_print(WIFI_MON, "%s:%d get allowed channels failed for the radio : %d\n",__func__,__LINE__, collector_elem->args->radio_index);
        return RETURN_ERR;
    }

    //Traverse through the providers
    provider_elem = hash_map_get_first(collector_elem->provider_list);
    while (provider_elem != NULL) {
        for (i=0; i<provider_elem->mon_stats_config->args.channel_list.num_channels; i++) {
            for (j=0; j<num_channels; j++) {
                if (provider_elem->mon_stats_config->args.channel_list.channels_list[i] == channels[j]) {
                    is_used[j] = 1;
                    break;
                }
            }
        }
        provider_elem = hash_map_get_next(collector_elem->provider_list, provider_elem);
    }

    i = 0;
    for (j=0; j<num_channels; j++) {
        if (is_used[j] == 1) {
            collector_elem->args->channel_list.channels_list[i] =  channels[j];
            i++;
            break;
        }
    }
    collector_elem->args->channel_list.num_channels = i;
    return RETURN_OK;
}

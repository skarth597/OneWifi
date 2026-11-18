/************************************************************************************
  If not stated otherwise in this file or this component's LICENSE file the
  following copyright and licenses apply:

  Copyright 2024 RDK Management

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
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "he_bus_connection.h"
#include "he_bus_common.h"
#include "he_bus_data_conversion.h"
#include "he_bus_memory.h"
#include "he_bus_utils.h"
#include <poll.h>
#include <stdint.h>

#define CONN_VERIFY_NULL_WITH_RC(T)                                                 \
    if (NULL == (T)) {                                                              \
        he_bus_conn_info_print("[%s] input parameter: %s is NULL\n", __func__, #T); \
        return HE_BUS_RETURN_ERR;                                                   \
    }

int save_connection_info(hash_map_t *conn_info_map, he_bus_connection_info_t *conn_info)
{
    CONN_VERIFY_NULL_WITH_RC(conn_info_map);
    CONN_VERIFY_NULL_WITH_RC(conn_info);

    char key[32] = { 0 };

    snprintf(key, sizeof(key), "%d", conn_info->fd);
    return hash_map_put(conn_info_map, strdup(key), conn_info) == 0 ? HE_BUS_RETURN_OK :
        HE_BUS_RETURN_ERR;
}

int bus_server_bind_listener(char const *socket_name, he_bus_listener *pListener)
{
    CONN_VERIFY_NULL_WITH_RC(socket_name);
    CONN_VERIFY_NULL_WITH_RC(pListener);

    int ret;
    he_bus_listener listener;

    listener.fd = -1;
    memset(&listener.local_endpoint, 0, sizeof(struct sockaddr_un));

    listener.local_endpoint.sun_family = AF_UNIX;
    strncpy(listener.local_endpoint.sun_path, socket_name,
        (sizeof(listener.local_endpoint.sun_path) - 1));

    he_bus_conn_info_print("binding listener:%s\r\n", socket_name);

    listener.fd = socket(listener.local_endpoint.sun_family, SOCK_STREAM, 0);
    if (listener.fd == HE_BUS_RETURN_ERR) {
        he_bus_conn_error_print("socket create failure:%d:%s\r\n", errno, strerror(errno));
        return HE_BUS_RETURN_ERR;
    }

    ret = bind(listener.fd, (struct sockaddr *)&listener.local_endpoint,
        sizeof(struct sockaddr_un));
    if (ret == HE_BUS_RETURN_ERR) {
        he_bus_conn_error_print("failed to bind socket:%d:%s\r\n", errno, strerror(errno));
        close(listener.fd);
        return ret;
    }

    if (chmod(socket_name, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) < 0) {
        he_bus_conn_error_print(
            "Failed to change socket's:%s write permission for non-root listener\r\n", socket_name);
        close(listener.fd);
        unlink(socket_name);
        return HE_BUS_RETURN_ERR;
    }

    ret = listen(listener.fd, 32);
    if (ret == HE_BUS_RETURN_ERR) {
        he_bus_conn_error_print("failed to set socket to listen mode:%d:%s\r\n", errno,
            strerror(errno));
        close(listener.fd);
        return ret;
    }

    *pListener = listener;
    return HE_BUS_RETURN_OK;
}

static void bus_connection_info_init(he_bus_connection_info_t *clnt, int fd,
    struct sockaddr_un *remote_endpoint)
{
    clnt->fd = fd;
    memcpy(&clnt->endpoint, remote_endpoint, sizeof(struct sockaddr_un));
}

static bool is_peer_authorized(int fd)
{
    struct ucred credentials;
    socklen_t credentials_length = sizeof(credentials);

    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &credentials, &credentials_length) < 0) {
        he_bus_conn_error_print("getsockopt SO_PEERCRED failed:%d:%s\r\n", errno,
            strerror(errno));
        return false;
    }

    if (credentials_length != sizeof(credentials)) {
        he_bus_conn_error_print("unexpected SO_PEERCRED length:%u\r\n",
            (unsigned int)credentials_length);
        return false;
    }

    if (credentials.uid != geteuid()) {
        he_bus_conn_error_print("rejecting local bus peer uid:%u\r\n",
            (unsigned int)credentials.uid);
        return false;
    }

    return true;
}

static int bus_server_register_new_client(int fd, server_listener_info_t *p_stream_info,
    struct sockaddr_un *remote_endpoint)
{
    he_bus_connection_info_t *new_client;

    new_client = (he_bus_connection_info_t *)he_bus_malloc(sizeof(he_bus_connection_info_t));
    new_client->fd = -1;

    bus_connection_info_init(new_client, fd, remote_endpoint);
    snprintf(new_client->identity, HE_BUS_MSG_ADDR_MAX, "%s/%d", remote_endpoint->sun_path, fd);
    if (save_connection_info(p_stream_info->connected_client_info_map, new_client) !=
        HE_BUS_RETURN_OK) {
        close(fd);
        return HE_BUS_RETURN_ERR;
    }

    he_bus_conn_info_print("new client:%s\r\n", new_client->identity);
    return HE_BUS_RETURN_OK;
}

static void bus_server_accept_client_connection(server_listener_info_t *p_stream_info)
{
    int fd;
    socklen_t socket_length;
    struct sockaddr_un remote_endpoint;
    unsigned int rx_buff_size = MSG_BUFF_SIZE, tx_buff_size = MSG_BUFF_SIZE;
    he_bus_listener *listener = &p_stream_info->listener_info;

    socket_length = sizeof(struct sockaddr_un);
    memset(&remote_endpoint, 0, sizeof(struct sockaddr_un));

    fd = accept(listener->fd, (struct sockaddr *)&remote_endpoint, &socket_length);
    if (fd == -1) {
        he_bus_conn_error_print("accept failure error:%d:%s\r\n", errno, strerror(errno));
        return;
    }

    if (!is_peer_authorized(fd)) {
        close(fd);
        return;
    }

    he_bus_conn_info_print("new client:%d successfully connected with server:%d:%s\r\n", fd,
        listener->fd, remote_endpoint.sun_path);
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &tx_buff_size, sizeof(tx_buff_size)); // Send buffer 4K
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rx_buff_size, sizeof(rx_buff_size)); // Receive buffer 4K

    if (bus_server_register_new_client(fd, p_stream_info,
            &p_stream_info->listener_info.local_endpoint) != HE_BUS_RETURN_OK) {
        he_bus_conn_error_print("failed to register accepted client fd:%d\r\n", fd);
    }
}

static struct pollfd *create_server_pollfds(server_listener_info_t *stream_info,
    nfds_t *pollfd_count)
{
    size_t count = 1;
    size_t index = 1;
    he_bus_connection_info_t *client = NULL;
    struct pollfd *poll_fds;

    if (stream_info->connected_client_info_map != NULL) {
        client = hash_map_get_first(stream_info->connected_client_info_map);
        while (client != NULL) {
            if (client->fd >= 0) {
                if (count == SIZE_MAX) {
                    return NULL;
                }
                count++;
            }
            client = hash_map_get_next(stream_info->connected_client_info_map, client);
        }
    }

    if (count > SIZE_MAX / sizeof(*poll_fds)) {
        return NULL;
    }

    poll_fds = calloc(count, sizeof(*poll_fds));
    if (poll_fds == NULL) {
        return NULL;
    }

    poll_fds[0].fd = stream_info->listener_info.fd;
    poll_fds[0].events = POLLIN;

    if (stream_info->connected_client_info_map != NULL) {
        client = hash_map_get_first(stream_info->connected_client_info_map);
        while (client != NULL) {
            if (client->fd >= 0) {
                poll_fds[index].fd = client->fd;
                poll_fds[index].events = POLLIN;
                index++;
            }
            client = hash_map_get_next(stream_info->connected_client_info_map, client);
        }
    }

    *pollfd_count = (nfds_t)index;
    return poll_fds;
}

int socket_raw_buffer_send(int fd, he_bus_raw_data_msg_t *p_msg_data)
{
    CONN_VERIFY_NULL_WITH_RC(p_msg_data);

    he_bus_stretch_buff_t output_data = { 0 };

    if (convert_bus_raw_msg_data_to_buffer(p_msg_data, &output_data) != he_bus_error_success) {
        he_bus_conn_error_print("%s:%d wrong data for :%s namespace\r\n", __func__, __LINE__,
            p_msg_data->component_name);
        FREE_BUFF_MEMORY(output_data.buff);
        return HE_BUS_RETURN_ERR;
    }

    int ret = send_data_to_endpoint(fd, output_data.buff, output_data.buff_len);
    FREE_BUFF_MEMORY(output_data.buff);
    he_bus_conn_dbg_print("%s:%d socket:%d send success:%d\r\n", __func__, __LINE__, fd, ret);
    return ret;
}

static int recv_bus_scratch_data(he_bus_connection_info_t *client,
    he_bus_stretch_buff_t *p_recv_data)
{
    ssize_t bytes_read;
    ssize_t total_bytes_read = 0;
    uint32_t total_recv_data_len = 0;
    uint8_t read_buffer[MSG_BUFF_SIZE] = { 0 };
    void *p_data;

    bytes_read = recv(client->fd, read_buffer, MSG_BUFF_SIZE, MSG_NOSIGNAL);
    if (bytes_read == -1) {
        he_bus_conn_error_print("unix broadcast server recv failure:%d:%s, client identity:%s\r\n",
            errno, strerror(errno), client->identity);
        return HE_BUS_RETURN_ERR;
    } else if (bytes_read == 0) {
        he_bus_conn_error_print("read zero bytes, broadcast stream closed: client identity:%s\r\n",
            client->identity);
        return HE_BUS_ERROR_STREAM_CLOSED;
    } else {
        total_bytes_read = bytes_read;
        p_data = read_buffer;
        uint32_t bus_msg_id = *(uint32_t *)p_data;
        he_bus_conn_info_print("%s:%d data recv:%ld client identity:%s bus_msg_id:%x:%x\r\n",
            __func__, __LINE__, bytes_read, client->identity, bus_msg_id,
            HE_BUS_MSG_IDENTIFICATION_NUM);
        if (bus_msg_id != HE_BUS_MSG_IDENTIFICATION_NUM) {
            he_bus_conn_error_print("bus msg verification falied for client identity:%s\r\n",
                client->identity);
            return HE_BUS_ERROR_MSG_VARIFICATION;
        } else {
            p_data += sizeof(uint32_t);
            total_recv_data_len = *(uint32_t *)p_data;
            he_bus_conn_info_print("%s:%d total data needs to recv:%ld client identity:%s\r\n",
                __func__, __LINE__, total_recv_data_len, client->identity);
            p_recv_data->buff_len = total_recv_data_len;
            p_recv_data->buff = he_bus_malloc(total_recv_data_len);
            if (bytes_read <= total_recv_data_len) {
                    memcpy(p_recv_data->buff, read_buffer, bytes_read);
                    p_data = p_recv_data->buff;
                    p_data += bytes_read;
            } else {
                    memcpy(p_recv_data->buff, read_buffer, total_recv_data_len);
                    p_data = p_recv_data->buff;
                    p_data += total_recv_data_len;
                    he_bus_conn_info_print("%s:%d recv more data (%d) than needs recv (%d)"
                                ". ignoring %d bytes", __func__, __LINE__, bytes_read,
                                total_recv_data_len, bytes_read - total_recv_data_len);
            }
        }
    }

    while (total_bytes_read < total_recv_data_len) {
        memset(read_buffer, 0, sizeof(read_buffer));
        bytes_read = recv(client->fd, read_buffer, MSG_BUFF_SIZE, MSG_NOSIGNAL);
        if (bytes_read == -1) {
            he_bus_conn_error_print(
                "unix broadcast server recv failure:%d:%s, client identity:%s\r\n", errno,
                strerror(errno), client->identity);
            return HE_BUS_RETURN_ERR;
        } else if (bytes_read == 0) {
            he_bus_conn_error_print(
                "read zero bytes, broadcast stream closed: client identity:%s\r\n",
                client->identity);
            return HE_BUS_ERROR_STREAM_CLOSED;
        } else {
            he_bus_conn_dbg_print("%s:%d rem data recv:%ld\r\n", __func__, __LINE__, bytes_read);

            if ((total_bytes_read + bytes_read) > total_recv_data_len) {
                // Recieved more data than would fit in the buffer/the message told us was there.
                // Ignore the other bytes
                he_bus_conn_info_print("%s:%d recv more data (%d) than needs recv (%d)"
                                    ". ignoring %d bytes", __func__, __LINE__, 
                                    bytes_read, (total_recv_data_len-total_bytes_read), 
                                    total_bytes_read + bytes_read - total_recv_data_len);
                bytes_read = (total_recv_data_len - total_bytes_read);
            }
            total_bytes_read += bytes_read;
            memcpy(p_data, read_buffer, bytes_read);
            p_data += bytes_read;
        }
    }
    return HE_BUS_RETURN_OK;
}

static int read_connected_client_data(he_bus_handle_t handle, he_bus_connection_info_t *client)
{
    (void)handle;
    he_bus_stretch_buff_t bus_recv_data = { 0 };

    int status = recv_bus_scratch_data(client, &bus_recv_data);
    if (status != HE_BUS_RETURN_OK) {
        he_bus_conn_info_print("%s:%d recv status:%d -%d:%s, client identity:%s\r\n", __func__,
            __LINE__, status, errno, strerror(errno), client->identity);
        return status;
    }
    he_bus_raw_data_msg_t recv_data = { 0 };

    convert_buffer_to_bus_raw_msg_data(&recv_data, &bus_recv_data);

    he_bus_conn_dbg_print("unix broadcast server total recv:%d client identity:%s\r\n",
        bus_recv_data.buff_len, client->identity);
    if (recv_data.msg_type == he_bus_inital_msg) {
        memset(client->component_name, 0, sizeof(client->component_name));
        strncpy(client->component_name, recv_data.component_name, recv_data.component_name_len);
        he_bus_conn_info_print("unix client comp: identity:%s on stream id:%d\r\n",
            client->component_name, client->fd);
    } else if (recv_data.msg_type == he_bus_msg_request) {
        he_bus_raw_data_msg_t res_data = { 0 };

        handle_bus_msg_data(handle, client->fd, &recv_data, &res_data);

        socket_raw_buffer_send(client->fd, &res_data);
        free_bus_msg_obj_data(&res_data.data_obj);
    }
    free_bus_msg_obj_data(&recv_data.data_obj);
    FREE_BUFF_MEMORY(bus_recv_data.buff);

    return HE_BUS_RETURN_OK;
}

static int read_connected_client_data_and_send_res(he_bus_handle_t handle,
    he_bus_connection_info_t *client)
{
    he_bus_stretch_buff_t bus_recv_data = { 0 };

    int status = recv_bus_scratch_data(client, &bus_recv_data);
    if (status != HE_BUS_RETURN_OK) {
        he_bus_conn_info_print("%s:%d recv status:%d -%d:%s, client identity:%s\r\n", __func__,
            __LINE__, status, errno, strerror(errno), client->identity);
        return status;
    }

    he_bus_conn_dbg_print("unix unicast server total recv:%d client identity:%s\r\n",
        bus_recv_data.buff_len, client->identity);
    he_bus_raw_data_msg_t res_data = { 0 };

    decode_and_handle_data(handle, client->fd, &bus_recv_data, &res_data);

    socket_raw_buffer_send(client->fd, &res_data);
    free_bus_msg_obj_data(&res_data.data_obj);
    FREE_BUFF_MEMORY(bus_recv_data.buff);

    return HE_BUS_RETURN_OK;
}

static void remove_connected_client(he_bus_handle_t handle, hash_map_t *client_info_map,
    he_bus_connection_info_t *client, bool is_broadcast)
{
    char key[32] = { 0 };
    he_bus_connection_info_t *removed_client;

    if (is_broadcast) {
        remove_client_all_details(handle, client->component_name);
    }

    snprintf(key, sizeof(key), "%d", client->fd);
    removed_client = hash_map_remove(client_info_map, key);
    if (removed_client != NULL) {
        if (removed_client->fd >= 0) {
            close(removed_client->fd);
        }
        he_bus_free(removed_client);
    }
}

static void process_connected_client_poll_events(he_bus_handle_t handle,
    server_listener_info_t *stream_info, const struct pollfd *poll_fds, nfds_t pollfd_count,
    bool is_broadcast)
{
    nfds_t index;

    for (index = 1; index < pollfd_count; index++) {
        short events = poll_fds[index].revents;
        char key[32] = { 0 };
        he_bus_connection_info_t *client;
        bool remove_client = (events & (POLLERR | POLLHUP | POLLNVAL)) != 0;

        if (!(events & (POLLIN | POLLERR | POLLHUP | POLLNVAL))) {
            continue;
        }

        snprintf(key, sizeof(key), "%d", poll_fds[index].fd);
        client = hash_map_get(stream_info->connected_client_info_map, key);
        if (client == NULL) {
            continue;
        }

        if (events & POLLIN) {
            int status = is_broadcast ? read_connected_client_data(handle, client) :
                read_connected_client_data_and_send_res(handle, client);
            remove_client = remove_client || status == HE_BUS_ERROR_STREAM_CLOSED;
        }

        if (remove_client) {
            remove_connected_client(handle, stream_info->connected_client_info_map, client,
                is_broadcast);
        }
    }
}

static int poll_server_connections(he_bus_handle_t handle, server_listener_info_t *stream_info,
    bool is_broadcast)
{
    int ret;
    nfds_t pollfd_count = 0;
    struct pollfd *poll_fds = create_server_pollfds(stream_info, &pollfd_count);

    if (poll_fds == NULL) {
        he_bus_conn_error_print("failed to allocate server poll descriptors\r\n");
        return HE_BUS_RETURN_ERR;
    }

    ret = poll(poll_fds, pollfd_count, 10000);
    if (ret < 0) {
        int poll_error = errno;
        free(poll_fds);
        if (poll_error != EINTR) {
            he_bus_conn_error_print("server poll failure:%d:%s\r\n", poll_error,
                strerror(poll_error));
            return HE_BUS_RETURN_ERR;
        }
        return HE_BUS_RETURN_OK;
    }

    if (ret > 0) {
        if (poll_fds[0].revents & POLLIN) {
            bus_server_accept_client_connection(stream_info);
        }
        if (poll_fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            he_bus_conn_error_print("server listener poll error:%d\r\n", poll_fds[0].revents);
        }
        process_connected_client_poll_events(handle, stream_info, poll_fds, pollfd_count,
            is_broadcast);
    }

    free(poll_fds);
    return HE_BUS_RETURN_OK;
}

static void close_connected_client_fds(hash_map_t *client_info_map)
{
    he_bus_connection_info_t *client;

    if (client_info_map == NULL) {
        return;
    }

    client = hash_map_get_first(client_info_map);
    while (client != NULL) {
        if (client->fd >= 0) {
            close(client->fd);
        }
        client = hash_map_get_next(client_info_map, client);
    }
}

void *ipc_unix_broadcast_server_start(void *arg)
{
    if (arg == NULL) {
        he_bus_conn_error_print("%s:%d invalid input argument\r\n", __func__, __LINE__);
        return NULL;
    }

    he_bus_handle_t handle = (he_bus_handle_t)arg;
    he_bus_conn_info_t *conn_info = get_bus_connection_object(handle);
    server_listener_info_t *p_stream_info = &conn_info->server_info.broadcast;

    unlink(SOCKET_BROADCAST_SERVER_NAME);
    if (bus_server_bind_listener(SOCKET_BROADCAST_SERVER_NAME, &p_stream_info->listener_info) !=
        HE_BUS_RETURN_OK) {
        he_bus_conn_error_print("unix server socket start failure:%s\r\n",
            SOCKET_BROADCAST_SERVER_NAME);
        return NULL;
    }

    p_stream_info->connected_client_info_map = hash_map_create();

    while (conn_info->server_info.is_running) {
        poll_server_connections(handle, p_stream_info, true);
    }
    close(p_stream_info->listener_info.fd);
    p_stream_info->listener_info.fd = -1;
    close_connected_client_fds(p_stream_info->connected_client_info_map);
    hash_map_destroy(p_stream_info->connected_client_info_map);
    p_stream_info->connected_client_info_map = NULL;
    return NULL;
}

void *ipc_unix_unicast_server_start(void *arg)
{
    if (arg == NULL) {
        he_bus_conn_error_print("%s:%d invalid input argument\r\n", __func__, __LINE__);
        return NULL;
    }

    he_bus_handle_t handle = (he_bus_handle_t)arg;
    he_bus_conn_info_t *conn_info = get_bus_connection_object(handle);
    server_listener_info_t *p_stream_info = &conn_info->server_info.unicast;

    unlink(SOCKET_UNICAST_SERVER_NAME);
    if (bus_server_bind_listener(SOCKET_UNICAST_SERVER_NAME, &p_stream_info->listener_info) !=
        HE_BUS_RETURN_OK) {
        he_bus_conn_error_print("unix server socket start failure:%s\r\n",
            SOCKET_UNICAST_SERVER_NAME);
        return NULL;
    }

    p_stream_info->connected_client_info_map = hash_map_create();

    while (conn_info->server_info.is_running) {
        poll_server_connections(handle, p_stream_info, false);
    }
    close(p_stream_info->listener_info.fd);
    p_stream_info->listener_info.fd = -1;
    close_connected_client_fds(p_stream_info->connected_client_info_map);
    hash_map_destroy(p_stream_info->connected_client_info_map);
    p_stream_info->connected_client_info_map = NULL;
    return NULL;
}

int bus_client_bind(char const *socket_name, he_bus_connection_info_t *conn_info)
{
    CONN_VERIFY_NULL_WITH_RC(socket_name);
    CONN_VERIFY_NULL_WITH_RC(conn_info);

    he_bus_connection_info_t l_conn_info;

    l_conn_info.fd = -1;
    memset(&l_conn_info.endpoint, 0, sizeof(struct sockaddr_un));

    l_conn_info.endpoint.sun_family = AF_UNIX;
    strncpy(l_conn_info.endpoint.sun_path, socket_name,
        (sizeof(l_conn_info.endpoint.sun_path) - 1));

    he_bus_conn_info_print("connect socket:%s\r\n", socket_name);

    l_conn_info.fd = socket(l_conn_info.endpoint.sun_family, SOCK_STREAM, 0);
    if (l_conn_info.fd == HE_BUS_RETURN_ERR) {
        he_bus_conn_error_print("socket create failure:%d:%s\r\n", errno, strerror(errno));
        return HE_BUS_RETURN_ERR;
    }

    if (connect(l_conn_info.fd, (struct sockaddr *)&l_conn_info.endpoint,
            sizeof(struct sockaddr_un)) == HE_BUS_RETURN_ERR) {
        he_bus_conn_error_print("%s:%d:connect failed err: %d:%s\n", __func__, __LINE__, errno,
            strerror(errno));
        close(l_conn_info.fd);
        return HE_BUS_RETURN_ERR;
    }

    *conn_info = l_conn_info;
    bus_connection_info_init(conn_info, conn_info->fd, &conn_info->endpoint);
    snprintf(conn_info->identity, HE_BUS_MSG_ADDR_MAX, "%s/%d", conn_info->endpoint.sun_path,
        conn_info->fd);
    he_bus_conn_info_print("client successfully connected with server:%s\r\n", conn_info->identity);
    return HE_BUS_RETURN_OK;
}

static int recv_server_data(he_bus_handle_t handle, he_bus_connection_info_t *conn_info)
{
    he_bus_stretch_buff_t bus_recv_data = { 0 };

    int status = recv_bus_scratch_data(conn_info, &bus_recv_data);
    if (status != HE_BUS_RETURN_OK) {
        he_bus_conn_info_print("%s:%d recv status:%d -%d:%s, client identity:%s\r\n", __func__,
            __LINE__, status, errno, strerror(errno), conn_info->identity);
        return status;
    }

    he_bus_conn_info_print(":%s unix client recv:%ld server identity:%s\r\n", __func__,
        bus_recv_data.buff_len, conn_info->identity);
    he_bus_raw_data_msg_t recv_data = { 0 };

    convert_buffer_to_bus_raw_msg_data(&recv_data, &bus_recv_data);
    if (recv_data.msg_type == he_bus_inital_msg) {
        strncpy(conn_info->component_name, recv_data.component_name, recv_data.component_name_len);
        he_bus_conn_info_print("unix server comp: identity:%s on stream id:%d\r\n",
            conn_info->component_name, conn_info->fd);
    } else {
        handle_bus_msg_data(handle, conn_info->fd, &recv_data, NULL);
    }

    free_bus_msg_obj_data(&recv_data.data_obj);
    FREE_BUFF_MEMORY(bus_recv_data.buff);

    return HE_BUS_RETURN_OK;
}

void *ipc_unix_broadcast_client_start(void *arg)
{
    if (arg == NULL) {
        he_bus_conn_error_print("%s:%d invalid input argument\r\n", __func__, __LINE__);
        return NULL;
    }
    he_bus_handle_t handle = (he_bus_handle_t)arg;
    he_bus_conn_info_t *conn_info = get_bus_connection_object(handle);
    he_bus_client_info_t *p_client_info = &conn_info->client_info;

    if (bus_client_bind(SOCKET_BROADCAST_SERVER_NAME, &p_client_info->conn_info) !=
        HE_BUS_RETURN_OK) {
        he_bus_conn_error_print("unix client socket start failure:%s\r\n",
            SOCKET_BROADCAST_SERVER_NAME);
        return NULL;
    }
    send_bus_initial_msg_info(p_client_info->conn_info.fd, handle->component_name);

    while (p_client_info->is_running) {
        struct pollfd client_poll_fd = {
            .fd = p_client_info->conn_info.fd,
            .events = POLLIN,
        };

        int ret = poll(&client_poll_fd, 1, 10000);
        if (ret == 0) {
            continue;
        } else if (ret == -1) {
            if (errno != EINTR) {
                he_bus_conn_error_print("client poll failure:%d:%s\r\n", errno, strerror(errno));
            }
            continue;
        }

        if (client_poll_fd.revents & (POLLIN | POLLERR | POLLHUP | POLLNVAL)) {
            ret = (client_poll_fd.revents & POLLIN) ?
                recv_server_data(handle, &p_client_info->conn_info) : HE_BUS_ERROR_STREAM_CLOSED;
            if (ret == HE_BUS_ERROR_STREAM_CLOSED ||
                (client_poll_fd.revents & (POLLERR | POLLHUP | POLLNVAL))) {
                if (p_client_info->conn_info.fd >= 0) {
                    close(p_client_info->conn_info.fd);
                }
                p_client_info->conn_info.fd = -1;
                sleep(20); //@TODO TBD Do we need to trigger retry for server connection ?
                if (bus_client_bind(SOCKET_BROADCAST_SERVER_NAME, &p_client_info->conn_info) !=
                    HE_BUS_RETURN_OK) {
                    he_bus_conn_error_print("unix client socket start failure:%s\r\n",
                        SOCKET_BROADCAST_SERVER_NAME);
                }
                continue;
            } else if (ret != HE_BUS_RETURN_OK) {
                continue;
            }
        }
    }
    if (p_client_info->conn_info.fd >= 0) {
        close(p_client_info->conn_info.fd);
    }
    return NULL;
}

int send_data_to_endpoint(int fd, void *data, uint32_t data_len)
{
    CONN_VERIFY_NULL_WITH_RC(data);

    int ret = HE_BUS_RETURN_OK;
    ssize_t bytes_sent;

    // bytes_sent = sendmsg(fd, data, MSG_NOSIGNAL);
    bytes_sent = send(fd, data, data_len, MSG_NOSIGNAL);
    if (bytes_sent == -1) {
        he_bus_conn_error_print("error forwarding message to client. %d %s\r\n", errno,
            strerror(errno));
        ret = HE_BUS_RETURN_ERR;
    }
    he_bus_conn_info_print("Send Message:%d to=%d actual data:%d\r\n", bytes_sent, fd, data_len);
    return ret;
}

int ipc_unix_client_send_data(he_bus_handle_t handle, unsigned char *data, unsigned int len)
{
    CONN_VERIFY_NULL_WITH_RC(data);
    CONN_VERIFY_NULL_WITH_RC(handle);

    he_bus_conn_info_t *conn_info = get_bus_connection_object(handle);
    he_bus_client_info_t *p_client_info = &conn_info->client_info;
    int fd = p_client_info->conn_info.fd;

    return send_data_to_endpoint(fd, data, len);
}

static int single_recv_socket_data(he_bus_connection_info_t *conn_info,
    he_bus_stretch_buff_t *p_recv_data)
{
    CONN_VERIFY_NULL_WITH_RC(conn_info);
    CONN_VERIFY_NULL_WITH_RC(p_recv_data);

    he_bus_stretch_buff_t bus_recv_data = { 0 };

    int status = recv_bus_scratch_data(conn_info, &bus_recv_data);
    if (status != HE_BUS_RETURN_OK) {
        he_bus_conn_info_print("%s:%d recv status:%d -%d:%s, client identity:%s\r\n", __func__,
            __LINE__, status, errno, strerror(errno), conn_info->identity);
        return status;
    }

    he_bus_conn_info_print("[%s] unix client recv:%ld server identity:%s\r\n", __func__,
        bus_recv_data.buff_len, conn_info->identity);
    p_recv_data->buff = bus_recv_data.buff;
    p_recv_data->buff_len = bus_recv_data.buff_len;

    return HE_BUS_RETURN_OK;
}

int ipc_unix_send_data_and_wait_for_res(he_bus_stretch_buff_t *send_data,
    he_bus_stretch_buff_t *p_res_data, uint32_t recv_timeout)
{
    CONN_VERIFY_NULL_WITH_RC(send_data);
    CONN_VERIFY_NULL_WITH_RC(p_res_data);

    he_bus_connection_info_t conn_info;
    he_bus_stretch_buff_t recv_data = { 0 };
    struct timeval timeout;

    if (bus_client_bind(SOCKET_UNICAST_SERVER_NAME, &conn_info) != HE_BUS_RETURN_OK) {
        he_bus_conn_error_print("unix client socket start failure:%s\r\n",
            SOCKET_UNICAST_SERVER_NAME);
        return HE_BUS_RETURN_ERR;
    }

    int ret = send_data_to_endpoint(conn_info.fd, send_data->buff, send_data->buff_len);

    // Set timeout value for receiving data
    timeout.tv_sec = recv_timeout;
    timeout.tv_usec = 0;

    if (setsockopt(conn_info.fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt");
        he_bus_conn_error_print("unix client setsockopt is failured:%s\r\n",
            SOCKET_UNICAST_SERVER_NAME);
        close(conn_info.fd);
        return HE_BUS_RETURN_ERR;
    }

    ret = single_recv_socket_data(&conn_info, &recv_data);

    he_bus_conn_info_print("%s:%d: Received bytes: %d\n", __func__, __LINE__, recv_data.buff_len);
    p_res_data->buff = recv_data.buff;
    p_res_data->buff_len = recv_data.buff_len;

    close(conn_info.fd);
    return ret;
}

int get_client_broadcast_fd(he_bus_handle_t handle, char *comp_name, int *fd)
{
    CONN_VERIFY_NULL_WITH_RC(handle);
    CONN_VERIFY_NULL_WITH_RC(comp_name);
    CONN_VERIFY_NULL_WITH_RC(fd);

    he_bus_conn_info_t *main_conn_info = get_bus_connection_object(handle);
    server_listener_info_t *p_stream_info = &main_conn_info->server_info.broadcast;

    he_bus_connection_info_t *conn_info;

    he_bus_conn_info_print("%s:%d: finding stream id from comp name:%s\n", __func__, __LINE__,
        comp_name);
    conn_info = hash_map_get_first(p_stream_info->connected_client_info_map);
    while (conn_info != NULL) {
        he_bus_conn_dbg_print("%s:%d: comp name:%s\n", __func__, __LINE__,
            conn_info->component_name);
        if (!strncmp(conn_info->component_name, comp_name, (strlen(comp_name) + 1))) {
            *fd = conn_info->fd;
            he_bus_conn_info_print("%s:%d: stream id:%d found for comp:%s\n", __func__, __LINE__,
                *fd, comp_name);
            return HE_BUS_RETURN_OK;
        }
        conn_info = hash_map_get_next(p_stream_info->connected_client_info_map, conn_info);
    }

    return HE_BUS_RETURN_ERR;
}

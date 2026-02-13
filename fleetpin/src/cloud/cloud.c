/*
 * Copyright (c) 2020 Circuit Dojo LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/net/http/client.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cloud);

#include <modem/modem_key_mgmt.h>
#include <cJSON.h>

#include "cloud.h"

/* CA Certificate */
static const char cert[] = {
#include "isrg-root-x1.pem"
};

/* Variables */
const int32_t timeout = 5 * MSEC_PER_SEC;
#define SOCKET_TIMEOUT_SEC  8

#define PORT_STRING "3000"

/* Setup TLS options on a given socket */
int tls_setup(int fd)
{
    int err;
    int verify = TLS_PEER_VERIFY_REQUIRED;
    int session_cache = TLS_SESSION_CACHE_ENABLED;

    /* Security tag that we have provisioned the certificate with */
    const sec_tag_t tls_sec_tag[] = {
        CONFIG_CLOUD_TLS_SEC_TAG,
    };

    /* Cipher suite */
    // Cipher Suite: TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 (0xc02b)
    int cipher_list[] = {0xc02b};

    /* Set options */
    err = setsockopt(fd, SOL_TLS, TLS_PEER_VERIFY, &verify, sizeof(verify));
    if (err)
    {
        err = -errno;
        LOG_ERR("Failed to setup peer verification, err %d", errno);
        return err;
    }

    /* Associate the socket with the security tag
     * we have provisioned the certificate with.
     */
    err = setsockopt(fd, SOL_TLS, TLS_SEC_TAG_LIST, tls_sec_tag, sizeof(tls_sec_tag));
    if (err)
    {
        err = -errno;
        LOG_ERR("Failed to setup TLS sec tag, err %d", errno);
        return err;
    }

    err = setsockopt(fd, SOL_TLS, TLS_CIPHERSUITE_LIST, cipher_list, sizeof(cipher_list));
    if (err)
    {
        err = -errno;
        LOG_ERR("Failed to setup TLS cipher, err %d", errno);
        return err;
    }

    err = setsockopt(fd, SOL_TLS, TLS_HOSTNAME, CONFIG_CLOUD_HOSTNAME, sizeof(CONFIG_CLOUD_HOSTNAME) - 1);
    if (err)
    {
        err = -errno;
        LOG_ERR("Failed to setup TLS hostname, err %d", errno);
        return err;
    }

    err = setsockopt(fd, SOL_TLS, TLS_SESSION_CACHE, &session_cache,
                     sizeof(session_cache));
    if (err)
    {
        err = -errno;
        LOG_ERR("Failed to setup session cache, err %d", errno);
        return err;
    }

    return 0;
}

void dump_addrinfo(const struct addrinfo *ai)
{
	printf("addrinfo @%p: ai_family=%d, ai_socktype=%d, ai_protocol=%d, "
	       "sa_family=%d, sin_port=%x\n",
	       ai, ai->ai_family, ai->ai_socktype, ai->ai_protocol,
	       ai->ai_addr->sa_family,
	       ((struct sockaddr_in *)ai->ai_addr)->sin_port);
}

static int socket_setup(void)
{
    int fd = -1;
    int err = 0;
    struct addrinfo *res = NULL;

    /* Hints */
    struct addrinfo hints =
    {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };

    LOG_INF("Looking up %s", CONFIG_CLOUD_HOSTNAME);
    err = getaddrinfo(CONFIG_CLOUD_HOSTNAME, PORT_STRING, &hints, &res);
    if (err)
    {
        LOG_ERR("getaddrinfo() failed. Err: %i", errno);
        return err;
    }
    else
    {
        LOG_INF("getaddrinfo() successful."); 
        dump_addrinfo(res);
        LOG_INF("Addr Len: %" PRIu32 "", res->ai_addrlen);
    }

    //Have a feeling something here isnt right
    ((struct sockaddr_in *)res->ai_addr)->sin_port = htons(CONFIG_CLOUD_PORT);

    /* Create it */
    fd = socket(res->ai_family, SOCK_STREAM, IPPROTO_TLS_1_2);
    if (fd == -1)
    {
        LOG_ERR("Failed to open socket!");
        err = -ECONNABORTED;
        goto clean_up;
    }
    else
    {
        LOG_INF("Socket created.");
    }

    struct timeval recv_timeout = {.tv_sec = SOCKET_TIMEOUT_SEC};

    err = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));
    if (err != 0)
    {
        err = -errno;
        LOG_ERR("Set receive timeout failed, error: %d, errno: %d", err, errno);
        goto clean_up;
    }
    else
    {
        LOG_INF("Socket options set.");
    }    

    struct timeval send_timeout = {.tv_sec = SOCKET_TIMEOUT_SEC};

    err = setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout));
    if (err)
    {
        err = -errno;
        LOG_ERR("Set transmit timeout failed, error: %d, errno: %d", err, errno);
        goto clean_up;
    }
    else
    {
        LOG_INF("Socket TX successful.");
    } 

    /* Setup TLS socket options */
    /*
    err = tls_setup(fd);
    if (err < 0)
    {
        LOG_ERR("Unable to setup TLS. Err: %i", err);
        goto clean_up;
    }
    else
    {
        LOG_INF("TLS setup complete.");
    } 
    */

    /* Connect */
    //err = connect(fd, res->ai_addr, sizeof(struct sockaddr_in));
    err = connect(fd, res->ai_addr, res->ai_addrlen);
    if (err < 0)
    {
        err = -errno;
        LOG_ERR("Unable to connect. Err: %i - %s", err, strerror(errno));
    }
    else
    {
        LOG_INF("Connected!");
    }

clean_up:
    if (res != NULL)
    {
        LOG_INF("Free addr info");
        freeaddrinfo(res);
        res = NULL;
    }

    if (err < 0 && fd >= 0)
    {
        close(fd);
        fd = -1;
    }

    /* Return error or socket */
    if (err < 0)
        return err;
    else
        return fd;
}

int cloud_get_from_endpoint(const char * url_endpoint, const char * http_headers,
    http_response_cb_t callback_fn, 
    uint8_t * reponse_data_buffer, size_t reponse_data_buffer_len)
{
        int fd = -1;
    int ret = 0;

    LOG_INF("Publish path: %s%s", CONFIG_CLOUD_HOSTNAME, url_endpoint);

    /* Setup socket */
    fd = socket_setup();
    if (fd < 0)
    {
        LOG_ERR("Unable to setup socket. Err: %i", fd);
        return fd;
    }

    LOG_INF("Socket setup complete");

    /* POST */
    struct http_request req;
    memset(&req, 0, sizeof(req));
    req.method = HTTP_GET;
    req.url = url_endpoint;
    req.host = CONFIG_CLOUD_HOSTNAME;
    req.protocol = "HTTP/1.1";
    req.payload = NULL; // No payload
    req.payload_len = 0; // No payload
    req.response = callback_fn;
    req.recv_buf = reponse_data_buffer;
    req.recv_buf_len = reponse_data_buffer_len;
    req.content_type_value = "application/json";
    req.header_fields = (const char **)&http_headers;

    const int32_t get_timeout_ms = 30000; // As per fleetpin implementation, 30 second timeout.
    ret = http_client_req(fd, &req, get_timeout_ms, NULL);
    if (ret < 0)
    {
        LOG_ERR("Unable to send data to %s/%s. Err: %i", CONFIG_CLOUD_HOSTNAME, url_endpoint, ret);
    }
    else
    {
        LOG_INF("Data sent to %s%s successfully", CONFIG_CLOUD_HOSTNAME, url_endpoint);
    }

    if (fd)
    {
        /* Close connection */
        (void)close(fd);
    }
    return ret;
}

/* Provision certificate to modem */
int cert_provision(void)
{
    bool exists;
    int mismatch;

    /* Check to see if it exists first .. */
    int err = modem_key_mgmt_exists(CONFIG_CLOUD_TLS_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, &exists);
    if (err)
    {
        LOG_INF("Failed to check for certificates err %d", err);
        return err;
    }

    /* Compare if it does */
    if (exists)
    {
        mismatch = modem_key_mgmt_cmp(CONFIG_CLOUD_TLS_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, cert,
                                      strlen(cert));
        if (!mismatch)
        {
            LOG_INF("Certificate match");
            return 0;
        }

        LOG_INF("Certificate mismatch");
        err = modem_key_mgmt_delete(CONFIG_CLOUD_TLS_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN);
        if (err)
        {
            LOG_INF("Failed to delete existing certificate, err %d", err);
        }
    }

    LOG_INF("Provisioning certificate");

    /*  Provision certificate to the modem */
    err = modem_key_mgmt_write(CONFIG_CLOUD_TLS_SEC_TAG, MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN, cert,
                               sizeof(cert) - 1);
    if (err)
    {
        LOG_INF("Failed to provision certificate, err %d", err);
        return err;
    }

    return 0;
}

int cloud_init(void)
{
    /* Provision certificates before connecting to the LTE network */
    int err = cert_provision();
    if (err)
    {
       LOG_ERR("Unable to provision certificate! Err: %i", err);
    }

    return 0;
}

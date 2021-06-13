/* mqttexample.c
 *
 * Copyright (C) 2006-2021 wolfSSL Inc.
 *
 * This file is part of wolfMQTT.
 *
 * wolfMQTT is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfMQTT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

/* Include the autoconf generated config.h */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "wolfmqtt/mqtt_client.h"
#include "examples/mqttexample.h"
#include "examples/mqttnet.h"


/* locals */
static const char* kDefTopicName = DEFAULT_TOPIC_NAME;
static const char* kDefClientId =  DEFAULT_CLIENT_ID;

/* argument parsing */
static int myoptind = 0;
static char* myoptarg = NULL;

static MqttTopic topics[1];

#ifdef ENABLE_MQTT_TLS
static const char* mTlsCaFile;
static const char* mTlsCertFile;
static const char* mTlsKeyFile;
#endif

static int mygetopt(int argc, char** argv, const char* optstring)
{
    static char* next = NULL;

    char  c;
    char* cp;

    if (myoptind == 0)
        next = NULL;   /* we're starting new/over */

    if (next == NULL || *next == '\0') {
        if (myoptind == 0)
            myoptind++;

        if (myoptind >= argc || argv[myoptind][0] != '-' ||
                argv[myoptind][1] == '\0') {
            myoptarg = NULL;
            if (myoptind < argc)
                myoptarg = argv[myoptind];

            return -1;
        }

        if (XSTRNCMP(argv[myoptind], "--", 2) == 0) {
            myoptind++;
            myoptarg = NULL;

            if (myoptind < argc)
                myoptarg = argv[myoptind];

            return -1;
        }

        next = argv[myoptind];
        next++;                  /* skip - */
        myoptind++;
    }

    c  = *next++;
    /* The C++ strchr can return a different value */
    cp = (char*)XSTRCHR(optstring, c);

    if (cp == NULL || c == ':')
        return '?';

    cp++;

    if (*cp == ':') {
        if (*next != '\0') {
            myoptarg = next;
            next     = NULL;
        }
        else if (myoptind < argc) {
            myoptarg = argv[myoptind];
            myoptind++;
        }
        else
            return '?';
    }

    return c;
}


/* used for testing only, requires wolfSSL RNG */
#ifdef ENABLE_MQTT_TLS
#include <wolfssl/wolfcrypt/random.h>
#endif

static int mqtt_get_rand(byte* data, word32 len)
{
    int ret = -1;
#ifdef ENABLE_MQTT_TLS
    WC_RNG rng;
    ret = wc_InitRng(&rng);
    if (ret == 0) {
        ret = wc_RNG_GenerateBlock(&rng, data, len);
        wc_FreeRng(&rng);
    }
#elif defined(HAVE_RAND)
    word32 i;
    for (i = 0; i<len; i++) {
        data[i] = (byte)rand();
    }
#endif
    return ret;
}

void mqtt_context_init(MQTTCtx* mqttCtx, void* app_ctx)
{
    XMEMSET(mqttCtx, 0, sizeof(MQTTCtx));
    mqttCtx->app_ctx = app_ctx;
    mqttCtx->username = NULL;
    mqttCtx->password = NULL;
    mqttCtx->client_id = NULL;
    mqttCtx->host = DEFAULT_MQTT_HOST;

    mqttCtx->qos = DEFAULT_MQTT_QOS;
    mqttCtx->tls_cb = mqtt_tls_cb;
    mqttCtx->clean_session = 1;
    mqttCtx->keep_alive_sec = DEFAULT_KEEP_ALIVE_SEC;
    mqttCtx->lwt_msg_topic_name = DEFAULT_LWT_TOPIC_NAME;
    mqttCtx->connect_timeout_ms = DEFAULT_CON_TIMEOUT_MS;
    mqttCtx->cmd_timeout_ms = DEFAULT_CMD_TIMEOUT_MS;
#ifdef WOLFMQTT_V5
    mqttCtx->lwt_will_delay_interval = DEFAULT_LWT_WILL_DELAY_INTERVAL;
    mqttCtx->auth_method = DEFAULT_AUTH_METHOD;
    mqttCtx->max_packet_size = WOLFMQTT_MAX_PACKET_SIZE;
    mqttCtx->topic_alias_max = DEFAULT_TOPIC_ALIAS_MAX;
#endif

    atomic_store_explicit(&mqttCtx->package_id_last, 0, memory_order_seq_cst);
}

int mqtt_publish_msg(MQTTCtx *mqttCtx, const char* topic, MqttQoS qos, word32 timeout_ms, const uint8_t *content, int len)
{
    int rc = MQTT_CODE_SUCCESS;
    MqttPublish publish;
    /* Publish Topic */
    XMEMSET(&publish, 0, sizeof(MqttPublish));
    publish.retain = 0;
    publish.qos = qos;
    publish.duplicate = 0;
    publish.topic_name = topic;
    publish.packet_id = mqtt_get_packetid(&(mqttCtx->package_id_last));
    publish.buffer = (byte*)content;
    publish.total_len = (word32)len;
    publish.timeout_ms = timeout_ms;

    for (;;) {
        if (mqttCtx->stat == WMQ_WAIT_MSG) {
            break;
        }
        if (mqttCtx->sleep_ms_cb) {
            mqttCtx->sleep_ms_cb(mqttCtx->app_ctx, 1);
        }
    }

    do {
        rc = MqttClient_Publish(&mqttCtx->client, &publish);
        if (rc == MQTT_CODE_SUCCESS) {
            return rc;
        }
        if (mqttCtx->sleep_ms_cb) {
            mqttCtx->sleep_ms_cb(mqttCtx->app_ctx, 0);
        }
    } while (rc == MQTT_CODE_CONTINUE);
    return rc;
}

int mqtt_receive_msg(MQTTCtx *mqttCtx, uint8_t *buffer, uint32_t buffer_len)
{
    uint32_t total_len;
    size_t sizeof_total_len = sizeof(total_len);
    size_t readed_len = ringbuf_read(&mqttCtx->on_message_rb, (uint8_t *)&total_len, sizeof_total_len, false);
    if (readed_len != sizeof_total_len) {
        if (readed_len == 0) {
            return 0;
        }
        ringbuf_reset(&mqttCtx->on_message_rb);
        return -1;
    }
    if (total_len < readed_len) {
        ringbuf_reset(&mqttCtx->on_message_rb);
        return -2;
    }
    if (total_len > mqttCtx->on_message_rb.capacity) {
        ringbuf_reset(&mqttCtx->on_message_rb);
        return -3;
    }

#ifdef WOLFMQTT_V5
    if (total_len > mqttCtx->max_packet_size) {
        ringbuf_reset(&mqttCtx->on_message_rb);
        return -4;
    }
#endif

    if (ringbuf_read_available(&mqttCtx->on_message_rb) < total_len) {
        return 0;
    }
    if (buffer_len < total_len) {
        ringbuf_read(&mqttCtx->on_message_rb, NULL, total_len, true);
        return -5;
    } else {
        ringbuf_read(&mqttCtx->on_message_rb, buffer, total_len, true);
        return 1;
    }
    return 0;
}

#ifndef TEST_RAND_SZ
#define TEST_RAND_SZ 4
#endif
static char* mqtt_append_random(const char* inStr, word32 inLen)
{
    int rc;
    const char kHexChar[] = { '0', '1', '2', '3', '4', '5', '6', '7',
                              '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    byte rndBytes[TEST_RAND_SZ], rndHexStr[TEST_RAND_SZ*2];
    char *tmp = NULL;

    rc = mqtt_get_rand(rndBytes, (word32)sizeof(rndBytes));
    if (rc == 0) {
        /* Convert random to hex string */
        int i;
        for (i=0; i<(int)sizeof(rndBytes); i++) {
            byte in = rndBytes[i];
            rndHexStr[(i*2)] =   kHexChar[in >> 4];
            rndHexStr[(i*2)+1] = kHexChar[in & 0xf];
        }
    }
    if (rc == 0) {
        /* Allocate topic name and client id */
        tmp = (char*)WOLFMQTT_MALLOC(inLen + 1 + sizeof(rndHexStr) + 1);
        if (tmp == NULL) {
            rc = MQTT_CODE_ERROR_MEMORY;
        }
    }
    if (rc == 0) {
        /* Format: inStr + `_` randhex + null term */
        XMEMCPY(tmp, inStr, inLen);
        tmp[inLen] = '_';
        XMEMCPY(tmp + inLen + 1, rndHexStr, sizeof(rndHexStr));
        tmp[inLen + 1 + sizeof(rndHexStr)] = '\0';
    }
    return tmp;
}

void mqtt_show_usage(MQTTCtx* mqttCtx)
{
    MQTTCtxExample *mqttExample = mqttCtx->app_ctx;
    PRINTF("%s:", mqttCtx->app_name);
    PRINTF("-?          Help, print this usage");
    PRINTF("-h <host>   Host to connect to, default: %s",
            mqttCtx->host);
#ifdef ENABLE_MQTT_TLS
    PRINTF("-p <num>    Port to connect on, default: Normal %d, TLS %d",
            MQTT_DEFAULT_PORT, MQTT_SECURE_PORT);
    PRINTF("-t          Enable TLS");
    PRINTF("-A <file>   Load CA (validate peer)");
    PRINTF("-K <key>    Use private key (for TLS mutual auth)");
    PRINTF("-c <cert>   Use certificate (for TLS mutual auth)");
#else
    PRINTF("-p <num>    Port to connect on, default: %d",
            MQTT_DEFAULT_PORT);
#endif
    PRINTF("-q <num>    Qos Level 0-2, default: %d",
            mqttCtx->qos);
    PRINTF("-s          Disable clean session connect flag");
    PRINTF("-k <num>    Keep alive seconds, default: %d",
            mqttCtx->keep_alive_sec);
    PRINTF("-i <id>     Client Id, default: %s",
            mqttCtx->client_id);
    PRINTF("-l          Enable LWT (Last Will and Testament)");
    PRINTF("-u <str>    Username");
    PRINTF("-w <str>    Password");
    if (mqttCtx->message) {
        /* Only mqttclient example can set message from CLI */
        PRINTF("-m <str>    Message, default: %s", mqttCtx->message);
    }
    PRINTF("-n <str>    Topic name, default: %s", mqttExample->topic_name);
    PRINTF("-r          Set Retain flag on publish message");
    PRINTF("-C <num>    Command Timeout, default: %dms",
            mqttCtx->cmd_timeout_ms);
#ifdef WOLFMQTT_V5
    PRINTF("-P <num>    Max packet size the client will accept, default: %d",
            WOLFMQTT_MAX_PACKET_SIZE);
#endif
    PRINTF("-T          Test mode");
    if (mqttCtx->pub_file) {
        PRINTF("-f <file>   Use file for publish, default: %s",
                mqttCtx->pub_file);
    }
}

void mqtt_init_ctx(MQTTCtx* mqttCtx, MQTTCtxExample* mqttExample)
{
    XMEMSET(mqttExample, 0, sizeof(MQTTCtxExample));
    mqtt_context_init(mqttCtx, mqttExample);
    mqttCtx->client_id = kDefClientId;
    mqttCtx->topics = topics;
    mqttCtx->topic_count = sizeof(topics) / sizeof(topics[0]);
    mqttExample->topic_name = kDefTopicName;
#ifdef WOLFMQTT_V5
    mqttCtx->enable_eauth = 0;
    mqttExample->topic_alias = 1;
#endif
}

int mqtt_parse_args(MQTTCtx* mqttCtx, int argc, char** argv)
{
    MQTTCtxExample *mqttExample = mqttCtx->app_ctx;
    int rc;

    #ifdef ENABLE_MQTT_TLS
        #define MQTT_TLS_ARGS "c:A:K:"
    #else
        #define MQTT_TLS_ARGS ""
    #endif
    #ifdef WOLFMQTT_V5
        #define MQTT_V5_ARGS "P:"
    #else
        #define MQTT_V5_ARGS ""
    #endif

    while ((rc = mygetopt(argc, argv, "?h:p:q:sk:i:lu:w:m:n:C:Tf:rt" \
            MQTT_TLS_ARGS MQTT_V5_ARGS)) != -1) {
        switch ((char)rc) {
        case '?' :
            mqtt_show_usage(mqttCtx);
            return MY_EX_USAGE;

        case 'h' :
            mqttCtx->host = myoptarg;
            break;

        case 'p' :
            mqttCtx->port = (word16)XATOI(myoptarg);
            if (mqttCtx->port == 0) {
                return err_sys("Invalid Port Number!");
            }
            break;

        case 'q' :
            mqttCtx->qos = (MqttQoS)((byte)XATOI(myoptarg));
            if (mqttCtx->qos > MQTT_QOS_2) {
                return err_sys("Invalid QoS value!");
            }
            break;

        case 's':
            mqttCtx->clean_session = 0;
            break;

        case 'k':
            mqttCtx->keep_alive_sec = XATOI(myoptarg);
            break;

        case 'i':
            mqttCtx->client_id = myoptarg;
            break;

        case 'l':
            mqttCtx->enable_lwt = 1;
            break;

        case 'u':
            mqttCtx->username = myoptarg;
            break;

        case 'w':
            mqttCtx->password = myoptarg;
            break;

        case 'm':
            mqttCtx->message = myoptarg;
            break;

        case 'n':
            mqttExample->topic_name = myoptarg;
            break;

        case 'C':
            mqttCtx->cmd_timeout_ms = XATOI(myoptarg);
            break;

        case 'T':

            mqttCtx->test_mode = 1;
            break;

        case 'f':
            mqttCtx->pub_file = myoptarg;
            break;

        case 'r':
            mqttCtx->retain = 1;
            break;

        case 't':
            mqttCtx->use_tls = 1;
            break;

    #ifdef ENABLE_MQTT_TLS
        case 'A':
            mTlsCaFile = myoptarg;
            break;
        case 'c':
            mTlsCertFile = myoptarg;
            break;
        case 'K':
            mTlsKeyFile = myoptarg;
            break;
    #endif

    #ifdef WOLFMQTT_V5
        case 'P':
            mqttCtx->max_packet_size = XATOI(myoptarg);
            break;
    #endif

        default:
            mqtt_show_usage(mqttCtx);
            return MY_EX_USAGE;
        }
    }

    rc = 0;
    myoptind = 0; /* reset for test cases */

    /* if TLS not enable, check args */
#ifndef ENABLE_MQTT_TLS
    if (mqttCtx->use_tls) {
        PRINTF("Use TLS option not allowed (TLS not compiled in)");
        mqttCtx->use_tls = 0;
        if (mqttCtx->test_mode) {
            return MY_EX_USAGE;
        }
    }
#endif

    /* for test mode only */
    /* add random data to end of client_id and topic_name */
    if (mqttCtx->test_mode && mqttExample->topic_name == kDefTopicName) {
        char* topic_name = mqtt_append_random(kDefTopicName,
                (word32)XSTRLEN(kDefTopicName));
        if (topic_name) {
            mqttExample->topic_name = (const char*)topic_name;
            mqttExample->dynamicTopic = 1;
        }
    }
    if (mqttCtx->test_mode && mqttCtx->client_id == kDefClientId) {
        char* client_id = mqtt_append_random(kDefClientId,
                (word32)XSTRLEN(kDefClientId));
        if (client_id) {
            mqttCtx->client_id = (const char*)client_id;
            mqttCtx->dynamicClientId = 1;
        }
    }

    return rc;
}

void mqtt_free_ctx(MQTTCtx* mqttCtx)
{
    MQTTCtxExample *mqttExample;
    if (mqttCtx == NULL) {
        return;
    }

    mqttExample = mqttCtx->app_ctx;
    if (mqttExample->dynamicTopic && mqttExample->topic_name) {
        WOLFMQTT_FREE((char*)mqttExample->topic_name);
        mqttExample->topic_name = NULL;
    }
    if (mqttCtx->dynamicClientId && mqttCtx->client_id) {
        WOLFMQTT_FREE((char*)mqttCtx->client_id);
        mqttCtx->client_id = NULL;
    }
}

#if defined(__GNUC__) && !defined(NO_EXIT)
    __attribute__ ((noreturn))
#endif
int err_sys(const char* msg)
{
    if (msg) {
        PRINTF("wolfMQTT error: %s", msg);
    }
    exit(EXIT_FAILURE);
}


word16 mqtt_get_packetid(_Atomic word16 *package_id_last)
{
    /* Check rollover */
    for (;;) {
        word16 package_id = atomic_fetch_add_explicit(package_id_last, 1, memory_order_seq_cst);;
        if (package_id != 0) {
            return package_id;
        }
    }
}

#ifdef ENABLE_MQTT_TLS
static int mqtt_tls_verify_cb(int preverify, WOLFSSL_X509_STORE_CTX* store)
{
    char buffer[WOLFSSL_MAX_ERROR_SZ];
    MQTTCtx *mqttCtx = NULL;
    char appName[PRINT_BUFFER_SIZE] = {0};

    if (store->userCtx != NULL) {
        /* The client.ctx was stored during MqttSocket_Connect. */
        mqttCtx = (MQTTCtx *)store->userCtx;
        XSTRNCPY(appName, " for ", PRINT_BUFFER_SIZE-1);
        XSTRNCAT(appName, mqttCtx->app_name,
                PRINT_BUFFER_SIZE-XSTRLEN(appName)-1);
    }

    PRINTF("MQTT TLS Verify Callback%s: PreVerify %d, Error %d (%s)",
            appName, preverify,
            store->error, store->error != 0 ?
                    wolfSSL_ERR_error_string(store->error, buffer) : "none");
    PRINTF("  Subject's domain name is %s", store->domain);

    if (store->error != 0) {
        /* Allowing to continue */
        /* Should check certificate and return 0 if not okay */
        PRINTF("  Allowing cert anyways");
    }

    return 1;
}

/* Use this callback to setup TLS certificates and verify callbacks */
int mqtt_tls_cb(MqttClient* client)
{
    int rc = WOLFSSL_FAILURE;
    MQTTCtx* mqttCtx = (MQTTCtx*)client->ctx;

    client->tls.ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
    if (client->tls.ctx) {
        wolfSSL_CTX_set_verify(client->tls.ctx, WOLFSSL_VERIFY_PEER,
                mqtt_tls_verify_cb);

        /* default to success */
        rc = WOLFSSL_SUCCESS;
    }
    if (rc == WOLFSSL_SUCCESS && mqttCtx->root_ca != NULL) {
        /* Examples for loading buffer directly */
        /* Load CA certificate buffer */
        rc = wolfSSL_CTX_load_verify_buffer(client->tls.ctx,
            (const byte*)mqttCtx->root_ca, (long)XSTRLEN(mqttCtx->root_ca), WOLFSSL_FILETYPE_PEM);

        /* Load Client Cert */
        if (rc == WOLFSSL_SUCCESS)
            rc = wolfSSL_CTX_use_certificate_buffer(client->tls.ctx,
                (const byte*)mqttCtx->device_cert, (long)XSTRLEN(mqttCtx->device_cert),
                WOLFSSL_FILETYPE_PEM);

        /* Load Private Key */
        if (rc == WOLFSSL_SUCCESS)
            rc = wolfSSL_CTX_use_PrivateKey_buffer(client->tls.ctx,
                (const byte*)mqttCtx->device_priv_key, (long)XSTRLEN(mqttCtx->device_priv_key),
                WOLFSSL_FILETYPE_PEM);
    }

    PRINTF("MQTT TLS Setup (%d)", rc);

    return rc;
}
#else
int mqtt_tls_cb(MqttClient* client)
{
    (void)client;
    return 0;
}
#endif /* ENABLE_MQTT_TLS */

/* mqttexample.h
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

#ifndef WOLFMQTT_EXAMPLE_H
#define WOLFMQTT_EXAMPLE_H

#include "wolfmqtt/ringbuf.h"
#include "wolfmqtt/mqtt_client.h"

#ifdef __cplusplus
    extern "C" {
#endif

/* Compatibility Options */
#ifdef NO_EXIT
	#undef exit
	#define exit(rc) return rc
#endif

#ifndef MY_EX_USAGE
#define MY_EX_USAGE 2 /* Exit reason code */
#endif

/* STDIN / FGETS for examples */
#ifndef WOLFMQTT_NO_STDIO
    /* For Linux/Mac */
    #if !defined(FREERTOS) && !defined(USE_WINDOWS_API) && \
        !defined(FREESCALE_MQX) && !defined(FREESCALE_KSDK_MQX) && \
        !defined(MICROCHIP_MPLAB_HARMONY)
        /* Make sure its not explicitly disabled and not already defined */
        #if !defined(WOLFMQTT_NO_STDIN_CAP) && \
            !defined(WOLFMQTT_ENABLE_STDIN_CAP)
            /* Wake on stdin activity */
            #define WOLFMQTT_ENABLE_STDIN_CAP
        #endif
    #endif

    #ifdef WOLFMQTT_ENABLE_STDIN_CAP
        #ifndef XFGETS
            #define XFGETS     fgets
        #endif
        #ifndef STDIN
            #define STDIN 0
        #endif
    #endif
#endif /* !WOLFMQTT_NO_STDIO */


/* Default Configurations */
#define DEFAULT_CMD_TIMEOUT_MS  5000
#define DEFAULT_CON_TIMEOUT_MS  8000
#define DEFAULT_MQTT_QOS        MQTT_QOS_2
#define DEFAULT_KEEP_ALIVE_SEC  30
#define DEFAULT_CLIENT_ID       "WolfMQTTClient"
#define WOLFMQTT_TOPIC_NAME     "wolfMQTT/example/"
#define DEFAULT_TOPIC_NAME      WOLFMQTT_TOPIC_NAME"testTopic"
#define DEFAULT_LWT_TOPIC_NAME  WOLFMQTT_TOPIC_NAME"lwttopic"
#define DEFAULT_AUTH_METHOD    "EXTERNAL"
#define DEFAULT_LWT_WILL_DELAY_INTERVAL 5
#define PRINT_BUFFER_SIZE       80
#define DEFAULT_TOPIC_ALIAS_MAX 16
#define DEFAULT_MESSAGE         "test"

#if !defined(WOLFMQTT_MAX_PACKET_SIZE)
/* The max MQTT control packet size the client is willing to accept. */
#define WOLFMQTT_MAX_PACKET_SIZE      768
#endif

/* Default MQTT host broker to use, when none is specified in the examples */
#ifndef DEFAULT_MQTT_HOST
#define DEFAULT_MQTT_HOST       "test.mosquitto.org" /* broker.hivemq.com */
#endif

/* MQTT Client state */
typedef enum _MQTTCtxState {
    WMQ_BEGIN = 0,
    WMQ_NET_INIT,
    WMQ_INIT,
    WMQ_TCP_CONN,
    WMQ_MQTT_CONN,
    WMQ_SUB,
    WMQ_PUB,
    WMQ_WAIT_MSG,
    WMQ_PING,
    WMQ_UNSUB,
    WMQ_DISCONNECT,
    WMQ_NET_DISCONNECT,
    WMQ_DONE
} MQTTCtxState;

/* MQTT Client context */
/* This is used for the examples as reference */
/* Use of this structure allow non-blocking context */
typedef struct _MQTTCtx {
    MQTTCtxState stat;

    void* app_ctx; /* For storing application specific data */

    /* client and net containers */
    MqttClient client;
    MqttNet net;
    MqttTlsCb tls_cb;

    byte stopped; /* Setting stopped to 1 to stop the non blocking state machine */

    /* temp mqtt containers */
    MqttConnect connect;
    MqttMessage lwt_msg;
    MqttSubscribe subscribe;
    MqttUnsubscribe unsubscribe;
    MqttTopic *topics;
    word32 topic_count;
    MqttPublish publish;
    MqttDisconnect disconnect;
    MqttPing ping;
#ifdef WOLFMQTT_SN
    SN_Publish publishSN;
#endif

    /* configuration */
    MqttQoS qos;
    const char* app_name;
    const char* host;
    const char* username;
    const char* password;
    const char* lwt_msg_topic_name;
    const char* message;
    const char* pub_file;
    const char* client_id;

    /* buffer for receiving client id from server */
    byte* client_id_buf;
    int client_id_buf_size;

    byte *tx_buf;
    int tx_buf_size;
    byte *rx_buf;
    int rx_buf_size;
    _Atomic word16 package_id_last;
    struct ringbuf on_message_rb; // on message ring buffer
    int return_code;
    int use_tls;
    int retain;
    int enable_lwt;
#ifdef ENABLE_MQTT_TLS
    const char* root_ca;
    const char* device_cert;
    const char* device_priv_key;
#endif
#ifdef WOLFMQTT_V5
    word32 max_packet_size;
#endif
    word32 connect_timeout_ms;
    word32 cmd_timeout_ms;
    word16 keep_alive_sec;
    word16 port;
#ifdef WOLFMQTT_V5
    /* Server property, client can set value, but finally will use the minimal value of these two */
    word16  topic_alias_max;
#endif
    byte    clean_session;
    byte    test_mode;
#ifdef WOLFMQTT_V5
    byte    subId_not_avail; /* Server property */
    byte    enable_eauth; /* Enhanced authentication */
    const char* auth_method; /* Auth method for enhanced authentication */
    word32  lwt_will_delay_interval;
#endif
    unsigned int dynamicClientId:1;
#ifdef WOLFMQTT_NONBLOCK
    unsigned int useNonBlockMode:1; /* set to use non-blocking mode.
        network callbacks can return MQTT_CODE_CONTINUE to indicate "would block" */
#endif
} MQTTCtx;

typedef struct MQTTCtxExample {
    const char* topic_name;
#ifdef WOLFMQTT_V5
    word16  topic_alias;
#endif
    unsigned int dynamicTopic:1;
    void* app_ctx; /* For storing application specific data */
} MQTTCtxExample;

WOLFMQTT_LOCAL void mqtt_context_init(MQTTCtx* mqttCtx, void* app_ctx);
WOLFMQTT_LOCAL int mqtt_publish_msg(MQTTCtx *mqttCtx, const char* topic, MqttQoS qos, const uint8_t *content, int len);
WOLFMQTT_LOCAL int mqtt_receive_msg(MQTTCtx *mqttCtx, uint8_t *buffer, uint32_t buffer_len);

void mqtt_show_usage(MQTTCtx* mqttCtx);
void mqtt_init_ctx(MQTTCtx* mqttCtx, MQTTCtxExample* example);
void mqtt_free_ctx(MQTTCtx* mqttCtx);
int mqtt_parse_args(MQTTCtx* mqttCtx, int argc, char** argv);
int err_sys(const char* msg);

int mqtt_tls_cb(MqttClient* client);
word16 mqtt_get_packetid(_Atomic word16 *package_id_last);

#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* WOLFMQTT_EXAMPLE_H */

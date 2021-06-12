/* nbclient.c
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

#ifdef WOLFMQTT_NONBLOCK
#include "nbclient.h"
#include "examples/mqttnet.h"

/* Locals */
static int mStopRead = 0;

/* Configuration */

/* Maximum size for network read/write callbacks. */
#define MAX_BUFFER_SIZE 1024
#define TEST_MESSAGE    "test"

#ifdef WOLFMQTT_DISCONNECT_CB
/* callback indicates a network error occurred */
static int mqtt_disconnect_cb(MqttClient* client, int error_code, void* ctx)
{
    (void)client;
    (void)ctx;
    PRINTF("Network Error Callback: %s (error %d)",
        MqttClient_ReturnCodeToString(error_code), error_code);
    return 0;
}
#endif

static int mqtt_message_cb(MqttClient *client, MqttMessage *msg,
    byte msg_new, byte msg_done)
{
    byte buf[PRINT_BUFFER_SIZE+1];
    word32 len;
    MQTTCtx* mqttCtx = (MQTTCtx*)client->ctx;

    (void)mqttCtx;

    if (msg_new) {
        /* Determine min size to dump */
        len = msg->topic_name_len;
        if (len > PRINT_BUFFER_SIZE) {
            len = PRINT_BUFFER_SIZE;
        }
        XMEMCPY(buf, msg->topic_name, len);
        buf[len] = '\0'; /* Make sure its null terminated */

        /* Print incoming message */
        PRINTF("MQTT Message: Topic %s, Qos %d, Len %u",
            buf, msg->qos, msg->total_len);

        /* for test mode: check if TEST_MESSAGE was received */
        if (mqttCtx->test_mode) {
            if (XSTRLEN(TEST_MESSAGE) == msg->buffer_len &&
                XSTRNCMP(TEST_MESSAGE, (char*)msg->buffer,
                         msg->buffer_len) == 0)
            {
                mStopRead = 1;
            }
        }
    }

    /* Print message payload */
    len = msg->buffer_len;
    if (len > PRINT_BUFFER_SIZE) {
        len = PRINT_BUFFER_SIZE;
    }
    XMEMCPY(buf, msg->buffer, len);
    buf[len] = '\0'; /* Make sure its null terminated */
    PRINTF("Payload (%d - %d): %s",
        msg->buffer_pos, msg->buffer_pos + len, buf);

    if (msg_done) {
        PRINTF("MQTT Message: Done");
    }

    /* Return negative to terminate publish processing */
    return MQTT_CODE_SUCCESS;
}

int mqttclient_test(MQTTCtx *mqttCtx)
{
    MQTTCtxExample *mqttExample = mqttCtx->app_ctx;
    int rc = MQTT_CODE_SUCCESS, i;

    switch (mqttCtx->stat) {
        case WMQ_BEGIN:
        {
            PRINTF("MQTT Client: QoS %d, Use TLS %d", mqttCtx->qos,
                    mqttCtx->use_tls);

            mqttCtx->useNonBlockMode = 1;
        }
        FALL_THROUGH;

        case WMQ_NET_INIT:
        {
            mqttCtx->stat = WMQ_NET_INIT;

            /* Initialize Network */
            rc = MqttClientNet_Init(&mqttCtx->net, mqttCtx);
            if (rc == MQTT_CODE_CONTINUE) {
                return rc;
            }
            PRINTF("MQTT Net Init: %s (%d)",
                MqttClient_ReturnCodeToString(rc), rc);
            if (rc != MQTT_CODE_SUCCESS) {
                goto exit;
            }

            /* setup tx/rx buffers */
            mqttCtx->tx_buf = (byte*)WOLFMQTT_MALLOC(MAX_BUFFER_SIZE);
            mqttCtx->rx_buf = (byte*)WOLFMQTT_MALLOC(MAX_BUFFER_SIZE);
        }
        FALL_THROUGH;

        case WMQ_INIT:
        {
            mqttCtx->stat = WMQ_INIT;

            /* Initialize MqttClient structure */
            rc = MqttClient_Init(&mqttCtx->client, &mqttCtx->net,
                mqtt_message_cb,
                mqttCtx->tx_buf, MAX_BUFFER_SIZE,
                mqttCtx->rx_buf, MAX_BUFFER_SIZE,
                mqttCtx->cmd_timeout_ms);
            if (rc == MQTT_CODE_CONTINUE) {
                return rc;
            }
            PRINTF("MQTT Init: %s (%d)",
                MqttClient_ReturnCodeToString(rc), rc);
            if (rc != MQTT_CODE_SUCCESS) {
                goto exit;
            }
            mqttCtx->client.ctx = mqttCtx;

        #ifdef WOLFMQTT_DISCONNECT_CB
            /* setup disconnect callback */
            rc = MqttClient_SetDisconnectCallback(&mqttCtx->client,
                mqtt_disconnect_cb, NULL);
            if (rc != MQTT_CODE_SUCCESS) {
                goto exit;
            }
        #endif
        }
        FALL_THROUGH;

        case WMQ_TCP_CONN:
        {
            mqttCtx->stat = WMQ_TCP_CONN;

            /* Connect to broker */
            rc = MqttClient_NetConnect(&mqttCtx->client, mqttCtx->host,
                   mqttCtx->port,
                mqttCtx->connect_timeout_ms, mqttCtx->use_tls, mqttCtx->tls_cb);
            if (rc == MQTT_CODE_CONTINUE) {
                return rc;
            }
            PRINTF("MQTT Socket Connect: %s (%d)",
                MqttClient_ReturnCodeToString(rc), rc);
            if (rc != MQTT_CODE_SUCCESS) {
                goto exit;
            }

            /* Build connect packet */
            XMEMSET(&mqttCtx->connect, 0, sizeof(MqttConnect));
            mqttCtx->connect.keep_alive_sec = mqttCtx->keep_alive_sec;
            mqttCtx->connect.timeout_ms = mqttCtx->connect_timeout_ms;
            mqttCtx->connect.clean_session = mqttCtx->clean_session;
            mqttCtx->connect.client_id = mqttCtx->client_id;

            /* Last will and testament sent by broker to subscribers
                of topic when broker connection is lost */
            XMEMSET(&mqttCtx->lwt_msg, 0, sizeof(mqttCtx->lwt_msg));
            mqttCtx->connect.lwt_msg = &mqttCtx->lwt_msg;
            mqttCtx->connect.enable_lwt = mqttCtx->enable_lwt;
            if (mqttCtx->enable_lwt) {
                /* Send client id in LWT payload */
                mqttCtx->lwt_msg.qos = mqttCtx->qos;
                mqttCtx->lwt_msg.retain = 0;
                mqttCtx->lwt_msg.topic_name = mqttCtx->lwt_msg_topic_name;
                mqttCtx->lwt_msg.buffer = (byte*)mqttCtx->client_id;
                mqttCtx->lwt_msg.total_len =
                  (word16)XSTRLEN(mqttCtx->client_id);
            }
            /* Optional authentication */
            mqttCtx->connect.username = mqttCtx->username;
            mqttCtx->connect.password = mqttCtx->password;
        }
        FALL_THROUGH;

        case WMQ_MQTT_CONN:
        {
            mqttCtx->stat = WMQ_MQTT_CONN;

            /* Send Connect and wait for Connect Ack */
            rc = MqttClient_Connect(&mqttCtx->client, &mqttCtx->connect);
            if (rc == MQTT_CODE_CONTINUE) {
                return rc;
            }
            PRINTF("MQTT Connect: Proto (%s), %s (%d)",
                MqttClient_GetProtocolVersionString(&mqttCtx->client),
                MqttClient_ReturnCodeToString(rc), rc);
            if (rc != MQTT_CODE_SUCCESS) {
                goto disconn;
            }

            /* Validate Connect Ack info */
            PRINTF("MQTT Connect Ack: Return Code %u, Session Present %d",
                mqttCtx->connect.ack.return_code,
                (mqttCtx->connect.ack.flags &
                    MQTT_CONNECT_ACK_FLAG_SESSION_PRESENT) ?
                    1 : 0
            );

            /* Build list of topics */
            XMEMSET(&mqttCtx->subscribe, 0, sizeof(MqttSubscribe));

            i = 0;
            mqttCtx->topics[i].topic_filter = mqttExample->topic_name;
            mqttCtx->topics[i].qos = mqttCtx->qos;

            /* Subscribe Topic */
            mqttCtx->subscribe.packet_id = mqtt_get_packetid(&(mqttCtx->package_id_last));
            mqttCtx->subscribe.topic_count = mqttCtx->topic_count;
            mqttCtx->subscribe.topics = mqttCtx->topics;
        }
        FALL_THROUGH;

        case WMQ_SUB:
        {
            mqttCtx->stat = WMQ_SUB;

            rc = MqttClient_Subscribe(&mqttCtx->client, &mqttCtx->subscribe);
            if (rc == MQTT_CODE_CONTINUE) {
                return rc;
            }

            PRINTF("MQTT Subscribe: %s (%d)",
                MqttClient_ReturnCodeToString(rc), rc);
            if (rc != MQTT_CODE_SUCCESS) {
                goto disconn;
            }

            /* show subscribe results */
            for (i = 0; i < mqttCtx->subscribe.topic_count; i++) {
                MqttTopic *topic = &mqttCtx->subscribe.topics[i];
                PRINTF("  Topic %s, Qos %u, Return Code %u",
                    topic->topic_filter,
                    topic->qos, topic->return_code);
            }

            /* Publish Topic */
            XMEMSET(&mqttCtx->publish, 0, sizeof(MqttPublish));
            mqttCtx->publish.retain = 0;
            mqttCtx->publish.qos = mqttCtx->qos;
            mqttCtx->publish.duplicate = 0;
            mqttCtx->publish.topic_name = mqttExample->topic_name;
            mqttCtx->publish.packet_id = mqtt_get_packetid(&(mqttCtx->package_id_last));

            if (mqttCtx->pub_file) {
                rc = mqtt_file_load(mqttCtx->pub_file, &mqttCtx->publish.buffer,
                        (int*)&mqttCtx->publish.total_len);
                if (rc != MQTT_CODE_SUCCESS) {
                    /* There was an error loading the file */
                    PRINTF("MQTT Publish file error: %d", rc);
                }
            }
            else {
                mqttCtx->publish.buffer = (byte*)mqttCtx->message;
                mqttCtx->publish.total_len = (word16)XSTRLEN(mqttCtx->message);
            }
        }
        FALL_THROUGH;

        case WMQ_PUB:
        {
            mqttCtx->stat = WMQ_PUB;

            if ((rc == MQTT_CODE_SUCCESS) || (rc == MQTT_CODE_CONTINUE)) {
                /* This loop allows payloads larger than the buffer to be sent by
                   repeatedly calling publish.
                */
                do {
                    rc = MqttClient_Publish(&mqttCtx->client, &mqttCtx->publish);
                    if (rc == MQTT_CODE_CONTINUE) {
                        return rc;
                    }
                } while(rc == MQTT_CODE_PUB_CONTINUE);

                if ((mqttCtx->pub_file) && (mqttCtx->publish.buffer)) {
                    WOLFMQTT_FREE(mqttCtx->publish.buffer);
                }

                PRINTF("MQTT Publish: Topic %s, %s (%d)",
                    mqttCtx->publish.topic_name,
                    MqttClient_ReturnCodeToString(rc), rc);
                if (rc != MQTT_CODE_SUCCESS) {
                    goto disconn;
                }
            }

            /* Read Loop */
            PRINTF("MQTT Waiting for message...");
        }
        FALL_THROUGH;

        case WMQ_WAIT_MSG:
        {
            mqttCtx->stat = WMQ_WAIT_MSG;

            do {
                /* Try and read packet */
                rc = MqttClient_WaitMessage(&mqttCtx->client,
                    ((word32)mqttCtx->keep_alive_sec) * 1000);

                /* check return code */
                if (rc == MQTT_CODE_CONTINUE) {
                    return rc;
                }

                /* check for test mode */
                if (mStopRead) {
                    rc = MQTT_CODE_SUCCESS;
                    PRINTF("MQTT Exiting...");
                    break;
                }

                if (rc == MQTT_CODE_SUCCESS) {
                    rc = MQTT_CODE_CONTINUE;
                    return rc;
                }

                /* There was an error */
                PRINTF("MQTT Message Wait: %s (%d)",
                    MqttClient_ReturnCodeToString(rc), rc);
                break;
            } while (1);

            /* Check for error */
            if (rc != MQTT_CODE_SUCCESS) {
                goto disconn;
            }

            /* Unsubscribe Topics */
            XMEMSET(&mqttCtx->unsubscribe, 0, sizeof(MqttUnsubscribe));
            mqttCtx->unsubscribe.packet_id = mqtt_get_packetid(&(mqttCtx->package_id_last));
            mqttCtx->unsubscribe.topic_count = mqttCtx->topic_count;
            mqttCtx->unsubscribe.topics = mqttCtx->topics;

            mqttCtx->stat = WMQ_UNSUB;

            return MQTT_CODE_CONTINUE;
        }
        FALL_THROUGH;

        case WMQ_UNSUB:
        {
            /* Unsubscribe Topics */
            rc = MqttClient_Unsubscribe(&mqttCtx->client,
                &mqttCtx->unsubscribe);
            if (rc == MQTT_CODE_CONTINUE) {
                return rc;
            }
            PRINTF("MQTT Unsubscribe: %s (%d)",
                MqttClient_ReturnCodeToString(rc), rc);
            if (rc != MQTT_CODE_SUCCESS) {
                goto disconn;
            }
            mqttCtx->return_code = rc;
        }
        FALL_THROUGH;

        case WMQ_DISCONNECT:
        {
            /* Disconnect */
            rc = MqttClient_Disconnect_ex(&mqttCtx->client,
                   &mqttCtx->disconnect);
            if (rc == MQTT_CODE_CONTINUE) {
                return rc;
            }
            PRINTF("MQTT Disconnect: %s (%d)",
                MqttClient_ReturnCodeToString(rc), rc);
            if (rc != MQTT_CODE_SUCCESS) {
                goto disconn;
            }
        }
        FALL_THROUGH;

        case WMQ_NET_DISCONNECT:
        {
            mqttCtx->stat = WMQ_NET_DISCONNECT;

            rc = MqttClient_NetDisconnect(&mqttCtx->client);
            if (rc == MQTT_CODE_CONTINUE) {
                return rc;
            }
            PRINTF("MQTT Socket Disconnect: %s (%d)",
                MqttClient_ReturnCodeToString(rc), rc);
        }
        FALL_THROUGH;

        case WMQ_DONE:
        {
            mqttCtx->stat = WMQ_DONE;
            rc = mqttCtx->return_code;
            goto exit;
        }

        default:
            rc = MQTT_CODE_ERROR_STAT;
            goto exit;
    } /* switch */

disconn:
    mqttCtx->stat = WMQ_NET_DISCONNECT;
    mqttCtx->return_code = rc;
    rc = MQTT_CODE_CONTINUE;

exit:

    if (rc != MQTT_CODE_CONTINUE) {
        /* Free resources */
        if (mqttCtx->tx_buf) WOLFMQTT_FREE(mqttCtx->tx_buf);
        if (mqttCtx->rx_buf) WOLFMQTT_FREE(mqttCtx->rx_buf);

        /* Cleanup network */
        MqttClientNet_DeInit(&mqttCtx->net);

        MqttClient_DeInit(&mqttCtx->client);
    }

    return rc;
}

#endif /* WOLFMQTT_NONBLOCK */


/* so overall tests can pull in test function */
#if !defined(NO_MAIN_DRIVER) && !defined(MICROCHIP_MPLAB_HARMONY)
    #ifdef USE_WINDOWS_API
        #include <windows.h> /* for ctrl handler */

        static BOOL CtrlHandler(DWORD fdwCtrlType)
        {
            if (fdwCtrlType == CTRL_C_EVENT) {
            #ifdef WOLFMQTT_NONBLOCK
                mStopRead = 1;
            #endif
                PRINTF("Received Ctrl+c");
                return TRUE;
            }
            return FALSE;
        }
    #elif HAVE_SIGNAL
        #include <signal.h>
        static void sig_handler(int signo)
        {
            if (signo == SIGINT) {
            #ifdef WOLFMQTT_NONBLOCK
                mStopRead = 1;
            #endif
                PRINTF("Received SIGINT");
            }
        }
    #endif

    int main(int argc, char** argv)
    {
        int rc;
#ifdef WOLFMQTT_NONBLOCK
        MQTTCtx mqttCtx;
        MQTTCtxExample mqttExample;

        /* init defaults */
        mqtt_init_ctx(&mqttCtx, &mqttExample);
        mqttCtx.app_name = "nbclient";
        mqttCtx.message = DEFAULT_MESSAGE;

        /* parse arguments */
        rc = mqtt_parse_args(&mqttCtx, argc, argv);
        if (rc != 0) {
            if (rc == MY_EX_USAGE) {
                /* return success, so make check passes with TLS disabled */
                return 0;
            }
            return rc;
        }
#endif

    #ifdef USE_WINDOWS_API
        if (SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler,
              TRUE) == FALSE)
        {
            PRINTF("Error setting Ctrl Handler! Error %d", (int)GetLastError());
        }
    #elif HAVE_SIGNAL
        if (signal(SIGINT, sig_handler) == SIG_ERR) {
            PRINTF("Can't catch SIGINT");
        }
    #endif

#ifdef WOLFMQTT_NONBLOCK
        do {
            rc = mqttclient_test(&mqttCtx);
        } while (rc == MQTT_CODE_CONTINUE);

        mqtt_free_ctx(&mqttCtx);
#else
        (void)argc;
        (void)argv;

        /* This example requires non-blocking mode to be enabled
           ./configure --enable-nonblock */
        PRINTF("Example not compiled in!");
        rc = 0; /* return success, so make check passes with TLS disabled */
#endif

        return (rc == 0) ? 0 : EXIT_FAILURE;
    }

#endif /* NO_MAIN_DRIVER */

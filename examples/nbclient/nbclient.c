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

#include "nbclient.h"
#include "examples/mqttnet.h"
#include <errno.h>
#include "main.h"

uint8_t publish_payload[WOLFMQTT_MAX_PACKET_SIZE];

enum MqttPacketResponseCodes mqttclient_nb_state_machine(MQTTCtx *mqttCtx)
{
    enum MqttPacketResponseCodes rc = MQTT_CODE_SUCCESS;
    MqttClient *client = &mqttCtx->client;

    switch (mqttCtx->stat) {
        case WMQ_BEGIN:
        {
            mqttCtx->connect_start_time_ms = 0;
            mqttCtx->connected = 0;
            ringbuf_reset(&mqttCtx->on_message_rb);
            ringbuf_reset(&mqttCtx->send_message_rb);
        }
        FALL_THROUGH;

        case WMQ_TCP_CONN:
        {
            mqttCtx->stat = WMQ_TCP_CONN;

            /* Connect to broker */
            rc = MqttClient_NetConnect(&mqttCtx->client, mqttCtx->host,
                   mqttCtx->port,
                mqttCtx->connect_timeout_ms, mqttCtx->use_tls, mqttCtx->tls_cb);
            /* Track elapsed time with no activity and trigger timeout */
            rc = MqttClient_CheckTimeout(rc, &mqttCtx->connect_start_time_ms,
                mqttCtx->connect_timeout_ms, client->net->get_timer_ms());
            if (rc == MQTT_CODE_CONTINUE) {
                return rc;
            }
            if (rc != MQTT_CODE_ERROR_ROUTE_TO_HOST &&
                rc != MQTT_CODE_ERROR_DNS_RESOLVE) {
                PRINTF("MQTT Socket Connect: %s (%d)(%d)",
                    MqttClient_ReturnCodeToString(rc), rc, errno);
            }
            if (rc != MQTT_CODE_SUCCESS) {
                goto disconn;
            }

            mqttclient_connect_initialize(mqttCtx);
        }
        FALL_THROUGH;

        case WMQ_MQTT_CONN:
        {
            mqttCtx->stat = WMQ_MQTT_CONN;

            /* Send Connect and wait for Connect Ack */
            rc = MqttClient_Connect(&mqttCtx->client, mqttCtx->connect);
            if (rc == MQTT_CODE_CONTINUE) {
                return rc;
            }
            mqttclient_connect_finalize(rc, mqttCtx);
            if (rc != MQTT_CODE_SUCCESS) {
                goto disconn;
            }
            mqttclient_subscribe_initialize(mqttCtx);
        }
        FALL_THROUGH;

        case WMQ_SUB:
        {
            mqttCtx->stat = WMQ_SUB;

            rc = MqttClient_Subscribe(&mqttCtx->client, mqttCtx->subscribe);
            if (rc == MQTT_CODE_CONTINUE) {
                return rc;
            }
            mqttclient_subscribe_finalize(rc, mqttCtx);
            if (rc != MQTT_CODE_SUCCESS) {
                goto disconn;
            }

            XMEMSET(&mqttCtx->client.msg, 0, sizeof(mqttCtx->client.msg));
            mqttCtx->connected = 1;
        }
        FALL_THROUGH;

        case WMQ_PUB:
        {
            char *payload;
            char *topic;
            uint32_t total_len;
            uint16_t topic_len;
            uint32_t payload_len;
            mqttCtx->stat = WMQ_WAIT_MSG;
            int recv_result = mqtt_receive_msg(&mqttCtx->send_message_rb, publish_payload, sizeof(publish_payload));
            if (recv_result > 0) {
                // 4个字节totalLen 2个字节topicLen  topic \0 payload \0
                total_len = *(uint32_t *)publish_payload;
                topic_len = *(uint16_t *)(publish_payload + sizeof(total_len));
                topic = publish_payload + sizeof(total_len) + sizeof(topic_len);
                payload = (char *)(publish_payload + sizeof(total_len) + sizeof(topic_len) + topic_len + 1);
                payload_len = total_len - sizeof(total_len) - sizeof(topic_len) - topic_len - 1 - 1;
                PRINTF("read send topic = %s\n", topic);
                mqtt_publish_msg(mqttCtx, topic, 0, -1, payload, payload_len);
            }
        }
        FALL_THROUGH;

        case WMQ_WAIT_MSG:
        {
            mqttCtx->stat = WMQ_PUB;

            do {
                /* Try and read packet */
                rc = MqttClient_WaitMessage(&mqttCtx->client,
                    ((word32)mqttCtx->keep_alive_sec) * 1000);

                /* check return code */
                if (rc == MQTT_CODE_CONTINUE) {
                    return rc;
                }

                XMEMSET(&mqttCtx->client.msg, 0, sizeof(mqttCtx->client.msg));

                /* check if stopped */
                if (mqttCtx->stopped) {
                    rc = MQTT_CODE_SUCCESS;
                    PRINTF("MQTT Exiting...");
                    break;
                }

                if (rc == MQTT_CODE_SUCCESS) {
                    rc = MQTT_CODE_CONTINUE;
                    return rc;
                }

                /* There was an error */
                PRINTF("MQTT Message Wait: %s (%d)(%d)",
                    MqttClient_ReturnCodeToString(rc), rc, errno);
                break;
            } while (1);

            /* Check for error */
            if (rc != MQTT_CODE_SUCCESS) {
                goto disconn;
            }

            mqttclient_unsubscribe_initialize(mqttCtx);
            mqttCtx->stat = WMQ_UNSUB;

            return MQTT_CODE_CONTINUE;
        }
        FALL_THROUGH;

        case WMQ_UNSUB:
        {
            /* Unsubscribe Topics */
            rc = MqttClient_Unsubscribe(&mqttCtx->client,
                mqttCtx->unsubscribe);
            if (rc == MQTT_CODE_CONTINUE) {
                return rc;
            }
            PRINTF("MQTT Unsubscribe: %s (%d)(%d)",
                MqttClient_ReturnCodeToString(rc), rc, errno);
            if (rc != MQTT_CODE_SUCCESS) {
                goto disconn;
            }
            mqttCtx->return_code = rc;

            XMEMSET(mqttCtx->disconnect, 0, sizeof(mqttCtx->disconnect[0]));
        }
        FALL_THROUGH;

        case WMQ_DISCONNECT:
        {
            /* Disconnect */
            mqttCtx->connected = 0;
            rc = MqttClient_Disconnect_ex(&mqttCtx->client,
                   mqttCtx->disconnect);
            if (rc == MQTT_CODE_CONTINUE) {
                return rc;
            }
#ifdef WOLFMQTT_DEBUG_SOCKET
            PRINTF("MQTT Disconnect: %s (%d)(%d)",
                MqttClient_ReturnCodeToString(rc), rc, errno);
#endif
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
#ifdef WOLFMQTT_DEBUG_SOCKET
            PRINTF("MQTT Socket Disconnect: %s (%d)(%d)",
                MqttClient_ReturnCodeToString(rc), rc,errno);
#endif
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
    return rc;
}

/* so overall tests can pull in test function */
#if 0
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

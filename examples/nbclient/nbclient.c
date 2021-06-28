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

#include "../mqttnet.h"
#include "nbclient.h"
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
    MQTTCtx* mqttCtx = (MQTTCtx*)client->ctx;

    if (msg_new) {
        word16 topic_len = msg->topic_name_len;
        word32 payload_len = msg->buffer_len;
        // total_len = sigma of (word32 topic_len:word16, body_len:word32, topic, \0, body, \0)
        word32 total_len = sizeof(word32) + sizeof(topic_len) + topic_len + 1 + payload_len + 1;
        word32 write_available = ringbuf_write_available(&mqttCtx->on_message_rb);
        if (write_available >= total_len) {
            ringbuf_write(&mqttCtx->on_message_rb, (const uint8_t*)&total_len, sizeof(total_len));
            ringbuf_write(&mqttCtx->on_message_rb, (const uint8_t*)&topic_len, sizeof(topic_len));
            ringbuf_write(&mqttCtx->on_message_rb, (const uint8_t*)msg->topic_name, topic_len);
            ringbuf_write(&mqttCtx->on_message_rb, (const uint8_t*)"\0", 1);
            ringbuf_write(&mqttCtx->on_message_rb, (const uint8_t*)msg->buffer, payload_len);
            ringbuf_write(&mqttCtx->on_message_rb, (const uint8_t*)"\0", 1);
        }
    }

    /* Return negative to terminate publish processing */
    return MQTT_CODE_SUCCESS;
}

#ifdef WOLFMQTT_PROPERTY_CB
/* The property callback is called after decoding a packet that contains at
   least one property. The property list is deallocated after returning from
   the callback. */
static int mqtt_property_cb(MqttClient *client, MqttProp *head, void *ctx)
{
    MqttProp *prop = head;
    int rc = 0;
    MQTTCtx* mqttCtx;

    if ((client == NULL) || (client->ctx == NULL)) {
        return MQTT_CODE_ERROR_BAD_ARG;
    }
    mqttCtx = (MQTTCtx*)client->ctx;

    while (prop != NULL)
    {
        switch (prop->type)
        {
            case MQTT_PROP_ASSIGNED_CLIENT_ID:
                if (mqttCtx->client_id_buf != NULL) {
                    /* Store assigned client ID from CONNACK*/
                    /* really want strlcpy() semantics, but that's non-portable. */
                    memset(mqttCtx->client_id_buf, 0, mqttCtx->client_id_buf_size);
                    XSTRNCPY((char*)mqttCtx->client_id_buf,
                            prop->data_str.str,
                            mqttCtx->client_id_buf_size -1 );
                    /* Store client ID in global */
                    mqttCtx->client_id = (const char*)mqttCtx->client_id_buf;
                }
                break;

            case MQTT_PROP_SUBSCRIPTION_ID_AVAIL:
                mqttCtx->subId_not_avail =
                        prop->data_byte == 0;
                break;

            case MQTT_PROP_TOPIC_ALIAS_MAX:
                mqttCtx->topic_alias_max =
                 (mqttCtx->topic_alias_max < prop->data_short) ?
                 mqttCtx->topic_alias_max : prop->data_short;
                break;

#ifdef WOLFMQTT_V5
            case MQTT_PROP_MAX_PACKET_SZ:
                if ((prop->data_int > 0) &&
                    (prop->data_int <= MQTT_PACKET_SZ_MAX))
                {
                    client->packet_sz_max =
                        (client->packet_sz_max < prop->data_int) ?
                         client->packet_sz_max : prop->data_int;
                }
                else if (prop->data_int != 0) {
                    /* Protocol error */
                    rc = MQTT_CODE_ERROR_PROPERTY;
                }
                break;
#endif

            case MQTT_PROP_SERVER_KEEP_ALIVE:
                mqttCtx->keep_alive_sec = prop->data_short;
                break;

            case MQTT_PROP_MAX_QOS:
                client->max_qos = prop->data_byte;
                break;

            case MQTT_PROP_RETAIN_AVAIL:
                client->retain_avail = prop->data_byte;
                break;

            case MQTT_PROP_REASON_STR:
                PRINTF("Reason String: %s", prop->data_str.str);
                break;

            case MQTT_PROP_PAYLOAD_FORMAT_IND:
            case MQTT_PROP_MSG_EXPIRY_INTERVAL:
            case MQTT_PROP_CONTENT_TYPE:
            case MQTT_PROP_RESP_TOPIC:
            case MQTT_PROP_CORRELATION_DATA:
            case MQTT_PROP_SUBSCRIPTION_ID:
            case MQTT_PROP_SESSION_EXPIRY_INTERVAL:
            case MQTT_PROP_TOPIC_ALIAS:
            case MQTT_PROP_TYPE_MAX:
            case MQTT_PROP_RECEIVE_MAX:
            case MQTT_PROP_USER_PROP:
            case MQTT_PROP_WILDCARD_SUB_AVAIL:
            case MQTT_PROP_SHARED_SUBSCRIPTION_AVAIL:
            case MQTT_PROP_RESP_INFO:
            case MQTT_PROP_SERVER_REF:
            case MQTT_PROP_AUTH_METHOD:
            case MQTT_PROP_AUTH_DATA:
            case MQTT_PROP_NONE:
                break;
            case MQTT_PROP_REQ_PROB_INFO:
            case MQTT_PROP_WILL_DELAY_INTERVAL:
            case MQTT_PROP_REQ_RESP_INFO:
            default:
                /* Invalid */
                rc = MQTT_CODE_ERROR_PROPERTY;
                break;
        }
        prop = prop->next;
    }

    (void)ctx;

    return rc;
}
#endif

enum MqttPacketResponseCodes mqttclient_nb_state_init(MQTTCtx *mqttCtx)
{
    enum MqttPacketResponseCodes rc = MQTT_CODE_SUCCESS;
    PRINTF("MQTT Client: QoS %d, Use TLS %d", mqttCtx->qos,
                    mqttCtx->use_tls);
    mqttCtx->useNonBlockMode = 1;

    /* Initialize Network */
    rc = MqttClientNet_Init(&mqttCtx->net, mqttCtx);
    if (rc == MQTT_CODE_CONTINUE) {
        return rc;
    }
    PRINTF("MQTT Net Init: %s (%d)",
        MqttClient_ReturnCodeToString(rc), rc);
    if (rc != MQTT_CODE_SUCCESS) {
        return rc;
    }
    /* Initialize MqttClient structure */
    rc = MqttClient_Init(&mqttCtx->client, &mqttCtx->net,
        mqtt_message_cb,
        mqttCtx->tx_buf, mqttCtx->tx_buf_size,
        mqttCtx->rx_buf, mqttCtx->rx_buf_size,
        mqttCtx->cmd_timeout_ms);

    if (rc == MQTT_CODE_CONTINUE) {
        return rc;
    }
    PRINTF("MQTT Init: %s (%d)",
        MqttClient_ReturnCodeToString(rc), rc);
    if (rc != MQTT_CODE_SUCCESS) {
        return rc;
    }
    mqttCtx->client.ctx = mqttCtx;

#ifdef WOLFMQTT_DISCONNECT_CB
    /* setup disconnect callback */
    rc = MqttClient_SetDisconnectCallback(&mqttCtx->client,
        mqtt_disconnect_cb, NULL);
    if (rc != MQTT_CODE_SUCCESS) {
        return rc;
    }
#endif
#ifdef WOLFMQTT_PROPERTY_CB
    rc = MqttClient_SetPropertyCallback(&mqttCtx->client,
            mqtt_property_cb, NULL);
    if (rc != MQTT_CODE_SUCCESS) {
        return rc;
    }
#endif
    return rc;
}

void mqttclient_nb_state_cleanup(MQTTCtx *mqttCtx)
{
    /* Cleanup network */
    MqttClientNet_DeInit(&mqttCtx->net);

    MqttClient_DeInit(&mqttCtx->client);
}

enum MqttPacketResponseCodes mqttclient_nb_state_machine(MQTTCtx *mqttCtx)
{
    enum MqttPacketResponseCodes rc = MQTT_CODE_SUCCESS;
    int i = 0;

    switch (mqttCtx->stat) {
        case WMQ_BEGIN:
        {
            FALL_THROUGH;
        }

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
            if (rc != MQTT_CODE_ERROR_ROUTE_TO_HOST) {
                PRINTF("MQTT Socket Connect: %s (%d)",
                    MqttClient_ReturnCodeToString(rc), rc);
            }
            if (rc != MQTT_CODE_SUCCESS) {
                goto disconn;
            }

            /* Reset client properties */
            mqttCtx->client.start_time_ms = 0;
        #ifdef WOLFMQTT_MULTITHREAD
            MqttClient_RespList_Reset(&mqttCtx->client);
        #endif
            XMEMSET(&mqttCtx->client.packet, 0, sizeof(mqttCtx->client.packet));
            XMEMSET(&mqttCtx->client.read, 0, sizeof(mqttCtx->client.read));
            XMEMSET(&mqttCtx->client.write, 0, sizeof(mqttCtx->client.write));
            XMEMSET(&mqttCtx->client.msg, 0, sizeof(mqttCtx->client.msg));
            XMEMSET(&mqttCtx->client.publish_resp, 0, sizeof(mqttCtx->client.publish_resp));
            XMEMSET(&mqttCtx->ping, 0, sizeof(mqttCtx->ping));
            XMEMSET(&mqttCtx->subscribe, 0, sizeof(mqttCtx->subscribe));
            XMEMSET(&mqttCtx->disconnect, 0, sizeof(mqttCtx->disconnect));

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
                mqttCtx->lwt_msg.total_len = (word16)XSTRLEN(mqttCtx->client_id);
            #ifdef WOLFMQTT_V5
                if (mqttCtx->lwt_will_delay_interval > 0)
                {
                    /* Add a delay parameter to sending the LWT */
                    MqttProp* prop = MqttClient_PropsAdd(&mqttCtx->lwt_msg.props);
                    prop->type = MQTT_PROP_WILL_DELAY_INTERVAL;
                    prop->data_int = mqttCtx->lwt_will_delay_interval;
                }
            #endif
            }
            /* Optional authentication */
            mqttCtx->connect.username = mqttCtx->username;
            mqttCtx->connect.password = mqttCtx->password;

        #ifdef WOLFMQTT_V5
            mqttCtx->client.packet_sz_max = mqttCtx->max_packet_size;
            mqttCtx->client.enable_eauth = mqttCtx->enable_eauth;

            if (mqttCtx->client.enable_eauth == 1)
            {
                /* Enhanced authentication */
                /* Add property: Authentication Method */
                MqttProp* prop = MqttClient_PropsAdd(&mqttCtx->connect.props);
                prop->type = MQTT_PROP_AUTH_METHOD;
                prop->data_str.str = (char*)mqttCtx->auth_method;
                prop->data_str.len = (word16)XSTRLEN(prop->data_str.str);
            }

            {
                /* Request Response Information */
                MqttProp* prop = MqttClient_PropsAdd(&mqttCtx->connect.props);
                prop->type = MQTT_PROP_REQ_RESP_INFO;
                prop->data_byte = 1;
            }
            {
                /* Request Problem Information */
                MqttProp* prop = MqttClient_PropsAdd(&mqttCtx->connect.props);
                prop->type = MQTT_PROP_REQ_PROB_INFO;
                prop->data_byte = 1;
            }
            {
                /* Maximum Packet Size */
                MqttProp* prop = MqttClient_PropsAdd(&mqttCtx->connect.props);
                prop->type = MQTT_PROP_MAX_PACKET_SZ;
                prop->data_int = mqttCtx->max_packet_size;
            }
            {
                /* Topic Alias Maximum */
                MqttProp* prop = MqttClient_PropsAdd(&mqttCtx->connect.props);
                prop->type = MQTT_PROP_TOPIC_ALIAS_MAX;
                prop->data_short = mqttCtx->topic_alias_max;
            }
        #endif

            FALL_THROUGH;
        }

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

        #ifdef WOLFMQTT_V5
            /* Release the allocated properties */
            MqttClient_PropsFree(&mqttCtx->connect.props);
            /* Release the allocated properties */
            MqttClient_PropsFree(&mqttCtx->lwt_msg.props);
        #endif

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

            /* Subscribe Topic */
            mqttCtx->subscribe.packet_id = mqtt_get_packetid(&(mqttCtx->package_id_last));
            mqttCtx->subscribe.topic_count = mqttCtx->topic_count;
            mqttCtx->subscribe.topics = mqttCtx->topics;

        #ifdef WOLFMQTT_V5
            if (mqttCtx->subId_not_avail != 1) {
                /* Subscription Identifier */
                for (uint32_t i = 0; i < mqttCtx->topic_count; ++i) {
                    if (mqttCtx->topics[i].sub_id > 0) {
                        MqttProp* prop = MqttClient_PropsAdd(&mqttCtx->subscribe.props);
                        prop->type = MQTT_PROP_SUBSCRIPTION_ID;
                        prop->data_int = mqttCtx->topics[i].sub_id;
                    }
                }
            }
        #endif

            FALL_THROUGH;
        }

        case WMQ_SUB:
        {
            mqttCtx->stat = WMQ_SUB;

            rc = MqttClient_Subscribe(&mqttCtx->client, &mqttCtx->subscribe);
            if (rc == MQTT_CODE_CONTINUE) {
                return rc;
            }

        #ifdef WOLFMQTT_V5
            MqttClient_PropsFree(&mqttCtx->subscribe.props);
        #endif

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

            FALL_THROUGH;
        }

        case WMQ_WAIT_MSG:
        {
            mqttCtx->stat = WMQ_WAIT_MSG;

            do {
                /* Try and read packet */
                rc = MqttClient_WaitMessage(&mqttCtx->client,
                    ((word32)mqttCtx->keep_alive_sec * 0.8) * 1000);

                /* check return code */
                if (rc == MQTT_CODE_CONTINUE) {
                    return rc;
                }

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

                if (rc == MQTT_CODE_ERROR_TIMEOUT) {
                    /* Need to send keep-alive ping */
                    rc = MQTT_CODE_CONTINUE;
                    PRINTF("Keep-alive timeout at %d, sending ping",
                        mqttCtx->client.net->get_timer_ms());
                    mqttCtx->stat = WMQ_PING;
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

        case WMQ_PING:
        {
            rc = MqttClient_Ping_ex(&mqttCtx->client, &mqttCtx->ping);
            if (rc == MQTT_CODE_CONTINUE) {
                return rc;
            }
            else if (rc != MQTT_CODE_SUCCESS) {
                PRINTF("MQTT Ping Keep Alive Error: %s (%d) at %d",
                    MqttClient_ReturnCodeToString(rc), rc,
                    mqttCtx->client.net->get_timer_ms());
                break;
            }

            /* Go back to waiting for message */
            mqttCtx->stat = WMQ_WAIT_MSG;
            rc = MQTT_CODE_CONTINUE;
            return rc;
        }

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

            FALL_THROUGH;
        }

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

            FALL_THROUGH;
        }

        case WMQ_NET_DISCONNECT:
        {
            mqttCtx->stat = WMQ_NET_DISCONNECT;

            rc = MqttClient_NetDisconnect(&mqttCtx->client);
            if (rc == MQTT_CODE_CONTINUE) {
                return rc;
            }
            PRINTF("MQTT Socket Disconnect: %s (%d)",
                MqttClient_ReturnCodeToString(rc), rc);

            FALL_THROUGH;
        }

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

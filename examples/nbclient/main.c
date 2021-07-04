
#include "nbclient.h"

#include "examples/mqttexample.h"

#include <process.h>
#include <stdlib.h>

/* Default Configurations */

#define RING_BUFFER_CAPACITY (1024 * 512)

byte rx_buffer[1024];
byte tx_buffer[1024];
byte rb_buffer[RING_BUFFER_CAPACITY];

MqttTopic topics_list[128];

char client_id_buffer[128];

static void mqtt_nbclient_thread(void *arg)
{
  MQTTCtx *mqttCtx = (MQTTCtx *)arg;
  PRINTF("mqtt_nbclient_thread");
  enum MqttPacketResponseCodes rc = MQTT_CODE_CONTINUE;
  do {
    rc = mqttclient_nb_state_machine(mqttCtx);
    if (rc != MQTT_CODE_CONTINUE) {
      if (rc == MQTT_CODE_ERROR_ROUTE_TO_HOST) {
      }
      mqttCtx->stat = WMQ_BEGIN;
    }
  } while (1);
}

void mqtt_client_create(MQTTCtx *mqttCtx)
{
  mqttclient_nb_state_init(mqttCtx);
  _beginthread(mqtt_nbclient_thread, 0, mqttCtx);
}

void mqtt_init_nbclient_ctx(MQTTCtx *mqttCtx, MQTTCtxExample *example)
{
  mqtt_init_ctx(mqttCtx, example);
  mqttCtx->app_name = "nbclient";
  mqttCtx->max_packet_size  = 1024 * 1024;
#if 0
  mqttCtx->host = DEFAULT_MQTT_HOST;
  mqttCtx->use_tls = 1;
#else
  mqttCtx->host = "127.0.0.1";
  mqttCtx->use_tls = 0;
#endif
  snprintf(client_id_buffer, sizeof(client_id_buffer), "WolfMQTTClient_time:%llu", (unsigned long long)time(NULL));
  mqttCtx->client_id = client_id_buffer;
  topics_list[0].qos = MQTT_QOS_1;
  topics_list[0].topic_filter = "wolfMQTT/example/testTopic";
  topics_list[0].return_code = MQTT_SUBSCRIBE_ACK_CODE_SUCCESS_MAX_QOS2;
#ifdef WOLFMQTT_V5
  /* Disable topic alias */
  mqttCtx->topic_alias_max = 0;
  example->topic_alias = 0;
  /* sub topic id */
  topics_list[0].sub_id = 1;
#endif

  mqttCtx->topics = topics_list;
  mqttCtx->topic_count = 1;
}

uint8_t recv_buffer[1024 * 512];

int main(int argc, const char **argv)
{
  int recv_count = 0;
  MQTTCtx mqttCtx;
  MQTTCtxExample example;
  mqtt_init_nbclient_ctx(&mqttCtx, &example);
  mqttCtx.rx_buf = rx_buffer;
  mqttCtx.rx_buf_size = sizeof(rx_buffer);
  mqttCtx.tx_buf = tx_buffer;
  mqttCtx.tx_buf_size = sizeof(tx_buffer);
  ringbuf_init(&(mqttCtx.on_message_rb), rb_buffer, RING_BUFFER_CAPACITY);
  mqtt_client_create(&mqttCtx);
  while (1) {
    int rc = MQTT_CODE_SUCCESS;
    static const char publish_payload[2048] = "Hello, the world payload";
    while (mqtt_receive_msg(&mqttCtx.on_message_rb, recv_buffer, sizeof(recv_buffer)) > 0) {
      recv_count += 1;
      uint32_t total_len;
      uint16_t topic_len;
      uint32_t topic_offset;
      uint32_t payload_len;
      uint32_t payload_offset;
      total_len = *(uint32_t *)recv_buffer;
      topic_len = *(uint16_t *)(recv_buffer + sizeof(total_len));
      topic_offset = sizeof(total_len) + sizeof(topic_len);
      payload_offset = sizeof(total_len) + sizeof(topic_len) + topic_len + 1;
      payload_len = total_len - payload_offset - 1;
#if 1
      if (recv_count % 10000 == 0) {
        PRINTF("mqtt receive msg topic:%s payload len:%d count:%d",
               (const char *)(recv_buffer + topic_offset), payload_len, recv_count);
        if (payload_len < 128) {
          PRINTF("msg payload: %s", (const char *)(recv_buffer + payload_offset));
        }
      }
#endif
    }
    rc = mqtt_publish_msg(&mqttCtx,
                          mqttCtx.topics[0].topic_filter, mqttCtx.topics[0].qos,
                          -1, publish_payload, sizeof(publish_payload));
    if (0) {
      PRINTF("app run ok");
    }
  }
}

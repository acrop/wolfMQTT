
#include "nbclient.h"

#include "examples/mqttexample.h"

#include <stdlib.h>
#include <process.h>

/* Default Configurations */

#define RING_BUFFER_CAPACITY (1024 * 128)

byte rx_buffer[1024 * 64];
byte tx_buffer[1024 * 64];
byte rb_buffer[RING_BUFFER_CAPACITY];

MqttTopic topics_list[128];

char client_id_buffer[128];

static void mqtt_nbclient_thread(void *arg)
{
  MQTTCtx *mqttCtx = (MQTTCtx *)arg;
  PRINTF("mqtt_nbclient_thread");
  enum MqttPacketResponseCodes rc = MQTT_CODE_CONTINUE;
  do
  {
    rc = mqttclient_nb_state_machine(mqttCtx);
    if (rc != MQTT_CODE_CONTINUE)
    {
      if (rc == MQTT_CODE_ERROR_ROUTE_TO_HOST)
      {
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

// https://tomeko.net/online_tools/cpp_text_escape.php?lang=en
static const char* root_ca =
"-----BEGIN CERTIFICATE-----\n"
"MIIC5zCCAc+gAwIBAgIBATANBgkqhkiG9w0BAQsFADATMREwDwYDVQQDDAhNeVRl\n"
"c3RDQTAeFw0xOTExMTUwNzI2NThaFw0yMTExMTQwNzI2NThaMCoxFzAVBgNVBAMM\n"
"DjAwMDQubm92YWxvY2FsMQ8wDQYDVQQKDAZzZXJ2ZXIwggEiMA0GCSqGSIb3DQEB\n"
"AQUAA4IBDwAwggEKAoIBAQDC5JE48PJ/BFTLEseEbrGIdYB6w29hme4KFKmAqlLQ\n"
"kpwwZJAsm/9iuXy6svJf7Tzzc173Jkgzw7DzhzSf1VgRDrOCQS+IU6s8UXfUMJt/\n"
"AmP1SkU2mUJ/+pnEGRKtVkF9LCScinI95Iwt3xngdjMYXwk+S9Le3/8782ClBwZG\n"
"vffXQ7hd5HnShgyqFVePgrKmr879NTylfvAWPwux2kdXNnbOHIrhcZm0NeMNf7hs\n"
"UNURFlqo4rA0FV9dIHMryPkM7ygoaMog2XmcCnq/jf/MfPTQPYjQ9iLPOGrYi0pY\n"
"X12uFb55duRGsvs7MIkNc8fn2VERoC69QX+GK+zAUGZ/AgMBAAGjLzAtMAkGA1Ud\n"
"EwQCMAAwCwYDVR0PBAQDAgUgMBMGA1UdJQQMMAoGCCsGAQUFBwMBMA0GCSqGSIb3\n"
"DQEBCwUAA4IBAQBpW7Ge5duo6/u3xIl0XhG/2dlSwlUUpO3Ecc13gmh44nJR66VH\n"
"BEiimsol6gIgcSTk4pVY1DLb/09Nwv0TILl3Dc4QtXhM4gIlNRR79mLVsnPTef5e\n"
"xkmesQaLihSCroHq8bONnO/Xgj5hCg8uI4j3vHtOikjABxQPOrCfc2uSrenU7aol\n"
"1HBijCY6R+pg6WxBOZ2Teiaoxjn78IxSKLXW0pLRJIPpet1hefR0sKkmPfVGyg8H\n"
"g7hqo+Houw8PQf2HLZnU656vyTlgIh6ES1x7Plb0cIw/LGr4rMkXs+DFg9SLbetT\n"
"ncT4plfucsek7ImN9Dw2w2hM2FZwB8ycZfmu\n"
"-----END CERTIFICATE-----";

static const char* device_priv_key =
"-----BEGIN RSA PRIVATE KEY-----\n"
"MIIEpAIBAAKCAQEAqsAKVhbfQEWblC8PvgubqpJasVoCEsSfvLF4b5DIAsoMeieP\n"
"26y6Vyd3njRyuigSQ6jP+mo3GyqSfeCbqfJ2dx3BNICEk7P46Bu37ewoI24pScnT\n"
"+5Rcfw//dh8WHDq0d0933i4vOKEKA6ft51AxY6KzvNrwxo6bml0Vi2DBx3WLGw+M\n"
"DiHXk2L0geSYSNjFz/u9dkgEUPhEGbNZuw2yxhwbNfaiPe2ld5Fir6iUybuj93xf\n"
"WqqNls77V6Qj7mI8pamdGFtQnkP+6l2XTa6JbunCqZo1PURtUXch5db6rMq/6rRZ\n"
"rlJM7NngPI1vv8jF4T3G4mjyT8I4KxQK1s909QIDAQABAoIBAQCAkkI2ONq6Rq+z\n"
"kQxFifAZLELmMGRHRY8SQn/xYg95KjLi+E82loVpguprUgrhabL3B3IzmS8NYa0U\n"
"47/S5COX5evJYMxze5z9CYIhwSUoKJcmXLcmRLyxYJZ3l0jK0Nl6zXfw8M3V0kz8\n"
"G8Lj3lqSL70vg5yxpkg8n8LNRHoleXvz/57HzllIUx2S6Dopc29ALJiR1lVFdPc1\n"
"z5vs6O+2e0TDmPpVTNKQMI8E+02i/W5BfJ21A7VJW0OFx9ozQU43E95VT9U3/pOz\n"
"NLjdIKXmr3Miw7+TWljwF0Ak7SL0AN/nLKHYt6PIIgs9YU1xqP44u/rtqBCeSSVE\n"
"2OBmAUcxAoGBAOo6CfZER7tLayhFNSw1Zt3UBsru+xZnCR1DBuPYn+qMJbbv/LAf\n"
"4zy14vQO9lY2d3k5Vd/zZSpIcXS12adqn7kN2d5PI4XZEVMH3O1aRcGxl1UETiQE\n"
"wiEeB5u4OdjoRxKk59MzMrGLYUaZMuyyhaw6t18ujw7DeS2IRoPgsYjzAoGBALqf\n"
"bnG0yMcwmcmsmsURB5OX9eXmSBld2ypaDUDComxTOKGo0reWwE8/tEm0VcQn2GcX\n"
"Uk5sbjvf3ZgOnDvBuUBr3yfXEW6OOw5mOaJeBJn3mncDcsaV5bujwfG6QS/KA6Ad\n"
"1JzdJDtT1Be+DoeEwInBx6WNMrCH5dXWC7CChwR3AoGAXjPVidxQVT2x7VJYXl1j\n"
"79e0m62eApaSDdjFTqHzPyv6hybiJBvPEr28d5gE7wuc5X5v0VBc4bKdHul8jl7N\n"
"ummdtFFz4gM5eoFxE2z5HTvFt4Wxv77CLPuc574iVeClpRP5wPGYc9uw1eoLlzL9\n"
"nBVJZtic5L0tYWiro6KdBI0CgYBE3zWpLOiz6hG3RcXQWFqNc5VCBNwy0FpjpNwj\n"
"PDEo/QV3U5CARFgwZvgoAy9rtrC8SvULECUWX6WtyiaKPxIY3jZ6w3ohbMgKpls6\n"
"uqvEDoaoyVMASq1/tA2NIgmQk2MHIjsmsM4APw2UvYUrKijMLgF57UP5tg1x/w5N\n"
"U750PQKBgQC9zAxKw4cNqNBCqySmdIFmfhYwpiAqHpUX1XcjjEY9b4Ym9OwQbK+f\n"
"5aGgRkmSW/Fc1ab33Gj2liLXCDN8bziri3KfMW6n9Dxhk8ppue7N5vNpjaoMLU2e\n"
"tT/aucPRjdDp9JPzZQaewIDz7OG8bJtwLfx25FiR2oWDz2kD02joag==\n"
"-----END RSA PRIVATE KEY-----";

static const char* device_cert =
"-----BEGIN CERTIFICATE-----\n"
"MIIDWjCCAkKgAwIBAgIVANIzUucLFUREa2BiJUXoRv6Z4XaIMA0GCSqGSIb3DQEB\n"
"CwUAME0xSzBJBgNVBAsMQkFtYXpvbiBXZWIgU2VydmljZXMgTz1BbWF6b24uY29t\n"
"IEluYy4gTD1TZWF0dGxlIFNUPVdhc2hpbmd0b24gQz1VUzAeFw0xNjExMzAxODIz\n"
"MzNaFw00OTEyMzEyMzU5NTlaMB4xHDAaBgNVBAMME0FXUyBJb1QgQ2VydGlmaWNh\n"
"dGUwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCqwApWFt9ARZuULw++\n"
"C5uqklqxWgISxJ+8sXhvkMgCygx6J4/brLpXJ3eeNHK6KBJDqM/6ajcbKpJ94Jup\n"
"8nZ3HcE0gISTs/joG7ft7CgjbilJydP7lFx/D/92HxYcOrR3T3feLi84oQoDp+3n\n"
"UDFjorO82vDGjpuaXRWLYMHHdYsbD4wOIdeTYvSB5JhI2MXP+712SARQ+EQZs1m7\n"
"DbLGHBs19qI97aV3kWKvqJTJu6P3fF9aqo2WzvtXpCPuYjylqZ0YW1CeQ/7qXZdN\n"
"rolu6cKpmjU9RG1RdyHl1vqsyr/qtFmuUkzs2eA8jW+/yMXhPcbiaPJPwjgrFArW\n"
"z3T1AgMBAAGjYDBeMB8GA1UdIwQYMBaAFJZuFLsbLnLXbHrfXutsILjrIB5qMB0G\n"
"A1UdDgQWBBTHoiSGnE/lSskzSaWXWflJWIC/szAMBgNVHRMBAf8EAjAAMA4GA1Ud\n"
"DwEB/wQEAwIHgDANBgkqhkiG9w0BAQsFAAOCAQEAlchZ7iW3kr6ny20ySEUhc9Dl\n"
"gEcihl6gcY7Oew0xWoUzuXBkSoOVbjPRiy9RbaLA94QgoxtZCpkF0F81Jro878+m\n"
"a5Cx0Ifj66ZAaeR3FSCtjSyLgg12peZux+VXchq3MwNb8iTD1nruIJ8kLPM+7fQy\n"
"nbGM69r7lUZ1539t9O44OB0aIRDRC+ZpYINnWjiuO7hK27oZs3HCk484C+OjRusJ\n"
"jKrLFSjEdbaUj3ukMv0sGO693Z5DqTL2t9ylM2LuE9iyiWF7DBHhuDLHsZfirjk3\n"
"7/MBDwfbv7td8GOy6C2BennS5tWOL06+8lYErP4ECEQqW6izI2Cup+O01rrjkQ==\n"
"-----END CERTIFICATE-----";
/*
openssl s_client -showcerts -connect emqx-test.growlogin.net:8883
*/
void mqtt_init_nbclient_ctx(MQTTCtx *mqttCtx, MQTTCtxExample* example)
{
  mqtt_init_ctx(mqttCtx, example);
  mqttCtx->app_name = "nbclient";
#if 1
  mqttCtx->host = "emqx-test.growlogin.net";
  mqttCtx->username = "growlogin";
  mqttCtx->password = "pass";
  mqttCtx->port = 8883;
  mqttCtx->root_ca = root_ca;
  mqttCtx->device_cert = device_cert;
  mqttCtx->device_priv_key = device_priv_key;
  mqttCtx->use_tls = 1;
#else
  mqttCtx->host = "127.0.0.1";
  mqttCtx->use_tls = 0;
#endif
  snprintf(client_id_buffer, sizeof(client_id_buffer), "WolfMQTTClient_time:%llu", (unsigned long long)time(NULL));
  mqttCtx->client_id = client_id_buffer;
  topics_list[0].qos = MQTT_QOS_0;
  topics_list[0].topic_filter = "wolfMQTT/example/testTopic";
  topics_list[0].return_code = MQTT_SUBSCRIBE_ACK_CODE_SUCCESS_MAX_QOS0;
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

uint8_t recv_buffer[WOLFMQTT_MAX_PACKET_SIZE];

int main(int argc, const char **argv)
{
  MQTTCtx mqttCtx;
  MQTTCtxExample example;
  mqtt_init_nbclient_ctx(&mqttCtx, &example);
  mqttCtx.rx_buf = rx_buffer;
  mqttCtx.rx_buf_size = sizeof(rx_buffer);
  mqttCtx.tx_buf = tx_buffer;
  mqttCtx.tx_buf_size = sizeof(tx_buffer);
  ringbuf_init(&(mqttCtx.on_message_rb), rb_buffer, RING_BUFFER_CAPACITY);
  mqtt_client_create(&mqttCtx);
  while (1)
  {
    const char publish_payload[] = "Hello, the world payload";
    if (mqtt_receive_msg(&mqttCtx, recv_buffer, sizeof(recv_buffer)) > 0)
    {
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
      PRINTF("mqtt receive msg topic:%s payload len:%d", (const char*)(recv_buffer + topic_offset), payload_len);
      if (payload_len < 128) {
        PRINTF("msg payload: %s", (const char*)(recv_buffer + payload_offset));
      }
    }
    #if 0
    if (mqtt_publish_msg(
      &mqttCtx, mqttCtx.topics[0].topic_filter, mqttCtx.topics[0].qos,
      publish_payload, sizeof(publish_payload)
      ) == MQTT_CODE_SUCCESS)
    {
      PRINTF("app run ok");
    }
    #endif
    Sleep(1000);
  }
}

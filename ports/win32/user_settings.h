/* Template build settings for Win32 */
/* This is meant to be customized */

#ifndef _WOLFMQTT_USER_SETTINGS_
#define _WOLFMQTT_USER_SETTINGS_

/* TLS Support */
#if !defined(ENABLE_MQTT_TLS)
  #undef USE_WINDOWS_API
  #if defined(_WIN32)
    #define USE_WINDOWS_API
  #endif
#endif

#ifdef ENABLE_MQTT_TLS
#include "user_settings_ssl.h"
#endif

/* MQTT-SN Support */
#undef WOLFMQTT_SN
#if 1
  #define WOLFMQTT_SN
#endif

/* MQTT v5.0 support */
#undef WOLFMQTT_V5
#if 1
  #define WOLFMQTT_V5
#endif

/* Enable property callback support */
#ifdef WOLFMQTT_V5
  /* If enable property callback */
  #undef WOLFMQTT_PROPERTY_CB
  #if 1
    #define WOLFMQTT_PROPERTY_CB
  #endif
  /* Max props count */
  #undef WOLFMQTT_MAX_PROPS
  #if 1
    #define WOLFMQTT_MAX_PROPS 128
  #endif
#endif

/* Non-blocking support */
#undef WOLFMQTT_NONBLOCK
#if 1
  #define WOLFMQTT_NONBLOCK
#endif

/* Disable socket timeout code */
#undef WOLFMQTT_NO_TIMEOUT
#if 0
  #define WOLFMQTT_NO_TIMEOUT
#endif

/* Disconnect callback support */
#undef WOLFMQTT_DISCONNECT_CB
#if 1
  #define WOLFMQTT_DISCONNECT_CB
#endif

/* Multi-threading */
#undef WOLFMQTT_MULTITHREAD
#if 1
  #define WOLFMQTT_MULTITHREAD
#endif

/* User threading */
#undef WOLFMQTT_USER_THREADING
#if 0
  #define WOLFMQTT_USER_THREADING
#endif

/* Disable STDIN/fgets capture for examples */
#undef WOLFMQTT_NO_STDIN_CAP
#if 0
  #define WOLFMQTT_NO_STDIN_CAP
#endif

/* Debugging */
#undef DEBUG_WOLFMQTT
#if 1
  #define DEBUG_WOLFMQTT
#endif

#undef WOLFMQTT_DEBUG_CLIENT
#if 0
  #define WOLFMQTT_DEBUG_CLIENT
#endif

#undef WOLFMQTT_DEBUG_SOCKET
#if 0
  #define WOLFMQTT_DEBUG_SOCKET
#endif

#undef WOLFMQTT_DEBUG_THREAD
#if 0
  #define WOLFMQTT_DEBUG_THREAD
#endif

/* Disable error strings */
#undef WOLFMQTT_NO_ERROR_STRINGS
#if 0
  #define WOLFMQTT_NO_ERROR_STRINGS
#endif

#if defined(ENABLE_MQTT_TLS)

#undef ENABLE_AWSIOT_EXAMPLE
#define ENABLE_AWSIOT_EXAMPLE

#undef ENABLE_AZUREIOTHUB_EXAMPLE
#define ENABLE_AZUREIOTHUB_EXAMPLE

#undef ENABLE_FIRMWARE_EXAMPLE
#define ENABLE_FIRMWARE_EXAMPLE

#endif

#undef HAVE_SOCKET
#if 0
  #define HAVE_SOCKET
#endif

#endif /* _WOLFMQTT_USER_SETTINGS_ */

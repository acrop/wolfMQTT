cmake_minimum_required(VERSION 3.15)

project(mqtt C)

include(GNUInstallDirs)

set(PROJECT_VERSION_MAJOR 1)
set(PROJECT_VERSION_MINOR 8)
set(PROJECT_VERSION_PATCH 0)
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")

function(add_option NAME HELP_STRING DEFAULT VALUES)
  list(FIND VALUES ${DEFAULT} IDX)
  if (${IDX} EQUAL -1)
    message(FATAL_ERROR "Failed to add option ${NAME}. Default value "
        "${DEFAULT} is not in list of possible values: ${VALUES}.")
  endif()

  if(DEFINED ${NAME})
    list(FIND VALUES ${${NAME}} IDX)
    if (${IDX} EQUAL -1)
      message(FATAL_ERROR "Failed to set option ${NAME}. Value "
          "${${NAME}} is not in list of possible values: ${VALUES}.")
    endif()
  endif()

  set(${NAME} ${DEFAULT} CACHE STRING ${HELP_STRING})
  set_property(CACHE ${NAME} PROPERTY STRINGS ${VALUES})
endfunction()

set(WOLFMQTT_EXAMPLES_HELP_STRING "Enable examples (default: disabled)")
add_option("WOLFMQTT_EXAMPLES" ${WOLFMQTT_EXAMPLES_HELP_STRING} "no" "yes;no")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../wolfssl)
  if (NOT TARGET wolfssl)
    set(WOLFSSL_USER_SETTINGS "yes" CACHE STRING "" FORCE)
    set(WOLFSSL_EXAMPLES no)
    set(WOLFSSL_CRYPT_TESTS no)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../wolfssl wolfssl)
  endif()
  set(WOLFMQTT_BUILD_WOLFSSL TRUE)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/wolfMQTT.cmake)

add_library(wolfmqtt)
wolfmqtt_steup_target(wolfmqtt)
if (WOLFMQTT_BUILD_WOLFSSL)
  if (WIN32)
    target_include_directories(wolfssl PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ports/win32>
    )
  endif()
  target_compile_definitions(wolfssl PUBLIC ENABLE_MQTT_TLS)
  target_link_libraries(wolfmqtt wolfssl)
else()
  if (WIN32)
    target_include_directories(wolfmqtt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ports/win32)
  endif()
endif()
target_compile_definitions(wolfmqtt PUBLIC WOLFMQTT_USER_SETTINGS)
target_compile_options(wolfmqtt PRIVATE
  $<$<C_COMPILER_ID:GNU>:
    -Werror=shadow
    -Werror=switch-enum
    -Werror=unused-parameter>
)

if (WOLFMQTT_EXAMPLES AND (WIN32 OR UNIX))
  add_executable(awsiot examples/aws/awsiot.c)
  wolfmqtt_steup_app_target(awsiot)

  add_executable(azureiothub examples/azure/azureiothub.c)
  wolfmqtt_steup_app_target(azureiothub)

  add_executable(fwclient examples/firmware/fwclient.c)
  wolfmqtt_steup_app_target(fwclient)

  add_executable(fwpush examples/firmware/fwpush.c)
  wolfmqtt_steup_app_target(fwpush)

  add_executable(mqttclient examples/mqttclient/mqttclient.c)
  wolfmqtt_steup_app_target(mqttclient)

  if (WOLFMQTT_BUILD_WOLFSSL)
    add_executable(mqttsimple examples/mqttsimple/mqttsimple.c)
    wolfmqtt_steup_app_target(mqttsimple)
  endif()

  add_executable(multithread examples/multithread/multithread.c)
  wolfmqtt_steup_app_target(multithread)

  add_executable(nbclient
    examples/nbclient/nbclient.c
    examples/nbclient/main.c
  )
  wolfmqtt_steup_app_target(nbclient)

  add_executable(sn-client_qos-1 examples/sn-client/sn-client_qos-1.c)
  wolfmqtt_steup_app_target(sn-client_qos-1)

  add_executable(sn-client examples/sn-client/sn-client.c)
  wolfmqtt_steup_app_target(sn-client)

  add_executable(sn-multithread examples/sn-client/sn-multithread.c)
  wolfmqtt_steup_app_target(sn-multithread)

  add_executable(wiot examples/wiot/wiot.c)
  wolfmqtt_steup_app_target(wiot)
endif()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/wolfmqtt/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/wolfmqtt/
        FILES_MATCHING PATTERN "*.h")

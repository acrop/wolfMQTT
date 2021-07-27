
function(wolfmqtt_steup_target target_name)
  target_compile_definitions(${target_name} PRIVATE
    BUILDING_WOLFMQTT
  )
  target_include_directories(${target_name} PUBLIC
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}
  )
  if (WIN32)
    get_target_property(TARGET_TYPE ${target_name} TYPE)
    if ("${TARGET_TYPE}" STREQUAL "SHARED_LIBRARY")
      target_compile_definitions(${target_name} PUBLIC _WINDLL)
    endif()
  endif()

  target_sources(${target_name} PRIVATE
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/mqtt_client.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/mqtt_packet.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/mqtt_socket.c
  )
endfunction(wolfmqtt_steup_target)

function(wolfmqtt_steup_app_target target_name)
  if (WIN32)
    target_link_libraries(${target_name} ws2_32)
  endif()
  target_sources(${target_name} PRIVATE
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/examples/mqttexample.c
    ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/examples/mqttnet.c
  )
  if (NOT "${target_name}" STREQUAL "mqttclient")
    target_sources(${target_name} PRIVATE
      ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/examples/mqttclient/mqttclient.c
    )
    target_compile_definitions(${target_name} PRIVATE MQTTCLIENT_DISABLE_TEST)
  endif()
  target_link_libraries(${target_name} wolfmqtt)
endfunction(wolfmqtt_steup_app_target)

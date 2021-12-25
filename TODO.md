# TODO List

* In multithread example, in version 5, if publish topic equals to subscribe topic,
 the topic alias would working!

* Fixes use MqttClient_WaitType to only use client->msg and it's only called in
  WaitMessage Thread

* Remove all unused msg in MQTTCtx

* Testing blocking mode of mqttclient, it's seems the server are sending some data to the client, what's that.

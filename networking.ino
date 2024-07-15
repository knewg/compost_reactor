
void manage_connection() {
  // Loop until we're reconnected
  if (WiFi.status() != WL_CONNECTED) { // Reconnect wifi
    if(!wifi.disconnected && wifi.lastReconnect > 0) { // Only warn once
      log_message(WARNING, "Wifi connection lost, awaiting reconnect");
      wifi.disconnected = true;
    }
    if (wifi.lastReconnect == 0) { //Do a full connection first time around
      wifi.lastReconnect = timing.now;
      WiFi.begin(credentials.wifi.ssid, credentials.wifi.password);
      log_message(INFO, "Connecting to Wifi");
    } else if (timing.now - wifi.lastReconnect > timing.min.reconnect.wifi) {
      wifi.lastReconnect = timing.now;
      WiFi.reconnect();
      log_message(INFO, "Reconnecting to Wifi");
    }
  }
  else
  {
    if (wifi.disconnected) {
      wifi.disconnected = false;
      log_message(DEBUG, "Wifi connection took %d ms", millis()-wifi.lastReconnect);
      IPAddress ip = WiFi.localIP();
      log_message(INFO, "Connected to Wifi with ip: %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    }
    if (!client.connected()) { // Reconnect MQTT
      if (mqtt.lastReconnect == 0 || timing.now - mqtt.lastReconnect > timing.min.reconnect.mqtt) {
        if(mqtt.lastReconnect > 0) {
          log_message(WARNING, "MQTT connection lost, reconnecting");
        }
        log_message(INFO, "Connecting to MQTT broker");
        mqtt.lastReconnect = timing.now;
        client.setServer(credentials.mqtt.server, 1883);
        client.setCallback(callback);
        // Attempt to connect
        if (client.connect(credentials.mqtt.client_id, credentials.mqtt.username, credentials.mqtt.password, mqtt.topics.connected, 1, true, "0")) {
          log_message(INFO, "Connected to MQTT broker");
          // Once connected, publish an announcement...
          client.publish(mqtt.topics.status, "connected");
          client.publish(mqtt.topics.oneShot, " ");
          client.publish(mqtt.topics.connected, "2");
          // ... and resubscribe
          client.subscribe(mqtt.topics.oneShot);
          log_message(DEBUG, "MQTT connection took %d ms", millis()-mqtt.lastReconnect);

          if(mqtt.log.position > 0) {
            flush_mqtt_log_queue();
          }
        } else {
          log_message(DEBUG, "MQTT connection attempt took %d ms", millis()-mqtt.lastReconnect);
          log_message(ERROR, "Failed connection to MQTT broker. State: %d", client.state());
        }
      }
    } else {
      // Connected run main client loop
      client.loop();
    }
  }
}

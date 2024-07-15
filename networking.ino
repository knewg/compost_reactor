unsigned long lastWifiConnectionAttempt = 0;
unsigned long lastMQTTConnectionAttempt = 0;
bool wifiWasDisconnected = true;
void manage_connection() {
  // Loop until we're reconnected
  if (WiFi.status() != WL_CONNECTED) { // Reconnect wifi
    if(!wifiWasDisconnected && lastWifiConnectionAttempt > 0) { // Only warn once
      log_message(LOG_WARNING, "Wifi connection lost, awaiting reconnect");
      wifiWasDisconnected = true;
    }
    if (lastWifiConnectionAttempt == 0) { //Do a full connection first time around
      lastWifiConnectionAttempt = now;
      WiFi.begin(wifi_ssid, wifi_password);
      log_message(LOG_INFO, "Connecting to Wifi");
    } else if (now - lastWifiConnectionAttempt > WIFI_RECONNECT_TIMEOUT) {
      lastWifiConnectionAttempt = now;
      WiFi.reconnect();
      log_message(LOG_INFO, "Reconnecting to Wifi");
    }
  }
  else
  {
    if (wifiWasDisconnected) {
      wifiWasDisconnected = false;
      log_message(LOG_DEBUG, "Wifi connection took %d ms", millis()-lastWifiConnectionAttempt);
      IPAddress ip = WiFi.localIP();
      log_message(LOG_INFO, "Connected to Wifi with ip: %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    }
    if (!client.connected()) { // Reconnect MQTT
      if (lastMQTTConnectionAttempt == 0 || now - lastMQTTConnectionAttempt > MQTT_RECONNECT_TIMEOUT) {
        if(lastMQTTConnectionAttempt > 0) {
          log_message(LOG_WARNING, "MQTT connection lost, reconnecting");
        }
        log_message(LOG_INFO, "Connecting to MQTT broker");
        lastMQTTConnectionAttempt = now;
        client.setServer(mqtt_server, 1883);
        client.setCallback(callback);
        // Attempt to connect
        if (client.connect(mqtt_client_id, mqtt_username, mqtt_password, MQTT_PREFIX MQTT_TOPIC_ONLINE, 1, true, "0")) {
          log_message(LOG_INFO, "Connected to MQTT broker");
          // Once connected, publish an announcement...
          client.publish(MQTT_PREFIX MQTT_TOPIC_STATUS , "Connected");
          client.publish(MQTT_PREFIX MQTT_TOPIC_OVERRIDE, " ");
          client.publish(MQTT_PREFIX MQTT_TOPIC_ONLINE, "2");
          // ... and resubscribe
          client.subscribe(MQTT_PREFIX MQTT_TOPIC_OVERRIDE);
          log_message(LOG_DEBUG, "MQTT connection took %d ms", millis()-lastMQTTConnectionAttempt);

          if(mqtt_log_queue_pos > 0) {
            flush_mqtt_log_queue();
          }
        } else {
          log_message(LOG_DEBUG, "MQTT connection attempt took %d ms", millis()-lastMQTTConnectionAttempt);
          log_message(LOG_ERROR, "Failed connection to MQTT broker. State: %d", client.state());
        }
      }
    } else {
      // Connected run main client loop
      client.loop();
    }
  }
}

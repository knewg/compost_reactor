unsigned long lastWifiConnectionAttempt = 0;
unsigned long lastMQTTConnectionAttempt = 0;
bool wifiWasDisconnected = true;
void manage_connection() {
  // Loop until we're reconnected
  if (WiFi.status() != WL_CONNECTED) { // Reconnect wifi
    if(!wifiWasDisconnected && lastWifiConnectionAttempt > 0) { // Only warn once
      log_message(WARNING, "Wifi connection lost, awaiting reconnect");
      wifiWasDisconnected = true;
    }
    if (lastWifiConnectionAttempt == 0) { //Do a full connection first time around
      lastWifiConnectionAttempt = now;
      WiFi.begin(credentials.wifi.ssid, credentials.wifi.password);
      log_message(INFO, "Connecting to Wifi");
    } else if (now - lastWifiConnectionAttempt > timing.min.reconnect.wifi) {
      lastWifiConnectionAttempt = now;
      WiFi.reconnect();
      log_message(INFO, "Reconnecting to Wifi");
    }
  }
  else
  {
    if (wifiWasDisconnected) {
      wifiWasDisconnected = false;
      log_message(DEBUG, "Wifi connection took %d ms", millis()-lastWifiConnectionAttempt);
      IPAddress ip = WiFi.localIP();
      log_message(INFO, "Connected to Wifi with ip: %u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    }
    if (!client.connected()) { // Reconnect MQTT
      if (lastMQTTConnectionAttempt == 0 || now - lastMQTTConnectionAttempt > timing.min.reconnect.mqtt) {
        if(lastMQTTConnectionAttempt > 0) {
          log_message(WARNING, "MQTT connection lost, reconnecting");
        }
        log_message(INFO, "Connecting to MQTT broker");
        lastMQTTConnectionAttempt = now;
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
          log_message(DEBUG, "MQTT connection took %d ms", millis()-lastMQTTConnectionAttempt);

          if(mqtt.log.position > 0) {
            flush_mqtt_log_queue();
          }
        } else {
          log_message(DEBUG, "MQTT connection attempt took %d ms", millis()-lastMQTTConnectionAttempt);
          log_message(ERROR, "Failed connection to MQTT broker. State: %d", client.state());
        }
      }
    } else {
      // Connected run main client loop
      client.loop();
    }
  }
}

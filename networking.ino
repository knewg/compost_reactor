void setupMqttTopics() {
  for (byte i = 0; i < numOutputs; i++) {
    strcpy(mqtt.topics.output[i], MQTT_PREFIX "output/");
    strcat(mqtt.topics.output[i], outputs[i].topic);
  }
  for (byte i = 0; i < numInputs; i++) {
    strcpy(mqtt.topics.input[i], MQTT_PREFIX "input/");
    strcat(mqtt.topics.input[i], inputs[i].topic);
  }
  strcpy(mqtt.writeTopics.oneShot, mqtt.topics.oneShot);
  strcat(mqtt.writeTopics.oneShot, "/set");
  strcpy(mqtt.writeTopics.recipe, mqtt.topics.recipe);
  strcat(mqtt.writeTopics.recipe, "/set");
  strcpy(mqtt.writeTopics.logLevel, mqtt.topics.logLevel);
  strcat(mqtt.writeTopics.logLevel, "/set");

}

void publishMqttOutputState(byte output) {
  client.publish(mqtt.topics.output[output], outputs[output].state ? "1" : "0");
}

void publishMqttInputState(byte input) {
  client.publish(mqtt.topics.input[input], inputs[input].state.current ? "1" : "0");
}

void publishMqttIOStatus() {
  for (byte i = 0; i < numOutputs; i++) {
    publishMqttOutputState(i);
  }
  for (byte i = 0; i < numInputs; i++) {
    publishMqttInputState(i);
  }
}

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
        client.setCallback(mqttCallback);
        // Attempt to connect
        if (client.connect(credentials.mqtt.client_id, credentials.mqtt.username, credentials.mqtt.password, mqtt.topics.connected, 1, true, "0")) {
          log_message(INFO, "Connected to MQTT broker");
          // Once connected, publish an announcement...
          client.publish(mqtt.topics.status, "connected");
          if(strcmp(recipe.oneShot, "") == 0) {
            client.publish(mqtt.topics.oneShot, " ");
          } else {
            client.publish(mqtt.topics.oneShot, recipe.oneShot);
          }
          char logLevel[3];
          sprintf(logLevel, "%d", settings.logLevel);
          client.publish(mqtt.topics.recipe, recipe.rolling);
          client.publish(mqtt.topics.logLevel, logLevel);
          client.publish(mqtt.topics.connected, "2");
          publishMqttIOStatus();
          // ... and resubscribe
          client.subscribe(mqtt.writeTopics.oneShot);
          client.subscribe(mqtt.writeTopics.recipe);
          client.subscribe(mqtt.writeTopics.logLevel);
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
      long rssi = WiFi.RSSI();
      if(abs(rssi - wifi.rssi) > 2) {
        wifi.rssi = rssi;
        char rssiStr [12];
        sprintf(rssiStr, "%d", rssi);
        client.publish(mqtt.topics.rssi, rssiStr);
      }
      client.loop();
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char sendMessage[100] = "";
  log_message(DEBUG, "Message arrived [%s]", topic);
  if(length == 0) {
    log_message(DEBUG, "Empty messsage arrived [%s]", topic);
    return;
  }
  for (int i = 0; i < length; i++) {
    if ((char)payload[i] == '\n') continue;
    sendMessage[i] = ((char)payload[i]);
  }
  sendMessage[length] = '\0';
  if (strcmp(topic, mqtt.writeTopics.oneShot) == 0) {
    log_message(USER_INPUT, "New one shot recipe set to %s", sendMessage);
    strcpy(recipe.oneShot, sendMessage);
    if(recipe.current.recipe == &recipe.oneShot[0]) { //Currently using oneshot recipe
      log_message(WARNING, "New one shot recipe activated while running. Old recipe discarded.");
      recipe.current.position = 0;
    }
    client.publish(mqtt.topics.oneShot, sendMessage);
  } else if (strcmp(topic, mqtt.writeTopics.recipe) == 0) {
    if(strcmp(recipe.rolling, sendMessage) == 0) { //Received the same message
      log_message(USER_INPUT, "Received duplicate recipe %s", sendMessage);
    } else {
      log_message(USER_INPUT, "New rolling recipe set to %s", sendMessage);
      if(recipe.current.recipe == &recipe.rolling[0]) { //Currently using rolling recipe
        log_message(INFO, "New rolling recipe activated while running. Restarting from instruction 1.");
        recipe.current.position = 0;
      }
      strcpy(recipe.rolling, sendMessage);
      client.publish(mqtt.topics.recipe, sendMessage);
    }
  } else if (strcmp(topic, mqtt.writeTopics.logLevel) == 0) {
    byte logLevel = atoi(sendMessage);
    log_message(USER_INPUT, "New log level set to %d", logLevel);
    settings.logLevel = logLevel;
    client.publish(mqtt.topics.logLevel, sendMessage);
  }
  log_message(USER_INPUT, "Topic:  %s, Message: %s", topic, sendMessage);
}

void mqttClearOneShotRecipe() {
  if(!client.connected()) {
    log_message(ERROR, "Could not clear one shot message, not connected");
  }
  client.publish(mqtt.topics.oneShot, "");
}

void mqttUpdateStatus(char * message) {
  if(!client.connected()) {
    log_message(ERROR, "Could not update status, not connected");
  }
  client.publish(mqtt.topics.status, message);
}


#define LOG_PREFIX_LENGTH 7

const char logLevels [][LOG_PREFIX_LENGTH + 1] {
  "ERROR: ",
  "WARN:  ",
  "INFO:  ",
  "DEBUG: "
};

void log_message(byte level, const char * message, ...) {
  if (settings.logLevel & level == 0)
    return;
  char log_buffer[100];

  // For ... args to work
  va_list args;
  va_start(args, message);
  vsprintf(log_buffer, message, args);
  va_end(args);

  // Prepend prefix
  memmove(log_buffer + LOG_PREFIX_LENGTH, log_buffer, strlen(log_buffer) + 1);
  memcpy(log_buffer, logLevels[level], LOG_PREFIX_LENGTH);

#ifdef SERIAL
  Serial.println(log_buffer);
#endif
  if(client.connected()) {
    if(mqtt_log_queue_pos > 0) {
      flush_mqtt_log_queue();
    }
    client.publish(MQTT_PREFIX MQTT_TOPIC_LOG, log_buffer);
  }
  else if (mqtt_log_queue_pos < MQTT_LOG_QUEUE_MAX) { //Keep log in buffer while it reconnects
    strcpy(mqtt_log_queue[mqtt_log_queue_pos], log_buffer);
    ++mqtt_log_queue_pos;
  }
}

void flush_mqtt_log_queue() {
  for(byte i = 0; i <mqtt_log_queue_pos; ++i) {
    client.publish(MQTT_PREFIX MQTT_TOPIC_LOG, mqtt_log_queue[i]);
  }
  if(mqtt_log_queue_pos >= MQTT_LOG_QUEUE_MAX-1) {
    log_message(LOG_WARNING, "Max MQTT Log queue reached, probably missing some entries");
  }
  log_message(LOG_WARNING, "Replayed %d log entries due to connectivity issues", mqtt_log_queue_pos);
  mqtt_log_queue_pos = 0;
}

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
  int logLabel = 0;
  while (level >>= 1) ++logLabel;

  // Prepend prefix
  memmove(log_buffer + LOG_PREFIX_LENGTH, log_buffer, strlen(log_buffer) + 1);
  memcpy(log_buffer, logLevels[logLabel], LOG_PREFIX_LENGTH);

#ifdef SERIAL
  Serial.println(log_buffer);
#endif
  if(client.connected()) {
    if(mqtt.log.position > 0) {
      flush_mqtt_log_queue();
    }
    client.publish(mqtt.topics.log, log_buffer);
  }
  else if (mqtt.log.position < mqtt.log.max) { //Keep log in buffer while it reconnects
    strcpy(mqtt.log.queue[mqtt.log.position], log_buffer);
    ++mqtt.log.position;
  }
}

void flush_mqtt_log_queue() {
  for(byte i = 0; i <mqtt.log.position; ++i) {
    client.publish(mqtt.topics.log, mqtt.log.queue[i]);
  }
  if(mqtt.log.position >= mqtt.log.max-1) {
    log_message(WARNING, "Max MQTT Log queue reached, probably missing some entries");
  }
  log_message(WARNING, "Replayed %d log entries due to connectivity issues", mqtt.log.position);
  mqtt.log.position = 0;
}

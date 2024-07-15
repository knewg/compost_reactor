#define MANUAL_REVERSE_BUTTON_PIN 34
#define MANUAL_FORWARD_BUTTON_PIN 35
#define CONTACTOR_FORWARD_FEEDBACK_PIN 26
#define CONTACTOR_REVERSE_FEEDBACK_PIN 25
#define MOTOR_PROTECTOR_FEEDBACK_PIN 27
#define EMERGENCY_LOOP_FEEDBACK_PIN 14
#define INDUCTION_SENSOR_FEEDBACK_PIN 32
#define SPARE_FEEDBACK_PIN 33

#define CONTACTOR_FORWARD_PIN 18
#define CONTACTOR_REVERSE_PIN 19
#define RELAY_FAN_PIN 5
#define RELAY_SPARE_PIN 17

#define SERIAL_INPUT_LENGTH 30
#define LOG_PREFIX_LENGTH 7

#define MQTT_LOG_QUEUE_MAX 10
#define MQTT_PREFIX "hallfors/compost_reactor/"


enum LogLevel {
  ERROR = 1,
  WARNING = 2,
  USER_INPUT = 4,
  INFO = 8,
  DEBUG = 16
};

struct Timing {
  struct Delay {
    const unsigned int fanStartup;
    const unsigned int debounce;
  } delay;
  struct Grace {
    const unsigned int inductionSensor;
    const unsigned int serialInput;
  } grace;
  struct Min {
    struct Reconnect {
      const unsigned int mqtt;
      const unsigned int wifi;
    } reconnect;
    struct Wait {
      const unsigned int fanTurnOff;
    } wait;
  } min;
  struct Max {
    const unsigned long rotation;
  } max;
  unsigned long now;
} timing = {
  .delay = {
    .fanStartup = 1000,
    .debounce = 500
  },
  .grace = {
    .inductionSensor = 2000,
    .serialInput = 1000
  },
  .min = {
    .reconnect = {
      .mqtt = 5000,
      .wifi = 30000
    },
    .wait = {
      .fanTurnOff = 5000
    }
  },
  .max = {
    .rotation = 60000
  },
  .now = 0
};


#include <WiFi.h>
#include <PubSubClient.h>

#include "credentials.h"

enum Inputs {
  MANUAL_REVERSE_BUTTON = 0,
  MANUAL_FORWARD_BUTTON = 1,
  CONTACTOR_FORWARD_FEEDBACK = 2,
  CONTACTOR_REVERSE_FEEDBACK = 3,
  MOTOR_PROTECTOR_FEEDBACK = 4,
  EMERGENCY_LOOP_FEEDBACK = 5,
  INDUCTION_SENSOR_FEEDBACK = 6,
  SPARE_FEEDBACK = 7
};

struct Input {
  byte pin;
  struct InputState {
    bool current;
    bool debounce;
    bool next;
    unsigned long debounceTime;
  } state;
};

const byte numInputs = 8;
struct Input inputs[numInputs] = {
  [MANUAL_REVERSE_BUTTON] = { .pin = MANUAL_REVERSE_BUTTON_PIN },
  [MANUAL_FORWARD_BUTTON] = { .pin = MANUAL_FORWARD_BUTTON_PIN },
  [CONTACTOR_FORWARD_FEEDBACK] = { .pin = CONTACTOR_FORWARD_FEEDBACK_PIN },
  [CONTACTOR_REVERSE_FEEDBACK] = { .pin = CONTACTOR_REVERSE_FEEDBACK_PIN },
  [MOTOR_PROTECTOR_FEEDBACK] = { .pin = MOTOR_PROTECTOR_FEEDBACK_PIN },
  [EMERGENCY_LOOP_FEEDBACK] = { .pin = EMERGENCY_LOOP_FEEDBACK_PIN },
  [INDUCTION_SENSOR_FEEDBACK] = { .pin = INDUCTION_SENSOR_FEEDBACK_PIN },
  [SPARE_FEEDBACK] = { .pin = SPARE_FEEDBACK_PIN }
};

enum Outputs {
  CONTACTOR_FORWARD = 0,
  CONTACTOR_REVERSE = 1,
  RELAY_FAN = 2,
  RELAY_SPARE = 3
};

struct Output {
  bool state;
  byte pin;
};

const byte numOutputs = 4;
struct Output outputs[numOutputs] = {
  [CONTACTOR_FORWARD] = { .pin = CONTACTOR_FORWARD_PIN },
  [CONTACTOR_REVERSE] = { .pin = CONTACTOR_REVERSE_PIN },
  [RELAY_FAN] = { .pin = RELAY_FAN_PIN },
  [RELAY_SPARE] = { .pin = RELAY_SPARE_PIN }
};

struct Recipe {
  struct CurrentInstruction {
    char instruction;
    int value;
    byte position;
  } current;
  char * oneShot;
  char * rolling;
} recipe = {
  .current = {
    .instruction = '0',
    .value = 0,
    .position = 0,
  },
  .oneShot = "",
  .rolling = "W10 F2 R1 W10",
};

struct Settings {
  byte logLevel;
} settings = {
  .logLevel = INFO + USER_INPUT + WARNING + ERROR
};

struct Mqtt {
  struct Topics {
    const char * log;
    const char * oneShot;
    const char * status;
    const char * settings;
    const char * connected;
  } topics;
  struct Log {
    char queue [MQTT_LOG_QUEUE_MAX][100];
    byte max;
    byte position;
  } log;
  unsigned long lastReconnect;
} mqtt = {
  .topics = {
    .log = MQTT_PREFIX "log",
    .oneShot = MQTT_PREFIX "one_shot_recipe",
    .status = MQTT_PREFIX "status",
    .settings = MQTT_PREFIX "settings",
    .connected = MQTT_PREFIX "connected"
  },
  .log = {
    .max = MQTT_LOG_QUEUE_MAX,
    .position = 0
  },
  .lastReconnect = 0
};

struct Wifi {
  WiFiClient client;
  bool disconnected;
  unsigned long lastReconnect;
} wifi = {
  .disconnected = true,
  .lastReconnect = 0
};

PubSubClient client(wifi.client);

void setup() {
  for (byte i = 0; i < numOutputs; i++) { //Reset all inputs to input, and  low
    pinMode(outputs[i].pin, OUTPUT);
    digitalWrite(outputs[i].pin, HIGH);
  }
  for (byte i = 0; i < numInputs; i++) { //Reset all inputs to input, and  low
    pinMode(inputs[i].pin, INPUT_PULLUP);
    inputs[i].state.current = LOW;
    inputs[i].state.next = LOW;
    inputs[i].state.debounce = LOW;
    inputs[i].state.debounceTime = 0;
  }
  Serial.begin(9600);    // Initialize the Serial monitor for debugging
  timing.now = millis();
  WiFi.mode(WIFI_STA); //Optional
  WiFi.setHostname(credentials.mqtt.client_id);
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  manage_connection();
  delay(1000);
}

void callback(char* topic, byte* payload, unsigned int length) {
  char sendMessage[100] = "";
  log_message(DEBUG, "Message arrived [%s]", topic);
  for (int i = 0; i < length; i++) {
    if ((char)payload[i] == '\n') continue;
    sendMessage[i] = ((char)payload[i]);
  }
  sendMessage[length] = '\0';
  if (strcmp(topic, "set")) {
    if (sendMessage == "recipe") {

    } else {

    }
  }
  log_message(USER_INPUT, "Topic:  %s, Message: %s", topic, sendMessage);
}

void loop() {
  timing.now = millis();
  manage_connection();
  check_inputs();
  check_serial_input();
  process_instructions();
}

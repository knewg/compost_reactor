#define MANUAL_REVERSE_BUTTON 0
#define MANUAL_FORWARD_BUTTON 1
#define CONTACTOR_FORWARD_FEEDBACK 2
#define CONTACTOR_REVERSE_FEEDBACK 3
#define MOTOR_PROTECTOR_FEEDBACK 4
#define EMERGENCY_LOOP_FEEDBACK 5
#define INDUCTION_SENSOR_FEEDBACK 6
#define SPARE_FEEDBACK 7

#define MANUAL_REVERSE_BUTTON_PIN 34
#define MANUAL_FORWARD_BUTTON_PIN 35
#define CONTACTOR_FORWARD_FEEDBACK_PIN 26
#define CONTACTOR_REVERSE_FEEDBACK_PIN 25
#define MOTOR_PROTECTOR_FEEDBACK_PIN 27
#define EMERGENCY_LOOP_FEEDBACK_PIN 14
#define INDUCTION_SENSOR_FEEDBACK_PIN 32
#define SPARE_FEEDBACK_PIN 33

#define CONTACTOR_FORWARD 0
#define CONTACTOR_REVERSE 1
#define RELAY_FAN 2
#define RELAY_SPARE 3

#define CONTACTOR_FORWARD_PIN 18
#define CONTACTOR_REVERSE_PIN 19
#define RELAY_FAN_PIN 5
#define RELAY_SPARE_PIN 17

#define SERIAL_INPUT_LENGTH 30

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
    unsigned long rotation;
  } max;
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
  }
};

#include <WiFi.h>
#include <PubSubClient.h>

#include "credentials.h"

WiFiClient wifiClient;
PubSubClient client(wifiClient);
unsigned long lastMessage = 0;
unsigned long now = 0;
int value = 0;

const byte numInputs = 8;
struct Input {
  byte pin;
  struct InputState {
    bool current;
    bool debounce;
    bool next;
    unsigned long debounceTime;
  } state;
};
struct Input inputs[numInputs] = {
  { .pin = MANUAL_REVERSE_BUTTON_PIN },
  { .pin = MANUAL_FORWARD_BUTTON_PIN },
  { .pin = CONTACTOR_FORWARD_FEEDBACK_PIN },
  { .pin = CONTACTOR_REVERSE_FEEDBACK_PIN },
  { .pin = MOTOR_PROTECTOR_FEEDBACK_PIN },
  { .pin = EMERGENCY_LOOP_FEEDBACK_PIN },
  { .pin = INDUCTION_SENSOR_FEEDBACK_PIN },
  { .pin = SPARE_FEEDBACK_PIN }
};

const byte numOutputs = 4;
struct Output {
  bool state;
  byte pin;
};
struct Output outputs[numOutputs] = {
  { .pin = CONTACTOR_FORWARD_PIN },
  { .pin = CONTACTOR_REVERSE_PIN },
  { .pin = RELAY_FAN_PIN },
  { .pin = RELAY_SPARE_PIN }
};

//char * standardRecipe = "W10 F2 R1 W10";
char * overrideRecipe = "";

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
  char * recipe;
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
    char * queue [MQTT_LOG_QUEUE_MAX];
    byte max;
    byte position;
  } log;
  int reconnect_timeout_ms;
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
  }
};

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
  #ifdef SERIAL
    Serial.begin(9600);    // Initialize the Serial monitor for debugging
  #endif
  now = millis();
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

int charToInt(char character) {
  int returnValue = character - '0';
  if (returnValue > 9 || returnValue < 0) {
    log_message(ERROR, "Int conversion error, %c is not an int", character);
    return 0;
  }
  return returnValue;
}

void getNextInstruction() {
  log_message(DEBUG, "getNextInstruction: Loading new instruction");
  recipe.current.instruction = '0';
  recipe.current.value = 0;
  bool instructionFound = false;
  bool valueFound = false;
  byte recipeLength = strlen(settings.recipe);
  log_message(DEBUG, "getNextInstruction: Recipelength: %d", recipeLength);
  for (; recipe.current.position < recipeLength; recipe.current.position++)
  {
    char nextChar = settings.recipe[recipe.current.position];
    log_message(DEBUG, "getNextInstruction: nextChar %c", nextChar);
    if (nextChar == ' ' || nextChar == 'R' || nextChar == 'F' || nextChar == 'W') {
      if (instructionFound) { //Instruction already found, recipe read
        if (valueFound) {
          return; //Return successfully
        }
        // No value, fail
        log_message(ERROR, "getNextInstruction: Recipe error, no value after instruction");
        recipe.current.instruction = '0';
        recipe.current.value = 0;
      }
      if(nextChar != ' ') { // Allow (optional) whitespace in recipe
        recipe.current.instruction = nextChar;
        instructionFound = true;
      }
    } else { //Expect value
      if (!instructionFound) {
        log_message(ERROR, "getNextInstruction: Recipe error, value before instruction");
        recipe.current.instruction = '0';
        recipe.current.value = 0;
        return;
      }
      if (valueFound) { //Multiply original value with 10
        recipe.current.value = recipe.current.value * 10;
      } else {
        valueFound = true;
      }
      recipe.current.value = recipe.current.value + charToInt(nextChar);
    }
  }
  recipe.current.position = 0;
  // Reached the end, ensure we got an instruction
  if (instructionFound) { //Instruction already found, recipe read
    if (valueFound) {
      log_message(DEBUG, "getNextInstruction: End of recipe");
      return; //Return successfully
    }
    // No value, fail
    log_message(ERROR, "getNextInstruction: Recipe error, recipe ended without value");
    recipe.current.instruction = '0';
    recipe.current.value = 0;
  }
  log_message(ERROR, "getNextInstruction: Recipe error, recipe ended without instruction");
  recipe.current.instruction = '0';
  recipe.current.value = 0;
}

void process_instructions() {
  if (recipe.current.instruction == '0') {
    getNextInstruction();
  }
  if (recipe.current.instruction == 'R' || recipe.current.instruction == 'F') { // Rotate drum
    rotate_drum();
  }
  else if (recipe.current.instruction == 'W') { // Wait
    run_wait_timer();
  }
}

unsigned long wait_timer_start = 0;
void run_wait_timer() {
  if(recipe.current.value > timing.min.wait.fanTurnOff) { //Longer than 5 seconds wait, turn off fan
    set_output(RELAY_FAN, false);
  }
  if (wait_timer_start == 0) { //Timer stopped
    log_message(INFO, "Waiting for %d seconds", recipe.current.value);
    wait_timer_start = now;
    return;
  }
  if (now - wait_timer_start >= recipe.current.value) {
    log_message(INFO, "%d second wait finished", recipe.current.value);
    wait_timer_start = 0;
    recipe.current.instruction = '0';
  }

}

void loop() {
  now = millis();
  manage_connection();
  check_inputs();
  #ifdef SERIAL
    check_serial_input();
  #endif
  process_instructions();
}

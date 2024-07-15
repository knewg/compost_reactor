#define NUM_INPUTS 8
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


#define NUM_OUTPUTS 4
#define CONTACTOR_FORWARD 0
#define CONTACTOR_REVERSE 1
#define RELAY_FAN 2
#define RELAY_SPARE 3

#define CONTACTOR_FORWARD_PIN 18
#define CONTACTOR_REVERSE_PIN 19
#define RELAY_FAN_PIN 5
#define RELAY_SPARE_PIN 17

#define DEBOUNCE_TIME  500
#define WIFI_RECONNECT_TIMEOUT 30000
#define MQTT_RECONNECT_TIMEOUT 5000
#define MQTT_LOG_QUEUE_MAX 10

#define ROTATION_MAX_TIME 10
#define MIN_WAIT_FAN_TURNOFF 5
#define INDUCTION_SENSOR_GRACE_TIME_MS 2000
#define FAN_STARTUP_DELAY_MS 1000

#define LOG_ERROR 1
#define LOG_WARNING 2
#define LOG_INPUT 4
#define LOG_INFO 8
#define LOG_DEBUG 16

#define MQTT_PREFIX "hallfors/compost_reactor/"
#define MQTT_TOPIC_LOG "log"
#define MQTT_TOPIC_OVERRIDE "override"
#define MQTT_TOPIC_STATUS "status"
#define MQTT_TOPIC_SETTINGS "settings"
#define MQTT_TOPIC_ONLINE "$connected"

#include <WiFi.h>
#include <PubSubClient.h>

#include "credentials.h"

//byte log_level = LOG_INFO + LOG_INPUT + LOG_WARNING + LOG_ERROR;

WiFiClient wificlient;
PubSubClient client(wificlient);
unsigned long lastMessage = 0;
unsigned long now = 0;
int value = 0;

bool inputsCurrentStates[NUM_INPUTS];
bool inputsDebounceStates[NUM_INPUTS];
bool inputsNewStates[NUM_INPUTS];
unsigned long inputsDebounceTimes[NUM_INPUTS];

const byte inputsPins[NUM_INPUTS] = {
  MANUAL_REVERSE_BUTTON_PIN,
  MANUAL_FORWARD_BUTTON_PIN,
  CONTACTOR_FORWARD_FEEDBACK_PIN,
  CONTACTOR_REVERSE_FEEDBACK_PIN,
  MOTOR_PROTECTOR_FEEDBACK_PIN,
  EMERGENCY_LOOP_FEEDBACK_PIN,
  INDUCTION_SENSOR_FEEDBACK_PIN,
  SPARE_FEEDBACK_PIN
};

struct Input {
  bool currentState;
  bool debounceState;
  unsigned long debounceTime;
  bool newState;
  byte pin;
}

struct Input inputs[NUM_INPUTS];
inputs[0].pin = MANUAL_REVERSE_BUTTON_PIN;
inputs[1].pin = MANUAL_FORWARD_BUTTON_PIN;
inputs[2].pin = CONTACTOR_FORWARD_FEEDBACK_PIN;
inputs[3].pin = CONTACTOR_REVERSE_FEEDBACK_PIN;
inputs[4].pin = MOTOR_PROTECTOR_FEEDBACK_PIN;
inputs[5].pin = EMERGENCY_LOOP_FEEDBACK_PIN;
inputs[6].pin = INDUCTION_SENSOR_FEEDBACK_PIN;
inputs[7].pin = SPARE_FEEDBACK_PIN;

bool output_states[NUM_OUTPUTS] = {0};
const byte output_pins[NUM_OUTPUTS] = {
  CONTACTOR_FORWARD_PIN,
  CONTACTOR_REVERSE_PIN,
  RELAY_FAN_PIN,
  RELAY_SPARE_PIN
};


//char * standardRecipe = "W10 F2 R1 W10";
char * overrideRecipe = "";

char currentInstruction = '0';
int currentValue = 0;
bool processingInstruction = false;
byte recipePosition = 0;

char * mqtt_log_queue[MQTT_LOG_QUEUE_MAX]; //Save
byte mqtt_log_queue_pos = 0;

struct Settings {
  char * recipe;
  byte logLevel;
};

struct Settings settings = {
  .recipe = "W10 F2 R1 W10",
  .logLevel = LOG_INFO + LOG_INPUT + LOG_WARNING + LOG_ERROR
};




void setup() {
  for (byte i = 0; i < NUM_OUTPUTS; i++) { //Reset all inputs to input, and  low
    pinMode(output_pins[i], OUTPUT);
    digitalWrite(output_pins[i], HIGH);
  }
  for (byte i = 0; i < NUM_INPUTS; i++) { //Reset all inputs to input, and  low
    pinMode(inputs[i].pin, INPUT_PULLUP);
    inputsCurrentStates[i] = LOW;
    inputsDebounceStates[i] = LOW;
    inputsNewStates[i] = LOW;
    inputsDebounceTimes[i] = 0;
  }
  #ifdef SERIAL
    Serial.begin(9600);    // Initialize the Serial monitor for debugging
  #endif
  now = millis();
  WiFi.mode(WIFI_STA); //Optional
  WiFi.setHostname(mqtt_client_id);
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  manage_connection();
  delay(1000);
}



void callback(char* topic, byte* payload, unsigned int length) {
  char sendMessage[100] = "";
  log_message(LOG_DEBUG, "Message arrived [%s]", topic);
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
  log_message(LOG_INPUT, "Topic:  %s, Message: %s", topic, sendMessage);
}

int charToInt(char character) {
  int returnValue = character - '0';
  if (returnValue > 9 || returnValue < 0) {
    log_message(LOG_ERROR, "Int conversion error, %c is not an int", character);
    return 0;
  }
  return returnValue;
}

void getNextInstruction() {
  log_message(LOG_DEBUG, "getNextInstruction: Loading new instruction");
  currentInstruction = '0';
  currentValue = 0;
  bool instructionFound = false;
  bool valueFound = false;
  byte recipeLength = strlen(settings.recipe);
  log_message(LOG_DEBUG, "getNextInstruction: Recipelength: %d", recipeLength);
  for (; recipePosition < recipeLength; recipePosition++)
  {
    char nextChar = settings.recipe[recipePosition];
    log_message(LOG_DEBUG, "getNextInstruction: nextChar %c", nextChar);
    if (nextChar == ' ' || nextChar == 'R' || nextChar == 'F' || nextChar == 'W') {
      if (instructionFound) { //Instruction already found, recipe read
        if (valueFound) {
          return; //Return successfully
        }
        // No value, fail
        log_message(LOG_ERROR, "getNextInstruction: Recipe error, no value after instruction");
        currentInstruction = '0';
        currentValue = 0;
      }
      if(nextChar != ' ') { // Allow (optional) whitespace in recipe
        currentInstruction = nextChar;
        instructionFound = true;
      }
    } else { //Expect value
      if (!instructionFound) {
        log_message(LOG_ERROR, "getNextInstruction: Recipe error, value before instruction");
        currentInstruction = '0';
        currentValue = 0;
        return;
      }
      if (valueFound) { //Multiply original value with 10
        currentValue = currentValue * 10;
      } else {
        valueFound = true;
      }
      currentValue = currentValue + charToInt(nextChar);
    }
  }
  recipePosition = 0;
  // Reached the end, ensure we got an instruction
  if (instructionFound) { //Instruction already found, recipe read
    if (valueFound) {
      log_message(LOG_DEBUG, "getNextInstruction: End of recipe");
      return; //Return successfully
    }
    // No value, fail
    log_message(LOG_ERROR, "getNextInstruction: Recipe error, recipe ended without value");
    currentInstruction = '0';
    currentValue = 0;
  }
  log_message(LOG_ERROR, "getNextInstruction: Recipe error, recipe ended without instruction");
  currentInstruction = '0';
  currentValue = 0;
}

void process_instructions() {
  if (currentInstruction == '0') {
    getNextInstruction();
  }
  if (currentInstruction == 'R' || currentInstruction == 'F') { // Rotate drum
    rotate_drum();
  }
  else if (currentInstruction == 'W') { // Wait
    run_wait_timer();
  }
}

unsigned long wait_timer_start = 0;
void run_wait_timer() {
  if(currentValue > MIN_WAIT_FAN_TURNOFF) { //Longer than 5 seconds wait, turn off fan
    set_output(RELAY_FAN, false);
  }
  if (wait_timer_start == 0) { //Timer stopped
    log_message(LOG_INFO, "Waiting for %d seconds", currentValue);
    wait_timer_start = now;
    return;
  }
  if ((now - wait_timer_start) / 1000 >= currentValue) {
    log_message(LOG_INFO, "%d second wait finished", currentValue);
    wait_timer_start = 0;
    currentInstruction = '0';
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

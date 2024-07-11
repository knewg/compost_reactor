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

#define ROTATION_MAX_TIME 10
#define MIN_WAIT_FAN_TURNOFF 5
#define INDUCTION_SENSOR_GRACE_TIME_MS 2000
#define FAN_STARTUP_DELAY_MS 1000
#define SERIAL_INPUT_GRACE_TIME_MS 1000

#define SERIAL_INPUT_LENGTH 30

#define LOG_ERROR 0
#define LOG_WARNING 1
#define LOG_INFO 2
#define LOG_DEBUG 3
#define LOG_PREFIX_LENGTH 7

#define LOG_LEVEL LOG_INFO

#define MQTT_PREFIX "hallfors/compostReactor/"
#define MQTT_TOPIC_LOG "log"
#define MQTT_TOPIC_OVERRIDE "override"
#define MQTT_TOPIC_STATUS "status"
#define MQTT_TOPIC_ONLINE "$online"

#include <WiFi.h>
#include <PubSubClient.h>

#include "credentials.h"
/*
const char* ssid = "";
const char* password = "";
const char* mqtt_server = "";
const char* mqtt_client_id = "";
const char* mqtt_username = "";
const char* mqtt_password = "";
*/

const char logLevels [][LOG_PREFIX_LENGTH + 1] {
  "ERROR: ",
  "WARN:  ",
  "INFO:  ",
  "DEBUG: "
};

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

bool output_states[NUM_OUTPUTS] = {0};
const byte output_pins[NUM_OUTPUTS] = {
  CONTACTOR_FORWARD_PIN,
  CONTACTOR_REVERSE_PIN,
  RELAY_FAN_PIN,
  RELAY_SPARE_PIN
};

bool processingInstruction = false;

char * standardRecipe = "W10 F2 R1 W10";
char * overrideRecipe = "";
byte recipePosition = 0;

char currentInstruction = '0';
int currentValue = 0;

unsigned long lastWifiConnectionAttempt = 0;
unsigned long lastMQTTConnectionAttempt = 0;

unsigned long lastDebounceTime = 0;

void setup() {
  for (byte i = 0; i < NUM_OUTPUTS; i++) { //Reset all inputs to input, and  low
    pinMode(output_pins[i], OUTPUT);
    digitalWrite(output_pins[i], HIGH);
  }
  for (byte i = 0; i < NUM_INPUTS; i++) { //Reset all inputs to input, and  low
    pinMode(inputsPins[i], INPUT_PULLUP);
    inputsCurrentStates[i] = LOW;
    inputsDebounceStates[i] = LOW;
    inputsNewStates[i] = LOW;
    inputsDebounceTimes[i] = 0;
  }
  Serial.begin(9600);    // Initialize the Serial monitor for debugging
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
  if (sendMessage == "READ") {

  } else {
  }
  log_message(LOG_INFO, sendMessage);
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
  byte recipeLength = strlen(standardRecipe);
  log_message(LOG_DEBUG, "getNextInstruction: Recipelength: %d", recipeLength);
  for (; recipePosition < recipeLength; recipePosition++)
  {
    char nextChar = standardRecipe[recipePosition];
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

unsigned long rotation_start = 0;
byte rotation_rotations = 0;
bool rotation_induction_sensor_grace = true;

void rotate_drum() {
  bool direction_forward = currentInstruction == 'F';

  //Drum currently stopped, start rotation
  if (rotation_start == 0) {
    rotation_rotations = 0;
    log_message(LOG_INFO, "Rotating drum %c %d times ", currentInstruction, currentValue);
    rotation_induction_sensor_grace = true;
    if (inputsCurrentStates[INDUCTION_SENSOR_FEEDBACK]) {
      log_message(LOG_WARNING, "Drum not started in zero position, sensor false");
    }
    start_rotating_drum(direction_forward);
    rotation_start = now;
  }

  //To minimise max power, delay fan start a set time
  if (!output_states[RELAY_FAN] && now - rotation_start >= FAN_STARTUP_DELAY_MS) {
    set_output(RELAY_FAN, true);
  }

  //check if induction sensor grace period ended
  if (rotation_induction_sensor_grace && now - rotation_start >= INDUCTION_SENSOR_GRACE_TIME_MS) {
    rotation_induction_sensor_grace = false;
  }

  //Induction sensor triggered a full rotation
  if (!inputsCurrentStates[INDUCTION_SENSOR_FEEDBACK] && !rotation_induction_sensor_grace) {
    drum_rotation_full_turn();
  }

  //Fallback if rotation took longer than max time
  if ((now - rotation_start) / 1000 >= ROTATION_MAX_TIME) {
    log_message(LOG_ERROR, "Rotation %d took longer than %d seconds, sensor probably broken ", rotation_rotations, ROTATION_MAX_TIME);
    drum_rotation_full_turn();
  }

  //Rotation is finished, stop drum.
  if (rotation_rotations >= currentValue) {
    log_message(LOG_INFO, "Finished all %d rotations %s", currentValue, direction_forward ? "forward" : "reverse");
    stop_drum_immediately();
  }
}

void start_rotating_drum(bool direction_forward) {
  if (direction_forward) { //Rotate forward/clockwise
    if (output_states[CONTACTOR_REVERSE]) {
      log_message(LOG_ERROR, "Forward drum start requested, but drum already running in reverse. This is bad.");
      set_output(CONTACTOR_REVERSE, false);
      delay(5000);
    }
    set_output(CONTACTOR_FORWARD, true);
  }
  else { //Rotate reverse/counter clockwise
    if (output_states[CONTACTOR_FORWARD]) {
      log_message(LOG_ERROR, "Reverse drum start requested, but drum already running forward. This is bad.");
      set_output(CONTACTOR_FORWARD, false);
      delay(5000);
    }
    set_output(CONTACTOR_REVERSE, true);
  }
}

void set_output(byte output, bool state) {
  if (output >= NUM_OUTPUTS) {
    log_message(LOG_ERROR, "Output %d does not exist, max is %d", output, NUM_OUTPUTS);
    return;
  }
  output_states[output] = state;
  digitalWrite(output_pins[output], !state);
}

void drum_rotation_full_turn() {
  log_message(LOG_INFO, "Finished 1 of %d rotations %s in %d seconds", currentValue, currentInstruction == 'F' ? "forward" : "reverse", (now - rotation_start) / 1000);
  rotation_induction_sensor_grace = true;
  rotation_start = now;
  ++rotation_rotations;
}

void stop_drum_immediately() {
  set_output(CONTACTOR_FORWARD, false);
  set_output(CONTACTOR_REVERSE, false);
  log_message(LOG_DEBUG, "Stopping drum immediately");
  //Wait 1 second for full stopping before continuing
  rotation_start = 0;
  currentInstruction = 'W';
  currentValue = 1;
}

void stop_drum_graceful() {
  if (rotation_start == 0) {
    log_message(LOG_WARNING, "Graceful drum stop requested, but drum not running");
    return;
  }
  log_message(LOG_DEBUG, "Stopping drum gracefully");
  rotation_rotations = currentValue; //Override amount of rotations
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

void check_inputs() {
  // Check all inputs
  for (byte i = 0; i < NUM_INPUTS; i++) {
    inputsNewStates[i] = !digitalRead(inputsPins[i]);

    if (inputsNewStates[i] != inputsCurrentStates[i]) { //Only do stuff if there is new input

      if (inputsNewStates[i] != inputsDebounceStates[i]) {
        inputsDebounceStates[i] = inputsNewStates[i];
        inputsDebounceTimes[i] = now;
      }
      if ((now - inputsDebounceTimes[i]) > DEBOUNCE_TIME) {
        if (inputsCurrentStates[i] == LOW && inputsNewStates[i] == HIGH) { //Input i high
          log_message(LOG_DEBUG, "Input %d high", i);
          handle_high_input(i);
        }
        else if (inputsCurrentStates[i] == HIGH && inputsNewStates[i] == LOW) { //Input i low
          log_message(LOG_DEBUG, "Input %d low", i);
          handle_low_input(i);
        }
        inputsCurrentStates[i] = inputsNewStates[i];
      }
    }
    else
    {
      inputsDebounceStates[i] = inputsCurrentStates[i];
    }
  }
}

unsigned int last_serial_input = 0;
char serial_input[SERIAL_INPUT_LENGTH];
byte serial_input_position = 0;
void check_serial_input() {
  while (Serial.available()) {
    last_serial_input = now;
    serial_input[serial_input_position] = Serial.read();
    if (serial_input[serial_input_position] == '\n') {
      serial_input[serial_input_position] = '\0';
      manage_manual_input(serial_input);
      log_message(LOG_DEBUG, "Recieved serial input: %s", serial_input);
      serial_input_position = 0;
    } else {
      ++serial_input_position;
    }
  }
  if (serial_input_position > 0 && now - last_serial_input > SERIAL_INPUT_GRACE_TIME_MS) {
    serial_input[serial_input_position] = '\0';
    log_message(LOG_WARNING, "Serial input timed out without \n at: %s", serial_input);
    serial_input_position = 0;
  }
}

const byte component_length = 3;
// Mananges manual input from serial input or MQTT topic "command"
void manage_manual_input(char * input_buffer) {
  char * part;
  log_message(LOG_DEBUG, "Splitting input to parts: %s", input_buffer);
  part = strtok (input_buffer, " ");
  if (part == NULL) {
    log_message(LOG_WARNING, "Invalid command %s", input_buffer);
    return;
  }
  if (strcmp(part, "input") == 0) { //Manage input
    // Get value for command
    part = strtok (NULL, " ");
    if (part != NULL) {
      byte input = atoi(part);
      if (input >= NUM_INPUTS) { //Too large number
        log_message(LOG_WARNING, "Too large input %d, max is %d for command %s", input, NUM_INPUTS - 1, input_buffer);
        return;
      }
      log_message(LOG_WARNING, "Input %d = %d", input, inputsCurrentStates[input]);
    }
    else { //List inputs
      for (int i = 0; i < NUM_INPUTS; i++) {
        log_message(LOG_WARNING, "Input %d = %d", i, inputsCurrentStates[i]);
      }
    }
  }
  else if (strcmp(part, "output") == 0) { //Manage input
    // Get value for command
    part = strtok (NULL, " ");
    if (part != NULL) {
      byte output = atoi(part);
      if (output >= NUM_OUTPUTS) { //Too large number
        log_message(LOG_WARNING, "Too large output %d, max is %d for command %s", output, NUM_OUTPUTS - 1, input_buffer);
        return;
      }
      log_message(LOG_WARNING, "Output %d = %d", output, output_states[output]);
    }
    else { //List inputs
      for (int i = 0; i < NUM_OUTPUTS; i++) {
        log_message(LOG_WARNING, "Output %d = %d", i, output_states[i]);
      }
    }
  }
  else if (strcmp(part, "all") == 0) { //List all
    manage_manual_input("input");
    manage_manual_input("output");
  }
  else {
    log_message(LOG_WARNING, "Unknown command %s, full input is: %s", part, input_buffer);
  }
}

void handle_high_input(byte input) {
  switch (input) {
    case 0: //Manual reverse
      log_message(LOG_WARNING, "Manual reverse turn triggered.");
      overrideRecipe = "R1";
      stop_drum_immediately();
      //rotate_drum();
      break;
    case 1: //Manual forward
      log_message(LOG_WARNING, "Manual forward turn triggered.");
      overrideRecipe = "F1";
      stop_drum_immediately();
      break;
    case 5: //Motor protector feedback
      log_message(LOG_WARNING, "Motor protector reset (on again).");
      break;
    case 6: //Emergency loop feedback
      log_message(LOG_WARNING, "Emergency stop released, or hatches closed.");
      break;
  }
}
void handle_low_input(byte input) {
  switch (input) {
    case 5: //Motor protector feedback
      log_message(LOG_ERROR, "Motor protector triggered. Manual intervention required");
      stop_drum_immediately();
      break;
    case 6: //Emergency loop feedback
      log_message(LOG_ERROR, "Emergency stop pressed, or hatches lifted. Manual intervention required");
      stop_drum_immediately();
      break;
  }
}

void log_message(byte level, const char * message, ...) {
  if (level > LOG_LEVEL)
    return;
  char log_buffer[100];
  //strcpy(log_buffer, logLevels[level]);
  va_list args;
  va_start(args, message);
  vsprintf(log_buffer, message, args);
  va_end(args);

  // Prepend prefix
  memmove(log_buffer + LOG_PREFIX_LENGTH, log_buffer, strlen(log_buffer) + 1);
  memcpy(log_buffer, logLevels[level], LOG_PREFIX_LENGTH);

  Serial.println(log_buffer);
  if(client.connected()) {
    client.publish(MQTT_PREFIX MQTT_TOPIC_LOG, log_buffer);
  }

}

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
      WiFi.begin(ssid, password);
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
        if (client.connect(mqtt_client_id, mqtt_username, mqtt_password, MQTT_PREFIX MQTT_TOPIC_ONLINE, 1, true, "false")) {
          log_message(LOG_INFO, "Connected to MQTT broker");
          // Once connected, publish an announcement...
          client.publish(MQTT_PREFIX MQTT_TOPIC_STATUS , "Connected");
          client.publish(MQTT_PREFIX MQTT_TOPIC_OVERRIDE, " ");
          client.publish(MQTT_PREFIX MQTT_TOPIC_ONLINE, "true");
          // ... and resubscribe
          client.subscribe(MQTT_PREFIX MQTT_TOPIC_OVERRIDE);
          log_message(LOG_DEBUG, "MQTT connection took %d ms", millis()-lastMQTTConnectionAttempt);
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

void loop() {
  now = millis();
  manage_connection();
  check_inputs();
  check_serial_input();
  process_instructions();
}

#define SERIAL_INPUT_GRACE_TIME_MS 1000
#define SERIAL_INPUT_LENGTH 30

unsigned int last_serial_input = 0;
char serial_input[SERIAL_INPUT_LENGTH];
byte serial_input_position = 0;
void check_serial_input() {
  while (Serial.available()) {
    last_serial_input = now;
    serial_input[serial_input_position] = Serial.read();
    if (serial_input[serial_input_position] == '\n') {
      serial_input[serial_input_position] = '\0';
      manage_serial_input(serial_input);
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

// Mananges manual input from serial input
void manage_serial_input(char * input_buffer) {
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
    manage_serial_input("input");
    manage_serial_input("output");
  }
  else {
    log_message(LOG_WARNING, "Unknown command %s, full input is: %s", part, input_buffer);
  }
}

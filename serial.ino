
struct SerialInput {
  unsigned long last;
  byte position;
  char string[SERIAL_INPUT_LENGTH];
} serialInput = {
  .last = 0,
  .position = 0
};

void check_serial_input() {
  while (Serial.available()) {
    serialInput.last = now;
    serialInput.string[serialInput.position] = Serial.read();
    if (serialInput.string[serialInput.position] == '\n') {
      serialInput.string[serialInput.position] = '\0';
      manage_serial_input(serialInput.string);
      log_message(DEBUG, "Recieved serial input: %s", serialInput.string);
      serialInput.position = 0;
    } else {
      ++serialInput.position;
    }
  }
  if (serialInput.position > 0 && now - serialInput.last > timing.grace.serialInput) {
    serialInput.string[serialInput.position] = '\0';
    log_message(WARNING, "Serial input timed out without \n at: %s", serialInput.string);
    serialInput.position = 0;
  }
}

// Mananges manual input from serial input
void manage_serial_input(char * input_buffer) {
  char * part;
  log_message(DEBUG, "Splitting input to parts: %s", input_buffer);
  part = strtok (input_buffer, " ");
  if (part == NULL) {
    log_message(WARNING, "Invalid command %s", input_buffer);
    return;
  }
  if (strcmp(part, "input") == 0) { //Manage input
    // Get value for command
    part = strtok (NULL, " ");
    if (part != NULL) {
      byte input = atoi(part);
      if (input >= numInputs) { //Too large number
        log_message(WARNING, "Too large input %d, max is %d for command %s", input, numInputs - 1, input_buffer);
        return;
      }
      log_message(WARNING, "Input %d = %d", input, inputs[input].state.current);
    }
    else { //List inputs
      for (int i = 0; i < numInputs; i++) {
        log_message(WARNING, "Input %d = %d", i, inputs[i].state.current);
      }
    }
  }
  else if (strcmp(part, "output") == 0) { //Manage input
    // Get value for command
    part = strtok (NULL, " ");
    if (part != NULL) {
      byte output = atoi(part);
      if (output >= numOutputs) { //Too large number
        log_message(WARNING, "Too large output %d, max is %d for command %s", output, numOutputs - 1, input_buffer);
        return;
      }
      log_message(WARNING, "Output %d = %d", output, outputs[output].state);
    }
    else { //List inputs
      for (int i = 0; i < numOutputs; i++) {
        log_message(WARNING, "Output %d = %d", i, outputs[i].state);
      }
    }
  }
  else if (strcmp(part, "all") == 0) { //List all
    manage_serial_input("input");
    manage_serial_input("output");
  }
  else {
    log_message(WARNING, "Unknown command %s, full input is: %s", part, input_buffer);
  }
}

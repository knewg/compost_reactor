
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


void check_inputs() {
  // Check all inputs
  for (byte i = 0; i < numInputs; i++) {
    inputs[i].state.next = !digitalRead(inputs[i].pin);

    if (inputs[i].state.next != inputs[i].state.current) { //Only do stuff if there is new input

      if (inputs[i].state.next != inputs[i].state.debounce) {
        inputs[i].state.debounce = inputs[i].state.next;
        inputs[i].state.debounceTime = now;
      }
      if ((now - inputs[i].state.debounceTime) > timing.delay.debounce) {
        if (inputs[i].state.current == LOW && inputs[i].state.next == HIGH) { //Input i high
          log_message(DEBUG, "Input %d high", i);
          handle_high_input(i);
        }
        else if (inputs[i].state.current == HIGH && inputs[i].state.next == LOW) { //Input i low
          log_message(DEBUG, "Input %d low", i);
          handle_low_input(i);
        }
        inputs[i].state.current = inputs[i].state.next;
      }
    }
    else
    {
      inputs[i].state.debounce = inputs[i].state.current;
    }
  }
}

void handle_high_input(byte input) {
  switch (input) {
    case 0: //Manual reverse
      log_message(WARNING, "Manual reverse turn triggered.");
      overrideRecipe = "R1";
      stop_drum_immediately();
      //rotate_drum();
      break;
    case 1: //Manual forward
      log_message(WARNING, "Manual forward turn triggered.");
      overrideRecipe = "F1";
      stop_drum_immediately();
      break;
    case 5: //Motor protector feedback
      log_message(WARNING, "Motor protector reset (on again).");
      break;
    case 6: //Emergency loop feedback
      log_message(WARNING, "Emergency stop released, or hatches closed.");
      break;
  }
}
void handle_low_input(byte input) {
  switch (input) {
    case 5: //Motor protector feedback
      log_message(ERROR, "Motor protector triggered. Manual intervention required");
      stop_drum_immediately();
      break;
    case 6: //Emergency loop feedback
      log_message(ERROR, "Emergency stop pressed, or hatches lifted. Manual intervention required");
      stop_drum_immediately();
      break;
  }
}

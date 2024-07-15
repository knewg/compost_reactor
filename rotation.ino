unsigned long rotation_start = 0;
byte rotation_rotations = 0;
bool rotation_induction_sensor_grace = true;

void rotate_drum() {
  bool direction_forward = recipe.current.instruction == 'F';

  //Drum currently stopped, start rotation
  if (rotation_start == 0) {
    rotation_rotations = 0;
    log_message(INFO, "Rotating drum %c %d times ", recipe.current.instruction, recipe.current.value);
    rotation_induction_sensor_grace = true;
    if (inputs[INDUCTION_SENSOR_FEEDBACK].state.current) {
      log_message(WARNING, "Drum not started in zero position, sensor false");
    }
    start_rotating_drum(direction_forward);
    rotation_start = now;
  }

  //To minimise max power, delay fan start a set time
  if (!outputs[RELAY_FAN].state && now - rotation_start >= timing.delay.fanStartup) {
    set_output(RELAY_FAN, true);
  }

  //check if induction sensor grace period ended
  if (rotation_induction_sensor_grace && now - rotation_start >= timing.grace.inductionSensor) {
    rotation_induction_sensor_grace = false;
  }

  //Induction sensor triggered a full rotation
  if (!inputs[INDUCTION_SENSOR_FEEDBACK].state.current && !rotation_induction_sensor_grace) {
    drum_rotation_full_turn();
  }

  //Fallback if rotation took longer than max time
  if (now - rotation_start >= timing.max.rotation) {
    log_message(ERROR, "Rotation %d took longer than %u seconds, sensor probably broken ", rotation_rotations, timing.max.rotation);
    drum_rotation_full_turn();
  }

  //Rotation is finished, stop drum.
  if (rotation_rotations >= recipe.current.value) {
    log_message(INFO, "Finished all %d rotations %s", recipe.current.value, direction_forward ? "forward" : "reverse");
    stop_drum_immediately();
  }
}

void start_rotating_drum(bool direction_forward) {
  if (direction_forward) { //Rotate forward/clockwise
    if (outputs[CONTACTOR_REVERSE].state) {
      log_message(ERROR, "Forward drum start requested, but drum already running in reverse. This is bad.");
      set_output(CONTACTOR_REVERSE, false);
      delay(5000);
    }
    set_output(CONTACTOR_FORWARD, true);
  }
  else { //Rotate reverse/counter clockwise
    if (outputs[CONTACTOR_FORWARD].state) {
      log_message(ERROR, "Reverse drum start requested, but drum already running forward. This is bad.");
      set_output(CONTACTOR_FORWARD, false);
      delay(5000);
    }
    set_output(CONTACTOR_REVERSE, true);
  }
}


void drum_rotation_full_turn() {
  log_message(INFO, "Finished 1 of %d rotations %s in %d seconds", recipe.current.value, recipe.current.instruction == 'F' ? "forward" : "reverse", now - rotation_start);
  rotation_induction_sensor_grace = true;
  rotation_start = now;
  ++rotation_rotations;
}

void stop_drum_immediately() {
  set_output(CONTACTOR_FORWARD, false);
  set_output(CONTACTOR_REVERSE, false);
  log_message(DEBUG, "Stopping drum immediately");
  //Wait 1 second for full stopping before continuing
  rotation_start = 0;
  recipe.current.instruction = 'W';
  recipe.current.value = 1;
}

void stop_drum_graceful() {
  if (rotation_start == 0) {
    log_message(WARNING, "Graceful drum stop requested, but drum not running");
    return;
  }
  log_message(DEBUG, "Stopping drum gracefully");
  rotation_rotations = recipe.current.value; //Override amount of rotations
}

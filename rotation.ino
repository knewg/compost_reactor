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

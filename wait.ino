struct Wait {
  unsigned long start;
  unsigned int timeout;
} wait = {
  .start = 0
};

void run_wait_timer() {
  if (wait.start == 0) { //Timer stopped
    log_message(INFO, "Waiting for %d seconds", recipe.current.value);
    wait.start = timing.now;
    wait.timeout = recipe.current.value * 1000;
    if(wait.timeout > timing.min.wait.fanTurnOff) { //Longer than 5 seconds wait, turn off fan
      log_message(DEBUG, "Timeout > %d turning off fan", timing.min.wait.fanTurnOff);
      set_output(RELAY_FAN, false);
    }
    mqttUpdateStatus("waiting");
    return;
  }
  if (timing.now - wait.start >= wait.timeout) {
    log_message(INFO, "%d second wait finished", recipe.current.value);
    wait.start = 0;
    recipe.current.instruction = '0';
  }
}

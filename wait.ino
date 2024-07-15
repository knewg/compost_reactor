struct Wait {
  unsigned long start;
} wait = {
  .start = 0
};

void run_wait_timer() {
  if (wait.start == 0) { //Timer stopped
    log_message(INFO, "Waiting for %d seconds", recipe.current.value);
    wait.start = timing.now;
    if(recipe.current.value > timing.min.wait.fanTurnOff) { //Longer than 5 seconds wait, turn off fan
      log_message(DEBUG, "Timeout > %d turning off fan", timing.min.wait.fanTurnOff);
      set_output(RELAY_FAN, false);
    }
    return;
  }
  if (timing.now - wait.start >= recipe.current.value) {
    log_message(INFO, "%d second wait finished", recipe.current.value);
    wait.start = 0;
    recipe.current.instruction = '0';
  }
}

void set_output(byte output, bool state) {
  if (output >= NUM_OUTPUTS) {
    log_message(LOG_ERROR, "Output %d does not exist, max is %d", output, NUM_OUTPUTS);
    return;
  }
  output_states[output] = state;
  digitalWrite(output_pins[output], !state);
}

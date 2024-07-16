void set_output(byte output, bool state) {
  if (output >= numOutputs) {
    log_message(ERROR, "Output %d does not exist, max is %d", output, numOutputs);
    return;
  }
  outputs[output].state = state;
  digitalWrite(outputs[output].pin, !state);
  publishMqttOutputState(output);
}

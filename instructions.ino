int charToInt(char character) {
  int returnValue = character - '0';
  if (returnValue > 9 || returnValue < 0) {
    log_message(ERROR, "Int conversion error, %c is not an int", character);
    return 0;
  }
  return returnValue;
}

void getNextInstruction() {
  log_message(DEBUG, "getNextInstruction: Loading new instruction");
  if(strcmp(recipe.oneShot, "") == 0) { //No one shot recipe use rolling
    log_message(DEBUG, "Using rolling recipe");
    if(recipe.current.recipe != &recipe.rolling[0]) {
      log_message(DEBUG, "First time returning to rolling recipe");
      recipe.current.recipe = recipe.rolling;
      recipe.current.position = 0;
    }
  } else {
    log_message(DEBUG, "Using oneShot recipe");
    if(recipe.current.recipe != &recipe.oneShot[0]) {
      log_message(DEBUG, "First time running oneShot recipe");
      recipe.current.recipe = recipe.oneShot;
      recipe.current.position = 0;
    }
  }
  recipe.current.instruction = '0';
  recipe.current.value = 0;
  bool instructionFound = false;
  bool valueFound = false;
  byte recipeLength = strlen(recipe.current.recipe);
  log_message(DEBUG, "getNextInstruction: Recipelength: %d", recipeLength);
  for (; recipe.current.position < recipeLength; recipe.current.position++)
  {
    char nextChar = recipe.current.recipe[recipe.current.position];
    log_message(DEBUG, "getNextInstruction: nextChar %c", nextChar);
    if (nextChar == ' ' || nextChar == 'R' || nextChar == 'F' || nextChar == 'W') {
      if (instructionFound) { //Instruction already found, recipe read
        if (valueFound) {
          return; //Return successfully
        }
        // No value, fail
        log_message(ERROR, "getNextInstruction: Recipe error, no value after instruction");
        recipe.current.instruction = '0';
        recipe.current.value = 0;
      }
      if(nextChar != ' ') { // Allow (optional) whitespace in recipe
        recipe.current.instruction = nextChar;
        instructionFound = true;
      }
    } else { //Expect value
      if (!instructionFound) {
        log_message(ERROR, "getNextInstruction: Recipe error, value before instruction");
        recipe.current.instruction = '0';
        recipe.current.value = 0;
        return;
      }
      if (valueFound) { //Multiply original value with 10
        recipe.current.value = recipe.current.value * 10;
      } else {
        valueFound = true;
      }
      recipe.current.value = recipe.current.value + charToInt(nextChar);
    }
  }
  recipe.current.position = 0;
  if(recipe.current.recipe == &recipe.oneShot[0]) { //One shot recipe finished, remove it
    log_message(DEBUG, "One shot recipe run finished, removing");
    strcpy(recipe.oneShot, "");
    mqttClearOneShotRecipe();
  }
  // Reached the end, ensure we got an instruction
  if (instructionFound) { //Instruction already found, recipe read
    if (valueFound) {
      log_message(DEBUG, "getNextInstruction: End of recipe");
      return; //Return successfully
    }
    // No value, fail
    log_message(ERROR, "getNextInstruction: Recipe error, recipe ended without value");
    recipe.current.instruction = '0';
    recipe.current.value = 0;
  }
  log_message(ERROR, "getNextInstruction: Recipe error, recipe ended without instruction");
  recipe.current.instruction = '0';
  recipe.current.value = 0;
}

void process_instructions() {
  bool newInstruction = false;
  if (recipe.current.instruction == '0') {
    getNextInstruction();
    newInstruction = true;
  }
  if (recipe.current.instruction == 'R' || recipe.current.instruction == 'F') { // Rotate drum
    rotate_drum();
  }
  else if (recipe.current.instruction == 'W') { // Wait
    run_wait_timer();
  }
}


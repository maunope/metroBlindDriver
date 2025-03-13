
#include <LowPower.h>                //https://github.com/rocketscream/Low-Power
#include <EEPROM.h>



//#define DEBUG_MODE

struct ProgramModeStatus
{
  //current program mode position index to be displayed
  int programPos = -1;
  //last succesfully displayed destination
  int lastReachedProgramPos = -1;
  //last recorded movement millis
  unsigned long lastMovementMillis = 0;
  byte stops[40] = {};
  byte stopsLength = 0;
  int stepsSeconds = -1;
};


struct ManualModeStatus
{
  //manual mode status variables
  int lastReachedManualStop = -1;
};

struct Signals
{
  //bit string currently set on output pins
  byte bitString = 0b00000000;
  //manual/program mode
  int mode = -1;
  //motion enabled status,
  bool motionEnabled = false;
};

struct EEpromData
{
  int mode = -1;
  byte programStops[40];
  byte programLength = 0;
  int programStepsSeconds = -1;
};



//debug mode redirects stdout to the serial port, this avoids the need for strings buffering
#ifdef DEBUG_MODE
// Define a custom putchar function to redirect output to Serial
int putCharToSerial(char c, FILE *stream) {
  if (stream == stdout) { // Check if it's standard output
    Serial.write(c);
  }
  return 0;
}

// Declare a FILE stream for stdout (no initializer here!)
FILE customStdOut;
//printf macro, together with putCharToSerial driver debug messages straight to the Serial port,
//stores string in Program Memory and appends a new line to each log line automatically
#define DEBUG_PRINTF(format, ...) printf_P(PSTR(format "\n"),##__VA_ARGS__);
#endif

#ifndef DEBUG_MODE
#define DEBUG_PRINTF(format, ...) // Nothing 
#endif

//automatic program based movement or manual stop selection
const int MANUAL_MODE = 0;
const int PROGRAM_MODE = 1;


//8 bits strings corresponding to each Roma Metropolitana A blind position
//rightmost bit is not relevant as the corresponding pin on the blind bit strings roller is not connected
//sorting mirrors the punch hole roller's, first element selection ("Fuori Servizio") is arbitrary
//first (0000000) and last (1111111) strings are "set all pins LOW/HIGH" commands, which have no correspondance on the roller

const byte COMMANDS[]  = {
  0b00000000,  // 00 (Stop the blind, wherever it is)
  0b00110100,  // 01 Fuori servizio 
  0b10001100, 0b01001100, 0b00101100, 0b00011100, 0b11000010, 0b10100010, 0b01100010, 0b10010010, //02-26 blank  space
  0b01010010, 0b00110010, 0b10001010, 0b01001010, 0b00101010, 0b00011010, 0b10000110, 0b01000110,
  0b00100110, 0b00010110, 0b00001110, 0b11000000, 0b10100000, 0b01100000, 0b10010000, 0b01010000,
  0b11100000,
  0b11010000,  // 27 Anagnina
  0b10110000,
  0b01110000,  // 29 Cinecittà
  0b11001000,
  0b10101000,  // 31 Arco di Travertino 
  0b01101000,
  0b10011000,  // 33 San giovanni 
  0b01011000,
  0b00111000,  // 35 Termini 
  0b11000100,
  0b10100100,  // 37 Lepanto 
  0b01100100,
  0b10010100,  // 39 Ottaviano 
  0b01010100,
  0b11111111   // 41 (Run the blind, wherever it is)};
};
//the blind has 40 stops, and this is going to change :-)
const int COMMANDSLEN = 41;

//map pins to bit string positions, 0 ot 6
//const int PIN_MAP[] = { 3, 4, 5, 6, 7, 8, 9 };
const int PIN_MAP[] = { 9, 8, 7, 6, 5, 4, 3 };

//indexes of Roma Metropolitana A stops
const int STOP = 0;
const int FUORISERVIZIO = 1;
const int ANAGNINA = 27;
const int CINECITTA = 29;
const int ARCODITRAVERTINO = 31;
const int SANGIOVANNI = 33;
const int TERMINI = 35;
const int LEPANTO = 37;
const int OTTAVIANO = 39;
const int RUN = 41;

//10 seconds are approx. 2.5 full revolutions
const int MAX_CONTINUOS_RUN_SECS = 10;

//start in manual mode by default, save eeprom conf to start program mode at startup
const int DEFAULT_MODE = MANUAL_MODE;

//seconds between each autonomous blind step
const int DEFAULT_SECONDS_BETWEEN_STEPS = 3600;

const long int SECONDS_PER_DAY = 86400;


const int RUNNING_PIN = 0;  //Reads high when the blind is rotating
const int PUSH_BUTTON_PIN = 1;
const int ENABLE_PIN = 10;  //Set to HIGH to enable the output relais coil
const int FEEDBACK_LED_PIN = 14;

//output signal status and current mode stored here, at all times this
// variable represents the desired status configured on output pins
Signals signals;

//status for program and manual mode, i.e. "reached stop X", "last movement recorded on Y"
ProgramModeStatus programModeStatus;
ManualModeStatus manualModeStatus;
unsigned long movementStartedMillis = 0;

unsigned long lastCommandReceived = 0;

//this default program cycles stops from Anagnina to Fuori servizio
const byte  DEFAULT_PROGRAM[40] = {27, 29, 31, 33, 35, 37, 39,  01};
byte DEFAULT_PROGRAM_LENGTH = 8;

// how long the push button has been pressed
unsigned long pushButtonPressedMillis = 0;

EEpromData eepromData;

bool forceMoveToNextProgramStop = false;

//resets default configuration and stops the blind,  doesn't persist data to eeprom
void resetDefaults(Signals& signals, ProgramModeStatus& programModeStatus, ManualModeStatus& manualModeStatus)
{
  stopBlind(signals);
  setMode(DEFAULT_MODE, signals, programModeStatus, manualModeStatus);
  memcpy(programModeStatus.stops, DEFAULT_PROGRAM, sizeof(DEFAULT_PROGRAM));
  programModeStatus.stopsLength = DEFAULT_PROGRAM_LENGTH;
  programModeStatus.stepsSeconds  = DEFAULT_SECONDS_BETWEEN_STEPS;
}

//writes current conf to eeprom, no validation
int writeConfToEEprom(Signals signals, ProgramModeStatus programModeStatus)
{
  EEpromData eepromData;
  eepromData.mode = signals.mode;
  memcpy(eepromData.programStops, programModeStatus.stops, sizeof(programModeStatus.stops));
  eepromData.programLength = programModeStatus.stopsLength;
  eepromData.programStepsSeconds = programModeStatus.stepsSeconds;
  EEPROM.put(0, eepromData);
}

//loads configuration from eeprom  (doesn't apply it though!), return 0 on success, -1 on faulty data
int loadConfFromEEprom(EEpromData& eepromData)
{
  int exitStatus = 0;
  EEPROM.get(0, eepromData);
  if (eepromData.mode != MANUAL_MODE && eepromData.mode != PROGRAM_MODE) {
    DEBUG_PRINTF("Corrupted EEProm data, invalid mode, reverting to default mode");
    eepromData.mode = DEFAULT_MODE;
    exitStatus = -1;
  }
  if (eepromData.programStepsSeconds < 0) {
    DEBUG_PRINTF("Corrupted EEProm data, invalid program mode interval, reverting to default value");
    eepromData.programStepsSeconds = DEFAULT_SECONDS_BETWEEN_STEPS;
    exitStatus = -1;
  }
  if (eepromData.programLength > 40) {
    DEBUG_PRINTF("Corrupted EEProm data, invalid program length, reverting to 0");
    eepromData.programLength = 0;
    exitStatus = -1;
  }
  for (int i = 0; i < eepromData.programLength; i++) {
    if (eepromData.programStops[i] < 0 || eepromData.programStops[i] > 40) {
      DEBUG_PRINTF("Corrupted EEProm data, invalid sto selection in program, reverting to blank program");
      exitStatus = -1;
    }
  }
  return exitStatus;
}

// blinks the feedback led alternating given on/off time, for the given number
// of times.
void blinkFeedbackLed(int onMillis, int offMillis, int iterations) {
  int previousState = digitalRead(FEEDBACK_LED_PIN);
  for (int i = 0; i < iterations; i++) {
    digitalWrite(FEEDBACK_LED_PIN, HIGH);
    delay(onMillis);
    digitalWrite(FEEDBACK_LED_PIN, LOW);
    delay(offMillis);
  }
  digitalWrite(FEEDBACK_LED_PIN, previousState);
}


//writes the 8 digits padded BIN  representation of a byte in a given 9bytes buffer and returns its pointer for your convenience :-)
char* getPaddedBin(byte bitString, char buf[]) {
  for (int i = 7; i >= 0; i--) {
    buf[7 - i] = (bitString & (1 << i)) ? '1' : '0';
  }
  buf[8] = '\0';
  return buf;
}

//returns its index in commands array if a given bitString exists on the control signals roller, or -1 if it doesnt
int positionExists(byte bitString)
{

  for (int i = 0; i < COMMANDSLEN; i++) {
    if (COMMANDS[i] == bitString) {
      return i;
    }
  }
  return -1;
}


//outputs a bitString to blind control signal pins and saves it as the current one
//this method won't reset pins once the requested position is reached
//this method doesn't check the existance of the requested newBitString
void setStopSelector(byte newBitString, Signals& signals) {
  //note that hte least significant bit is never used, as there's no pin using it
  if (newBitString != signals.bitString) {
    for (int i = 7; i >= 1; i--) {
      digitalWrite(PIN_MAP[7 - i], ((newBitString >> (i)) & 1) == 1 ? HIGH : LOW);
    }
    signals.bitString = newBitString;
  }
}



//sends a stop command to the blind, regardless of its position
void stopBlind(Signals& signals) {
  setStopSelector(COMMANDS[STOP], signals);
}


//sends a run command to the blind, regardless of its position,
//motion will continue until a different command is sent or enable pin is set to low
void runBlind(Signals& signals) {
  setStopSelector(COMMANDS[RUN], signals);
}


//enables/disables blind movement
bool setMotionEnabled(bool enable, Signals& signals) {
  digitalWrite(ENABLE_PIN, enable ? HIGH : LOW);
  signals.motionEnabled = enable;
}

//reads the current blind position,disabling motion temporarily
//this method doesn't check if the blind is rolling before stopping it
//this method doesn't check if the returned position exists in the roller
//this method restores the pre-existing blind status (selected stop and enable status) before returning
byte readBlindPosition(Signals& signals) {

  char binBufA[9];
  char binBufB[9];
  byte savedBitString = signals.bitString;
  bool savedMotionEnabled = signals.motionEnabled;

  //temporarily disbale motion, to ensure a consistent reading and avoid unwanted movement
  setMotionEnabled(false, signals);

  byte currentPosition = 0;
  stopBlind(signals);
  //only 7 as we're cycling on pins, but then we need to shift 8
  for (int i = 0; i < 7; i++) {
    digitalWrite(PIN_MAP[i], HIGH);
    delay(5);
    //inverts (HIGH outputs 0, LOW outpus 1), for easier readability/debugging this method represents stops bit strings as the corresponding selector bit string
    //Actually, if the selected bitString is 11001100, motion stops on 00110011 configurationon of the blind roller
    //(motion stop when bitwise AND between selected string and rolles string is 0)
    currentPosition |= ((digitalRead(RUNNING_PIN) == HIGH ? 0 : 1) << (7 - i));
    digitalWrite(PIN_MAP[i], LOW);
    delay(5);
  }
  //reset previous state
  setStopSelector(savedBitString, signals);
  setMotionEnabled(savedMotionEnabled, signals);
  //DEBUG_PRINTF("readBlindPosition returning: %s", getPaddedBin(currentPosition, binBuf));
  return currentPosition;
}

//parse incoming serial commands and applies requested changes
void parseSerialCommands(char command[], Signals& signals, ProgramModeStatus& programModeStatus, ManualModeStatus& manualModeStatus) {
  char binBufA[9];
  char binBufB[9];
  if (strncmp(command, ">>GO", 4) == 0 && isdigit(command[4]) && isdigit(command[5]) && strlen(command) == 6) {
    char digits[2] = " ";
    digits[0] = command[4];
    digits[1] = command[5];
    int selectedStopIndex = atoi(digits);
    //skip 0 and COMMANDSLEN as they are STOP and RUN
    if (selectedStopIndex > 0 and selectedStopIndex < COMMANDSLEN - 1) {
      setStopSelector(COMMANDS[selectedStopIndex], signals);
      Serial.println(F("Selected stop # "));
      Serial.print(command);
    } else {
      Serial.println("Invalid stop index received");
    }
  }
  else if (strcmp(command, "<<MILLIS") == 0) {
    Serial.println(millis());
  }
  else if (strcmp(command, (">>STOP")) == 0) {
    stopBlind(signals);
    Serial.println(F("Blind stop command received"));
  } else if (strcmp(command, (">>RUN")) == 0) {
    runBlind(signals);
    Serial.println("Blind run command received");
  } else if (strcmp(command, (">>FUORISERVIZIO")) == 0) {
    setStopSelector(COMMANDS[FUORISERVIZIO], signals);
  } else if (strcmp(command, (">>ANAGNINA")) == 0) {
    setStopSelector(COMMANDS[ANAGNINA], signals);
  } else if (strcmp(command, (">>CINECITTA")) == 0) {
    setStopSelector(COMMANDS[CINECITTA], signals);
  } else if (strcmp(command, (">>ARCODITRAVERTINO")) == 0) {
    setStopSelector(COMMANDS[ARCODITRAVERTINO], signals);
  } else if (strcmp(command, (">>SANGIOVANNI")) == 0) {
    setStopSelector(COMMANDS[SANGIOVANNI], signals);
  } else if (strcmp(command, (">>TERMINI")) == 0) {
    setStopSelector(COMMANDS[TERMINI], signals);
  } else if (strcmp(command, (">>LEPANTO")) == 0) {
    setStopSelector(COMMANDS[LEPANTO], signals);
  } else if (strcmp(command, (">>OTTAVIANO")) == 0) {
    setStopSelector(COMMANDS[OTTAVIANO], signals);
  }
  else if (strncmp(command, (">>PROGRAMSTEPSSECONDS"), 21) == 0) {
    char digits[4];
    for (int i = 0; i < 4; i++) {
      digits[i] = command[21 + i];
    }
    if (!isdigit(digits[0]) || !isdigit(digits[1]) || !isdigit(digits[2]) || !isdigit(digits[3]) ) {
      Serial.println(F("Wrong format received, 0 padded 4 digits int required"));
      return;
    }
    programModeStatus.stepsSeconds = atoi(digits);
    Serial.println(F("Program mode steps seconds set"));
  }
  else if (strcmp(command, ("<<PROGRAMSTEPSSECONDS")) == 0) {
    Serial.println(programModeStatus.stepsSeconds);
  }
  else if (strcmp(command, ">>DEFAULTPROGRAM") == 0) {
    memcpy(programModeStatus.stops, DEFAULT_PROGRAM, sizeof(DEFAULT_PROGRAM));
    programModeStatus.stopsLength = DEFAULT_PROGRAM_LENGTH;
    //DEBUG_PRINTF("First 3 stops: memory %d %d %d %d ", programModeStatus.stops[0], programModeStatus.stops[1], programModeStatus.stops[2], programModeStatus.stopsLength );
    Serial.println(F("Default auto mode stops program set, run  >>EEPROMDATA to persist"));
  }
  else if (strncmp(command, ">>PROGRAMSTOPS", 14) == 0) {
    //94 == 14 chars form >>PROGRAMSTOPS plus 80 for the max length of the program
    byte newProgram[40];
    int j = 0;
    int programStop = -1;
    //stop on a 00 position, or at the end of the input string, or at the end of the program array
    for (int i = 14; i < strlen(command) && programStop != 0 && j < (sizeof(newProgram) / sizeof(newProgram[0])); i += 2) {
      char digits[2] = " ";
      digits[0] = command[i];
      digits[1] = command[i + 1];
      if (!isdigit(digits[0]) || !isdigit(digits[1])) {
        Serial.println(F("Wrong format received, only up to 40 2 digits zero padded integers admitted"));
        return;
      }
      int programStop = atoi(digits);
      DEBUG_PRINTF("Program stop digit received: %d", programStop);
      if (programStop > 0) {
        newProgram[j] = programStop;
        j++;
      }
    }
    //check that the sequence is unidirectional, it is possible to circle around last element once, i.e
    // 35,37,39,01,03 is OK, sequence drops once as it circles back from last element
    // 37,39,29,01,03 is KO, sequence reaches 39, then circles all the way to 29, then again all the way to 1
    // non unidirectional sequences break recovery of the closest reached stop, plus they cause excessive flapping on the blind
    bool smallerFound = false;
    for (int i = 1; i < j; i++) {
      if (newProgram[i] < newProgram[i - 1]) {
        if (smallerFound == true) {
          Serial.println(F("ERROR, stops sequence isn't unidirectional"));
          return;
        }
        smallerFound = true;
      }
    }
    //sequence is valid, proceed
    memcpy(programModeStatus.stops, newProgram, sizeof(newProgram));
    programModeStatus.stopsLength = j;
    Serial.println(F("Custom auto mode stops program set, run  >>EEPROMDATA to persist"));
  }
  else if (strcmp(command, "<<PROGRAM") == 0) {
    Serial.println(F("Current program in ram:"));
    //DEBUG_PRINTF("First 3 stops: memory %d %d %d %d ", programModeStatus.stops[0], programModeStatus.stops[1], programModeStatus.stops[2], programModeStatus.stopsLength );
    for (int i = 0; i < programModeStatus.stopsLength; i++) {
      Serial.print(" ");
      Serial.print(programModeStatus.stops[i]);
    }
    Serial.println(" ");
  }
  else if (strcmp(command, ">>PROGRAMMODE") == 0) {
    setMode(PROGRAM_MODE, signals, programModeStatus, manualModeStatus);
    Serial.println(F("Current auto mode stops program"));
  }
  else if (strcmp(command, ">>MANUALMODE") == 0) {
    setMode(MANUAL_MODE, signals, programModeStatus, manualModeStatus);
    Serial.println("Manual blind movement selected");
  } else if (strcmp(command, ">>RESETDEFAULTS") == 0) {
    resetDefaults(signals, programModeStatus, manualModeStatus);
    Serial.println("Loaded default configuration, run >>EEPROMDATA to persist");
  } else if (strcmp(command, "<<POSITION") == 0) {
    Serial.println(getPaddedBin(readBlindPosition(signals), binBufA));
  } else if (strcmp(command, "<<EEPROMDATA") == 0) {
    EEpromData eepromData;
    loadConfFromEEprom(eepromData);
    Serial.println(F("Program stops in eeprom: "));
    //DEBUG_PRINTF("First 3 stops: memory %d %d %d %d ", programModeStatus.stops[0], programModeStatus.stops[1], programModeStatus.stops[2], programModeStatus.stopsLength );
    for (int i = 0; i < eepromData.programLength; i++) {
      Serial.print(eepromData.programStops[i]);
      Serial.print(" ");
    }
    Serial.println(" ");
    Serial.println(F("Program Length,Program stesps seconds, mode  in eeprom: "));
    Serial.print(eepromData.programLength);
    Serial.print(" ");
    Serial.print(eepromData.programStepsSeconds);
    Serial.print(" ");
    Serial.print(eepromData.mode);
    Serial.println(" ");
  } else if (strcmp(command, ">>EEPROMDATA") == 0) {
    writeConfToEEprom(signals, programModeStatus);
    Serial.println(F("Saved current configuration to EEPROM"));
  } else if (strcmp(command, "<<COMPILEDATETIME") == 0) {
    char buff[20];
    sprintf(buff, "%s %s", __DATE__, __TIME__);
    Serial.println(buff);
  } else if (strcmp(command, "") != 0) {
    Serial.println(F("Unknown command: "));
    Serial.print(command);
  }
}

//gets the index of the position in the program next to the current one, i.e. blind is at 10th,
// program is 1,3,5,7,9,11,13 4 is returned, as the first program position >10 is 11, fifth element.
//sets output to  -1 on error, returns 0 (first position) as a default
void moveProgramToClosestStop(ProgramModeStatus& programModeStatus, Signals& signals) {

  char binBufA[9];
  char binBufB[9];
  byte currentPosition = readBlindPosition(signals);
  int currentPositionIndex = -1;
  for (int i = 0; i < COMMANDSLEN; i++) {
    DEBUG_PRINTF("moveProgramToClosestStop COMMANDS[i]=%s currentPosition=%s", getPaddedBin(COMMANDS[i], binBufA), getPaddedBin(currentPosition, binBufB));
    if (COMMANDS[i] == currentPosition) {
      currentPositionIndex = i;
      break;
    }
  }
  if (currentPositionIndex == -1) {
    DEBUG_PRINTF("moveProgramToClosestStop Error, non existent position detected, currentPosition=%s, bitstring=%s", getPaddedBin(currentPosition, binBufA), getPaddedBin(signals.bitString, binBufB));
    programModeStatus.programPos = -1;
  }
  bool done = false;
  for (int i = 0; i < programModeStatus.stopsLength; i++) {
    // DEBUG_PRINTF("%d %d", programModeStatus.stops[i], currentPositionIndex);
    if (programModeStatus.stops[i] > currentPositionIndex) {
      //identified the position in the program next to the current one
      done = true;
    }
    if (i > 0 && programModeStatus.stops[i] < programModeStatus.stops[i - 1]) {
      // if the current stop in the program index is < than the previous, the sequence went above 40 and started from 01 again,
      //assuming the sequence is "one way" (checks on serial commands avoid going backwards), this is the next stop
      done = true;
    }
    if (done) {
      programModeStatus.programPos = i;
      DEBUG_PRINTF("moveProgramToClosestStop returning %d", programModeStatus.programPos);
      return;
    }
  }
  //got to the bottom of the array, let's pick the first
  programModeStatus.programPos = 0;
  DEBUG_PRINTF("moveProgramToClosestStop returning (not found) %d", programModeStatus.programPos);
}

//sets the current motion mode, either program or manual, stops motion if required,
//this method resets program position to -1, recalculation is required
void setMode(int newMode, Signals& signals, ProgramModeStatus& programModeStatus, ManualModeStatus& manualModeStatus) {
  if (signals.mode == newMode) {
    //nothing to do
    DEBUG_PRINTF("setMode nothing to do");
    return;
  }
  if (newMode != MANUAL_MODE && newMode != PROGRAM_MODE) {
    DEBUG_PRINTF("setMode: invalid mode requested, received %i", newMode);
  }
  signals.mode = newMode;
  stopBlind(signals);
  if (newMode == MANUAL_MODE) {
    programModeStatus.programPos = -1;
    manualModeStatus.lastReachedManualStop = readBlindPosition(signals);
    digitalWrite(FEEDBACK_LED_PIN, HIGH);
  } else if (newMode == PROGRAM_MODE) {
    //todo recover position
    moveProgramToClosestStop(programModeStatus, signals);
    digitalWrite(FEEDBACK_LED_PIN, LOW);
  }
}


void setup() {

  Serial.begin(19200);

#ifdef DEBUG_MODE
  //if in debug mode, redirect stdout to serial
  fdev_setup_stream(&customStdOut, putCharToSerial, NULL, _FDEV_SETUP_WRITE);
  stdout = &customStdOut; // Redirect stdout to our stream
#endif

  //7 pins controlling the blind
  for (int i = 0; i < 7; i++) {
    pinMode(PIN_MAP[i], OUTPUT);
  }

  pinMode(RUNNING_PIN, INPUT);
  pinMode(PUSH_BUTTON_PIN, INPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(FEEDBACK_LED_PIN, OUTPUT);

  //sets selector pins to 0 and disables movement
  stopBlind(signals);
  setMotionEnabled(false, signals);


  EEpromData eepromBuf;
  if (loadConfFromEEprom(eepromBuf) != 0) {
    blinkFeedbackLed(50, 50, 50);
    resetDefaults(signals, programModeStatus, manualModeStatus);
    DEBUG_PRINTF("Invalid EEPROM conf found, Saving default conf to EEPROM");
    writeConfToEEprom(signals, programModeStatus);
  }
  else
  {
    memcpy(programModeStatus.stops, eepromBuf.programStops, sizeof(eepromBuf.programStops));
    programModeStatus.stopsLength = eepromBuf.programLength;
    programModeStatus.stepsSeconds = eepromBuf.programStepsSeconds;
    setMode(eepromBuf.mode, signals, programModeStatus, manualModeStatus);
  }



}

void loop() {
  delay(50);
  char binBufA[9];
  char binBufB[9];

  // self reset every week, just in case I f*cked up and some variable would
  // overflow left unchecked (i.e. millis)
  if (millis() / 1000 >= SECONDS_PER_DAY * 7) {
    asm volatile("  jmp 0");
  }

  if (Serial.available() > 0) {
    char command[128];
    int i = 0;
    while (Serial.available() > 0 && i < 127) {
      char c = Serial.read();
      if (c == '\n') {
        break;  // Break when newline character is received
      }
      command[i] = c;
      i++;
    }
    command[i] = '\0';
    parseSerialCommands(command, signals, programModeStatus, manualModeStatus);
    lastCommandReceived = millis();
  }

  //current blind position, unknown until read from the roller
  byte currentBlindPosition = 0;

  ///
  /// Program mode:
  /// - Check what the next step is
  //  - if last reached position is behind the current selected and the delay between steps has elapsed, Enable motion to the desired stop
  //  - (Loop repeats until rotation stops, signals generation are reiterated on each loop)
  //  - rotation is stopped, check if a valid position has been reached, and move on to the next step in the program
  //  - sleep (TODO make a non blockng one)
  if (signals.mode == PROGRAM_MODE) {

    if (programModeStatus.programPos == -1) {
      //at boot we need to recover what the next position is
      moveProgramToClosestStop(programModeStatus, signals);
      DEBUG_PRINTF("Recovering next program position: %d", programModeStatus.programPos);
    }

    if (programModeStatus.programPos >= 0 ) {

      //program mode enters once in this clause to trigger movement,
      //-> last reached stop is behind the current programmed, and at least stepSeconds seconds have passed since last movement -> movement is triggered
      //-> the blind keeps moving
      //-> next iterations, lastReached still behind, lastMovement sitll > stepSeconds ago, we keep confirming movement
      //-> UNTIL the blind isn't moving, Eureka! programmed stop reached!, we update program pos and last reached stop, and last movement millisecond
      //-> on next iteration, we won't enter this IF clause, till the steps interval has expired
      //
      //
      // OR
      //
      // there's a forced movement to make
      // we keep entering regardless of elapsed milliseconds
      // same as previous case, but we don't update last movement timestamp

      if ((programModeStatus.lastReachedProgramPos  < programModeStatus.programPos &&  ((millis() - programModeStatus.lastMovementMillis) > ((unsigned long)programModeStatus.stepsSeconds * 1000))) || forceMoveToNextProgramStop == true) {
        setMotionEnabled(true, signals);
        //move to the desired position
        DEBUG_PRINTF("Starting movement to reach %d %d %d", programModeStatus.programPos, programModeStatus.stops[programModeStatus.programPos], COMMANDS[programModeStatus.stops[programModeStatus.programPos]]);
        setStopSelector(COMMANDS[programModeStatus.stops[programModeStatus.programPos]], signals);

        //not running, maybe requered stop was received
        if (digitalRead(RUNNING_PIN) == LOW) {
          setMotionEnabled(false, signals);
          currentBlindPosition = readBlindPosition(signals);
          //requested stop reached
          if (currentBlindPosition == signals.bitString) {
            //advance one step and handle rollover to program start
            programModeStatus.lastReachedProgramPos = programModeStatus.programPos;
            programModeStatus.programPos =  (programModeStatus.programPos == programModeStatus.stopsLength - 1) ? 0 : programModeStatus.programPos + 1;
            programModeStatus.lastReachedProgramPos = programModeStatus.programPos > 0 ? programModeStatus.lastReachedProgramPos : -1;
            DEBUG_PRINTF("New program pos reached %d, programpos %d", programModeStatus.lastReachedProgramPos, programModeStatus.programPos);
          } else {
            DEBUG_PRINTF("Requested position not reached, but blind not moving, possible malfunction (selected a stop between 21 and 25 maybe? checkk \"Unreachable Stops\" in deocumentation for details)");
            blinkFeedbackLed(300, 50, 5);
          }
          DEBUG_PRINTF("Stopped at: %s bitString: %s",  getPaddedBin(currentBlindPosition, binBufA), getPaddedBin(signals.bitString, binBufB));
          stopBlind(signals);

          //if we came here following a button command, don't reset time till next step
          if (forceMoveToNextProgramStop == false) {
            programModeStatus.lastMovementMillis = millis();
          } else {
            forceMoveToNextProgramStop = false;
          }
        }
      }
    }
    else {
      DEBUG_PRINTF("ERROR, could not recover program position");
      blinkFeedbackLed(300, 50, 2);
      delay(2000);
    }

  } else if (signals.mode == MANUAL_MODE) {
    setMotionEnabled(true, signals);
    setStopSelector(signals.bitString, signals);
    if (digitalRead(RUNNING_PIN) == LOW) {
      setMotionEnabled(false, signals);
      currentBlindPosition = readBlindPosition(signals);
      //requested stop reached
      if (currentBlindPosition == signals.bitString) {
        manualModeStatus.lastReachedManualStop = currentBlindPosition;
        DEBUG_PRINTF("Stopped in manual mode at: %s, bitString: %s)", getPaddedBin(currentBlindPosition, binBufA), getPaddedBin(signals.bitString, binBufB));
      } else if (manualModeStatus.lastReachedManualStop != currentBlindPosition) {
        DEBUG_PRINTF("Requested position not reached, but  blind not moving, possible malfunction (selected a stop between 21 and 25 maybe? check \"Unreachable Stops\" in deocumentation for details)");
        blinkFeedbackLed(300, 50, 5);

      }
      stopBlind(signals);
    }
  } else {
    DEBUG_PRINTF("ERROR, unknown mode: %d", signals.mode);
    blinkFeedbackLed(100, 100, 10);
    delay(2000);
  }

  int currentPositionIndex = positionExists(currentBlindPosition);
  //If we have stopped in some non existent place, let the user know
  if (currentPositionIndex < 0 && digitalRead(RUNNING_PIN) == LOW && currentBlindPosition > 0) {
    DEBUG_PRINTF("Blind not moving, but detected a non existant roller contacts status %s, possible malfunction", getPaddedBin(currentBlindPosition, binBufA));
    blinkFeedbackLed(300, 50, 5);
  }

  //track how long we've been moving the roller, protect from infinites loops
  movementStartedMillis = (digitalRead(RUNNING_PIN) == LOW ? 0 : ((movementStartedMillis == 0) ? millis() : movementStartedMillis));
  //give and error message and reset in case we've exceeded the maximum allowed uninterrupted run time
  if ((millis() - movementStartedMillis) / 1000 > MAX_CONTINUOS_RUN_SECS && movementStartedMillis > 0) {
    DEBUG_PRINTF("Maximum continuous run exceeded, %d seconds, max allowed: %d pausing for 30seconds and rebooting", ((millis() - movementStartedMillis) / 1000), MAX_CONTINUOS_RUN_SECS);
    setMotionEnabled(false, signals);
    stopBlind(signals);
    blinkFeedbackLed(100, 100, 20);
    delay(30000);
    asm volatile("  jmp 0");
  }


  //two button press commands:
  //3 seconds, switch mode
  //brief, advance 1 step, in manual mode it's 1 stop jump, in program next stop in the program sequence
  if (digitalRead(PUSH_BUTTON_PIN) == HIGH) {
    if (pushButtonPressedMillis == 0) {
      pushButtonPressedMillis = millis();
    }
    // long press
    else if (millis() - pushButtonPressedMillis >= 3000) {
      lastCommandReceived = millis();
      // this totals to 1 second
      blinkFeedbackLed(100, 100, 5);
      if (signals.mode == PROGRAM_MODE) {
        setMode(MANUAL_MODE, signals, programModeStatus, manualModeStatus);
        DEBUG_PRINTF("Manual mode set through button press");
      }
      else {
        setMode(PROGRAM_MODE, signals, programModeStatus, manualModeStatus);
        DEBUG_PRINTF("Program mode set through button press");
      }

      delay(200);  //avoid bounces
      pushButtonPressedMillis = 0;
    }
  }
  // short press
  else if (pushButtonPressedMillis > 0) {
    pushButtonPressedMillis = 0;
    lastCommandReceived = millis();
    if (digitalRead(RUNNING_PIN) == LOW) {
      blinkFeedbackLed(100, 0, 1);
      if (signals.mode == PROGRAM_MODE) {
        //this will force program mode to move one step forward on the next loop
        forceMoveToNextProgramStop = true;
      }
      else {
        //if I get here, I can assume currentBlindPosition is loaded, if it doesn't exist due to some failure, led blinked
        //currentBlindPosition = readBlindPosition(signals);
        if (currentPositionIndex >= 0)
        {
          //first command is STOP, last is RUN, we want to skip both, hence <commandsLength-1 and cycle back to 1 instead of 0
          int newPositionIndex = ((currentPositionIndex + 1) < COMMANDSLEN  ) ? currentPositionIndex + 1 : 1;

          setStopSelector(COMMANDS[newPositionIndex], signals);

          DEBUG_PRINTF("Manual movement command in manual mode received");
        }
        else {
          DEBUG_PRINTF("Skipping manual movement command in manual mode as non existent position read");
        }
      }
    }
  }


#ifndef DEBUG_MODE
  /*
      - Go to sleep if:
        - we are not in debug mode
        - motor is NOT runnig
        - it's been more than 5 minutes since boot
        - we haven't had a button or serial command in more than 5 minutes
  */

  if (digitalRead(RUNNING_PIN) == LOW && (millis() / 1000 > 30) && ((millis() - lastCommandReceived) / 1000) > 30) {
    //this allows >95% sleep time, considering the loop takes a few tens msec, while retaining ease of use
    Serial.println("Going To Sleep");
    LowPower.idle(SLEEP_8S, ADC_OFF, TIMER4_OFF, TIMER3_OFF, TIMER1_OFF, TIMER0_ON, SPI_OFF, USART1_OFF, TWI_OFF, USB_ON);
    Serial.println("Slept");
    if (digitalRead(PUSH_BUTTON_PIN) == HIGH)  //If I've just come && ((millis() - lastCommandReceived) / 1000) > 10out of sleep and the button is pressed, let the user know
    {
      Serial.println("Back from sleep with the button pressed");
      blinkFeedbackLed(200, 0, 1);
      //If we're here, hten the switch went from OFF to ON, which counts as a command
      lastCommandReceived = millis();
      delay(200);
    }
  }
#endif

}

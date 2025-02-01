
#include <Arduino.h>
//#include <LowPower.h>                //https://github.com/rocketscream/Low-Power
#include <Regexp.h>  //https://github.com/nickgammon/Regexp


#define DEBUG_MODE

//automatic program based movement or manual stop selection
const int MANUAL_MODE = 0;
const int PROGRAM_MODE = 1;

struct Signals
{
  //bit string currently set on output pins
  byte bitString=0b00000000;
  //manual/program mode
  int mode=-1;
  //motion enabled status
  bool motionEnabled=false;
};


struct ProgramModeStatus
{
  int programPos=-1;
  int lastReachedProgramPos=-1;
  unsigned long lastMovementMillis = 0;
};

struct ManualModeStatus
{
  //manual mode status variables
  int lastReachedManualStop= -1;
};

//8 bits strings corresponding to each Roma Metropolitana A blind position
//rightmost bit is not relevant as the corresponding pin on the blind bit strings roller is not connected
//sorting mirrors the punch hole roller's, first element selection ("Fuori Servizio") is arbitrary
//first (0000000) and last (1111111) strings are "set all pins LOW/HIGH" commands, which have no correspondance on the roller
byte COMMANDS[] = {
  0b00000000,  // 00 (Stop the blind, wherever it is)
  0b00110100,  // 01 Fuori servizio  
  0b10001100,0b01001100,0b00101100,0b00011100,0b11000010,0b10100010,0b01100010,0b10010010,   //02-26 blank  space
  0b01010010,0b00110010, 0b10001010,0b01001010,0b00101010,0b00011010,0b10000110,0b01000110,
  0b00100110,0b00010110,0b00001110,0b11000000,0b10100000,0b01100000,0b10010000,0b01010000,
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

//map pins to bit string positions, 0 ot 6
const int PIN_MAP[] = { 3, 4, 5, 6, 7, 8, 9 };

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

const int MAX_CONTINUOS_RUN_SECS = 3;
const int DEFAULT_MODE=MANUAL_MODE;

//seconds between each autonomous blind step
const int SECONDS_BETWEEN_STEPS = 2;

const int SECONDS_PER_DAY=86400;


const int RUNNING_PIN = 0;  //Reads high when the blind is rotating
const int BUTTON_PIN = 1;
const int ENABLE_PIN = 10;  //Set to HIGH to enable the output relais coild
const int FEEDBACK_LED_PIN = 14;

//output signal status and current mode stored here, at all times this
// variable represents the desired status configured on output pins
Signals signals;

//status for program and manual mode, i.e. "reached stop X", "last movement recorded on Y"
ProgramModeStatus programModeStatus;
ManualModeStatus manualModeStatus;


unsigned long movementStartedMillis=0;

//TODO read from eeprom
int program[] = { 01, 27, 29, 31, 33, 35, 37, 39 };



//print a message to Serial port if in DEBUG_MODE
void debugMessage(String message) {
#ifdef DEBUG_MODE
  Serial.println(message);
#endif
}

// blinks the feedback led alternating given on/off time, for the given number
// of times.
void blinkFeedbackLed(int onMillis, int offMillis, int iterations) {
  for (int i = 0; i < iterations; i++) {
    digitalWrite(FEEDBACK_LED_PIN, HIGH);
    delay(onMillis);
    digitalWrite(FEEDBACK_LED_PIN, LOW);
    delay(offMillis);
  }
  digitalWrite(FEEDBACK_LED_PIN, LOW);
}


//returns the 8 digits padded BIN  representation of a byte
String getPaddedBin(byte bitString) {

  String binaryString = String(bitString, BIN);  // Convert to binary string

  String padding = "00000000";
  String paddedString = padding + binaryString;
  return paddedString.substring(paddedString.length() - 8);
}




//outputs a bitString to blind control signal pins and saves it as the current one
//this method won't reset pins once the requested position is reached
//this method doesn't check the existance of the requested newBitString
void setStopSelector(byte newBitString,Signals& signals) {
  //debugMessage(getPaddedBin(newBitString));
  //debugMessage(getPaddedBin(currentBitString));
  //note that hte least significant bit is never used, as there's no pin using it
  if (newBitString != signals.bitString) {
    for (int i = 7; i >= 1; i--) {
      // debugMessage(((newBitString >> (i )) & 1) == 1 ? "1" : "0");
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
//this method doesn't check if the returned position exists in the rolles
//this method restores the pre-existing blind status (selected stop and enable status) before returning

byte readBlindPosition(Signals& signals) {

  byte savedBitString = signals.bitString;
  bool savedMotionEnabled = signals.motionEnabled;

  //temporarily disbale motion, to ensure a consistent reading and avoid unwanted movement
  setMotionEnabled(false,signals);

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
  setStopSelector(savedBitString,signals);
  setMotionEnabled(savedMotionEnabled,signals);
  debugMessage("readBlindPosition returning:" + getPaddedBin(currentPosition));
  return currentPosition;
}

void parseSerialCommands(String command,Signals& signals,ProgramModeStatus& programModeStatus,ManualModeStatus& manualModeStatus) {
  // match state object
  MatchState ms;
  ms.Target(command.c_str());


  if (ms.Match(">>GO[0-9][0-9]")) {
    command.replace(">>GO", "");
    int selectedStopIndex = command.toInt();
    if (selectedStopIndex >= 0 and selectedStopIndex < sizeof(COMMANDS) / sizeof(COMMANDS[0])) {
      setStopSelector(COMMANDS[selectedStopIndex], signals);
      Serial.println("Selected stop #" + command);
    } else {
      Serial.println("Invalid stop index received");
    }
  } else if (ms.Match(">>STOP")) {
    stopBlind(signals);
    Serial.println("Blind stop command received");
  } else if (ms.Match(">>RUN")) {
    runBlind(signals);
    Serial.println("Blind run command received");
  } else if (ms.Match(">>FUORISERVIZIO")) {
    setStopSelector(COMMANDS[FUORISERVIZIO], signals);
    Serial.println("Selected Fuori Servizio stop");
  } else if (ms.Match(">>ANAGNINA")) {
    setStopSelector(COMMANDS[ANAGNINA], signals);
    Serial.println("Selected Anagnina stop");
  } else if (ms.Match(">>CINECITTA")) {
    setStopSelector(COMMANDS[CINECITTA], signals);
    Serial.println("Selected Cinecittà stop");
  } else if (ms.Match(">>ARCODITRAVERTINO")) {
    setStopSelector(COMMANDS[ARCODITRAVERTINO], signals);
    Serial.println("Selected Arco di Travertino stop");
  } else if (ms.Match(">>SANGIOVANNI")) {
    setStopSelector(COMMANDS[SANGIOVANNI], signals);
    Serial.println("Selected San Giovanni stop");
  } else if (ms.Match(">>TERMINI")) {
    setStopSelector(COMMANDS[TERMINI], signals);
    Serial.println("Selected Termini stop");
  } else if (ms.Match(">>LEPANTO")) {
    setStopSelector(COMMANDS[LEPANTO], signals);
    Serial.println("Selected Lepanto stop");
  } else if (ms.Match(">>OTTAVIANO")) {
    setStopSelector(COMMANDS[OTTAVIANO], signals);
    Serial.println("Selected Ottaviano stop");
  }
  else if (ms.Match(">>PROGRAMSTOPS[0-9][0-9][\,0,9]?")) {
    Serial.println("Saved auto mode stops program");
  } else if (ms.Match("<<PROGRAM")) {
    Serial.println("TODO Print current auto mode stops program");
  }
  else if (ms.Match(">>PROGRAM")) {
    setMode(PROGRAM_MODE, signals, programModeStatus, manualModeStatus);
    Serial.println("Current auto mode stops program");
  }
  else if (ms.Match(">>MANUAL")) {
    setMode(MANUAL_MODE, signals, programModeStatus, manualModeStatus);
    Serial.println("Manual blind movement selected");
  } else if (ms.Match(">>RESETDEFAULTS")) {
    Serial.println("Loaded default configuration, run >>EEPROMDATA to persist");
  } else if (ms.Match("<<POSITION")) {
    Serial.println(getPaddedBin(readBlindPosition(signals)));
  } else if (ms.Match("<<EEPROMDATA")) {
    Serial.println("Current configuration stored in  EEPROM");
  } else if (ms.Match(">>EEPROMDATA")) {
    Serial.println("Saved current configuration to EEPROM");
  } else if (ms.Match("<<COMPILEDATETIME")) {
    // Serial.println("Software compiled on: " + getStandardTime(DateTime(F(__DATE__), F(__TIME__))).timestamp());
  } else if (!command.equals("")) {
    Serial.println("Unknown command: " + command);
  }
}

//gets the index of the position in the program next to the current one, i.e. blind is at 10th,
// program is 1,3,5,7,9,11,13 4 is returned, as the first program position >10 is 11, fifth element.
//sets output to  -1 on error, returns 0 (first position) as a default
void moveProgramToClosestStop(ProgramModeStatus programModeStatus,Signals& signals) {
  byte currentPosition = readBlindPosition(signals);
  int commandsLength = sizeof(COMMANDS) / sizeof(COMMANDS[0]);
  int currentPositionIndex = -1;
  for (int i = 0; i < commandsLength; i++) {
    debugMessage("moveProgramToClosestStop (COMMANDS[i] =" + getPaddedBin(COMMANDS[i]) + " currentPosition=" + getPaddedBin(currentPosition));
    if (COMMANDS[i] == currentPosition) {
      currentPositionIndex = i;
      break;
    }
  }
  if (currentPositionIndex == -1) {
    debugMessage("moveProgramToClosestStop Error, non existent position detected, currentPosition=" + getPaddedBin(currentPosition) + " currentBitString=" + getPaddedBin(signals.bitString));
    programModeStatus.programPos = -1;
  }
  int programLength = sizeof(program) / sizeof(program[0]);
  for (int i = 0; i < programLength; i++) {
    if (program[i] >= currentPositionIndex) {
      //identified the position in the program next to the current one
      programModeStatus.programPos = i;
      debugMessage("moveProgramToClosestStop returning " + String(programModeStatus.programPos));
      return;
    }
  }
  programModeStatus.programPos = 0;
  debugMessage("moveProgramToClosestStop returning " + String(programModeStatus.programPos));
}

//sets the current motion mode, either program or manual, stops motion if required,
//this method resets program position to -1, recalculation is required
void setMode(int newMode, Signals& signals,ProgramModeStatus& programModeStatus,ManualModeStatus& manualModeStatus) {
  if (signals.mode == newMode) {
    //nothing to do
    debugMessage("setMode nothing to do");
    return;
  }
  if (newMode != MANUAL_MODE && newMode != PROGRAM_MODE) {
    debugMessage("setMode: invalid mode requested, received " + String(newMode));
  }
  signals.mode = newMode;
  stopBlind(signals);
  if (newMode == MANUAL_MODE) {
    programModeStatus.programPos = -1;
    manualModeStatus.lastReachedManualStop=readBlindPosition(signals);
    digitalWrite(FEEDBACK_LED_PIN, HIGH);
  } else if (newMode == PROGRAM_MODE) {
    //todo recover position
    moveProgramToClosestStop(programModeStatus, signals);
    digitalWrite(FEEDBACK_LED_PIN, LOW);
  }
}

void setup() {
  Serial.begin(9600);
  //7 pins controlling the blind
  for (int i = 0; i < 7; i++) {
    pinMode(PIN_MAP[i], OUTPUT);
  }
  pinMode(RUNNING_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(FEEDBACK_LED_PIN, OUTPUT);

  //sets selector pins to 0 and disables movement
  stopBlind(signals);
  setMotionEnabled(false,signals);

  //set the current mode to default
  setMode(DEFAULT_MODE, signals,programModeStatus,manualModeStatus);
  
  //todo read conf from EEPROM
}

void loop() {
  delay(1000);

  // self reset every week, just in case I f*cked up and some variable would
  // overflow left unchecked (i.e. millis)
  if (millis()/1000 >= SECONDS_PER_DAY * 7) {
    asm volatile("  jmp 0");
  }

  if (Serial.available() > 0) {
    String command = "";
    int i = 0;
    while (Serial.available() > 0 && i < 64) {
      char c = Serial.read();
      if (c == '\n') {
        break;  // Break when newline character is received
      }
      command += c;
      i++;
    }
    parseSerialCommands(command, signals,programModeStatus,manualModeStatus);
  }

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
      debugMessage("Recovering next program position: " + String(programModeStatus.programPos));
    }
    if (programModeStatus.programPos >= 0 ) {
      if (programModeStatus.lastReachedProgramPos  < programModeStatus.programPos && millis() - programModeStatus.lastMovementMillis > SECONDS_BETWEEN_STEPS * 1000) {
        setMotionEnabled(true, signals);
        //move to the desired position
        setStopSelector(COMMANDS[program[programModeStatus.programPos]], signals);
        //not running, maybe requered stop was received
        if (digitalRead(RUNNING_PIN) == LOW) {
          setMotionEnabled(false, signals);
          byte currentBlindPosition = readBlindPosition(signals);
          //requested stop reached
          if (currentBlindPosition == signals.bitString) {
            //advance one step and handle rollover to program start
            programModeStatus.lastReachedProgramPos = programModeStatus.programPos;
            programModeStatus.programPos = programModeStatus.programPos == ((sizeof(program) / sizeof(program[0])) - 1) ? 0 : programModeStatus.programPos + 1;
            programModeStatus.lastReachedProgramPos = programModeStatus.programPos > 0 ? programModeStatus.lastReachedProgramPos : -1;
          } else {
            debugMessage("Requested position not reached, but  blind not moving, possible malfunction");
             blinkFeedbackLed(300, 50, 5);
          }
          debugMessage("Stopped at: " + getPaddedBin(currentBlindPosition) + " bitString: " + getPaddedBin(signals.bitString));
          stopBlind(signals);
          programModeStatus.lastMovementMillis = millis();
        }
      }
    }
    else {
      debugMessage("ERROR, could not recover program position");
      blinkFeedbackLed(300, 50, 2);
      delay(2000);
    }
  } else if (signals.mode == MANUAL_MODE) {
    setMotionEnabled(true,signals);
    setStopSelector(signals.bitString, signals);
    if (digitalRead(RUNNING_PIN) == LOW) {
      setMotionEnabled(false,signals);
      byte currentBlindPosition = readBlindPosition(signals);
      //requested stop reached
      if (currentBlindPosition == signals.bitString) {
        manualModeStatus.lastReachedManualStop=currentBlindPosition;
        debugMessage("Stopped in manual mode at: " + getPaddedBin(currentBlindPosition) + " bitString: " + getPaddedBin(signals.bitString));
      } else if (manualModeStatus.lastReachedManualStop!=currentBlindPosition){
        debugMessage("Requested position not reached, but  blind not moving, possible malfunction");
        blinkFeedbackLed(100, 100, 5);
      }
      stopBlind(signals);
    }
  } else {
    debugMessage("ERROR, unknown mode: " + String(signals.mode));
    blinkFeedbackLed(100, 100, 10);
    delay(2000);
  }

  //track how long we've been moving the roller
  movementStartedMillis=digitalRead(RUNNING_PIN)?0:(movementStartedMillis==0)?millis():movementStartedMillis;
  //give and error message and reset in case we've exceeded the maximum allowed uninterrupted run time
  if ((millis()-movementStartedMillis)/1000>MAX_CONTINUOS_RUN_SECS){
    debugMessage("Maximum continuous run exceeded, "+String((millis()-movementStartedMillis)/1000)+" seconds, max allowed: "+String(MAX_CONTINUOS_RUN_SECS));
    blinkFeedbackLed(100, 100, 20);
    delay(30000);
    asm volatile("  jmp 0");
  }

}

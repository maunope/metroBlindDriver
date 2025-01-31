
#include <Arduino.h>
//#include <LowPower.h>                //https://github.com/rocketscream/Low-Power
#include <Regexp.h>  //https://github.com/nickgammon/Regexp


//#define DEBUG_MODE

// 8 bits strings corresponding to each Roma Metropolitana A blind position
// rightmost bit is not relevant as the corresponding pin on the blind bit strings roller is not connected
byte COMMANDS[] = {
  0b00000000  // 00 (Stop the blind, wherever it is)
  ,
  0b00110100  // 01 Fuori servizio
  ,
  0b10001100, 0b01001100, 0b00101100, 0b00011100, 0b11000010, 0b10100010, 0b01100010, 0b10010010, 0b01010010,
  0b00110010, 0b10001010, 0b01001010, 0b00101010, 0b00011010, 0b10000110, 0b01000110, 0b00100110, 0b00010110,
  0b00001110, 0b11000000, 0b10100000, 0b01100000, 0b10010000, 0b01010000, 0b11100000,
  0b11010000  // 27 Anagnina
  ,
  0b10110000, 0b01110000  // 29 Cinecittà
  ,
  0b11001000, 0b10101000  // 31 Arco di Travertino
  ,
  0b01101000, 0b10011000  // 33 San giovanni
  ,
  0b01011000, 0b00111000  // 35 Termini
  ,
  0b11000100, 0b10100100  // 37 Lepanto
  ,
  0b01100100, 0b10010100  // 39 Ottaviano
  ,
  0b01010100, 0b11111111  // 41 (Run the blind, wherever it is)};
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

//automatic program based movement or manual stop selection
const int MANUAL_MODE = 0;
const int PROGRAM_MODE = 1;

//seconds between each autonomous blind step
const int SECONDS_BETWEEN_STEPS = 2;


const int RUNNING_PIN = 0;  //Reads high when the blind is rotating
const int BUTTON_PIN = 1;
const int ENABLE_PIN = 10;  //Set to HIGH to enable the output relais coild
const int LED_PIN = 14;




//stores the bit string currently set on output pins
byte currentBitString = 0b00000000;
//stores current motion enabled status
bool motionEnabled = false;

//TODO read from eeprom
int program[] = { 01, 27, 29, 31, 33, 35, 37, 39 };

int currentProgramPos = -1;
int lastReachedProgramPos = -1;

int currentMode = MANUAL_MODE;

unsigned long lastMovementMillis = 0;


//returns the 8 digits padded BIN  representation of a byte
String getPaddedBin(byte bitString) {

  String binaryString = String(bitString, BIN);  // Convert to binary string

  String padding = "00000000";
  String paddedString = padding + binaryString;
  return paddedString.substring(paddedString.length() - 8);
}

//print a message to Serial port if in DEBUG_MODE
void debugMessage(String message) {
#ifdef DEBUG_MODE
  Serial.println(message);
#endif
}


//outputs a bitString to blind control signal pins and saves it as the current one
//this method won't reset pins once the requested position is reached
//this method doesn't check the existance of the requested newBitString
void setStopSelector(byte newBitString, byte& currentBitString) {
  //Serial.println(getPaddedBin(newBitString));
  //Serial.println(getPaddedBin(currentBitString));
  //note that hte least significant bit is never used, as there's no pin using it
  if (newBitString != currentBitString) {
    for (int i = 7; i >= 1; i--) {
      // Serial.println(((newBitString >> (i )) & 1) == 1 ? "1" : "0");
      digitalWrite(PIN_MAP[7 - i], ((newBitString >> (i)) & 1) == 1 ? HIGH : LOW);
    }

    currentBitString = newBitString;
  }
}



//sends a stop command to the blind, regardless of its position
void stopBlind(byte& selectedBitString) {
  setStopSelector(COMMANDS[STOP], selectedBitString);
}


//sends a run command to the blind, regardless of its position,
//motion will continue until a different command is sent or enable pin is set to low
void runBlind(byte& selectedBitString) {
  setStopSelector(COMMANDS[RUN], selectedBitString);
}


//enables/disables blind movement
bool setMotionEnabled(bool enable, bool& motionEnabled) {
  digitalWrite(ENABLE_PIN, enable ? HIGH : LOW);
  motionEnabled = enable;
}

//reads the current blind position,disabling motion temporarily
//this method doesn't check if the blind is rolling before stopping it
//this method doesn't check if the returned position exists in the rolles
//this method restores the pre-existing blind status (selected stop and enable status) before returning

byte readBlindPosition(byte& currentBitString, bool& motionEnabled) {

  byte savedBitString = currentBitString;
  bool savedMotionEnabled = motionEnabled;

  //temporarily disbale motion, to ensure a consistent reading and avoid unwanted movement
  setMotionEnabled(false, motionEnabled);

  byte currentPosition = 0;
  //only 7 as we're cycling on pins, but then we need to shift 8
  for (int i = 0; i < 7; i++) {
    stopBlind(currentBitString);

    digitalWrite(PIN_MAP[i], HIGH);

    //inverts (HIGH outputs 0, LOW outpus 1), for easier readability/debugging this method represents stops bit strings as the corresponding selector bit string
    //Actually, if the selected bitString is 11001100, motion stops on 00110011 configurationon of the blind roller
    //(motion stop when bitwise AND between selected string and rolles string is 0)
    currentPosition |= ((digitalRead(RUNNING_PIN) == HIGH ? 0 : 1) << (7 - i));
  }

  //reset previous state
  setStopSelector(savedBitString, currentBitString);
  setMotionEnabled(savedMotionEnabled, motionEnabled);

  return currentPosition;
}

void parseSerialCommands(String command, byte& currentBitString, bool& motionEnable, int& currentMode) {
  // match state object
  MatchState ms;
  ms.Target(command.c_str());


  if (ms.Match(">>GO[0-9][0-9]")) {
    command.replace(">>GO", "");
    int selectedStopIndex = command.toInt();
    if (selectedStopIndex >= 0 and selectedStopIndex < sizeof(COMMANDS) / sizeof(COMMANDS[0])) {
      setStopSelector(COMMANDS[FUORISERVIZIO], currentBitString);
      Serial.println("Selected stop #" + command);
    } else {
      Serial.println("Invalid stop index received");
    }
  } else if (ms.Match(">>STOP")) {
    stopBlind(currentBitString);
    Serial.println("Blind stop command received");
  } else if (ms.Match(">>RUN")) {
    runBlind(currentBitString);
    Serial.println("Blind run command received");
  } else if (ms.Match(">>FUORISERVIZIO")) {
    setStopSelector(COMMANDS[FUORISERVIZIO], currentBitString);
    Serial.println("Selected Fuori Servizio stop");
  } else if (ms.Match(">>ANAGNINA")) {
    setStopSelector(COMMANDS[ANAGNINA], currentBitString);
    Serial.println("Selected Anagnina stop");
  } else if (ms.Match(">>CINECITTA")) {
    setStopSelector(COMMANDS[CINECITTA], currentBitString);
    Serial.println("Selected Cinecittà stop");
  } else if (ms.Match(">>ARCODITRAVERTINO")) {
    setStopSelector(COMMANDS[ARCODITRAVERTINO], currentBitString);
    Serial.println("Selected Arco di Travertino stop");
  } else if (ms.Match(">>SANGIOVANNI")) {
    setStopSelector(COMMANDS[SANGIOVANNI], currentBitString);
    Serial.println("Selected San Giovanni stop");
  } else if (ms.Match(">>TERMINI")) {
    setStopSelector(COMMANDS[TERMINI], currentBitString);
    Serial.println("Selected Termini stop");
  } else if (ms.Match(">>LEPANTO")) {
    setStopSelector(COMMANDS[LEPANTO], currentBitString);
    Serial.println("Selected Lepanto stop");
  } else if (ms.Match(">>OTTAVIANO")) {
    setStopSelector(COMMANDS[OTTAVIANO], currentBitString);
    Serial.println("Selected Ottaviano stop");
  }
  else if (ms.Match(">>PROGRAM[0-9][0-9][\,0,9]?")) {
    Serial.println("Saved auto mode stops program");
  } else if (ms.Match("<<PROGRAM")) {
    Serial.println("TODO Print current auto mode stops program");
  }
  else if (ms.Match(">>PROGRAM")) {
    setMode(currentMode,  PROGRAM_MODE, currentBitString, currentProgramPos,motionEnabled);
    Serial.println("Current auto mode stops program");
  }
  else if (ms.Match(">>MANUAL")) {
    setMode(currentMode,  MANUAL_MODE, currentBitString, currentProgramPos,motionEnabled);
    Serial.println("Manual blind movement selected");
  } else if (ms.Match(">>RESETDEFAULTS")) {
    Serial.println("Loaded default configuration, run >>EEPROMDATA to persist");
  } else if (ms.Match("<<POSITION")) {
    Serial.println(getPaddedBin(readBlindPosition(currentBitString, motionEnabled)));
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
int getClosestProgramPosition(int& currentProgramPos, byte& currentBitString, bool& motionEnabled) {
  byte currentPosition = readBlindPosition(currentBitString, motionEnabled);
  int commandsLength = sizeof(COMMANDS) / sizeof(COMMANDS[0]);
  int currentPositionIndex = -1;
  for (int i = 0; i < commandsLength; i++) {
    if (COMMANDS[i] == currentBitString) {
      int currentPositionIndex = i;
      break;
    }
  }
  if (currentPositionIndex == -1) {
    Serial.println("Error, non existent position detected");
    currentProgramPos = -1;
  }
  int programLength = sizeof(program) / sizeof(program[0]);
  for (int i = 0; i < programLength; i++) {
    if (program[i] >= currentPositionIndex) {
      //identified the position in the program next to the current one
      currentProgramPos = i;
    }
  }
  currentProgramPos = 0;
}

//sets the current motion mode, either program or manual, stops motion if required,
//this method resets program position to -1, recalculation is required
void setMode(int& currentMode, int newMode, byte& currentBitString, int& currentProgramPos, bool& motionEnabled) {
  if (currentMode == newMode) {
    //nothing to do
    return;
  }
  if (newMode != MANUAL_MODE && newMode != PROGRAM_MODE) {
    Serial.println("setMode: invalid mode requested, received " + String(newMode));
  }
  currentMode = MANUAL_MODE;
  stopBlind(currentBitString);
  if (newMode == MANUAL_MODE) {
    currentProgramPos = -1;
  } else if (newMode == PROGRAM_MODE) {
    //todo recover position
    getClosestProgramPosition(currentProgramPos, currentBitString, motionEnabled);
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
  pinMode(LED_PIN, OUTPUT);

  //sets selector pins to 0 and disables movement
  stopBlind(currentBitString);
  digitalWrite(ENABLE_PIN, LOW);

  //set the current mode to default
  setMode(currentMode, currentMode, currentBitString, currentProgramPos, motionEnabled);

  //todo read conf from EEPROM
}

void loop() {
  delay(100);

  // self reset every week, just in case I f*cked up and some variable would
  // overflow left unchecked
  //Serial.println(String((RTCDateTime-bootTime).totalseconds())+" "+String(SECONDS_PER_DAY*7));
  /*if ((RTCDateTime - bootTime).totalseconds() >= SECONDS_PER_DAY * 7) {
    asm volatile("  jmp 0");
    }*/
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
    parseSerialCommands(command, currentBitString, motionEnabled, currentMode);
  }

  ///
  /// Program mode:
  /// - Check what the next step is
  //  - if last reached position is behind the current selected and the delay between steps has elapsed, Enable motion to the desired stop
  //  - (Loop repeats until rotation stops, signals generation are reiterated on each loop)
  //  - rotation is stopped, check if a valid position has been reached, and move on to the next step in the program
  //  - sleep (TODO make a non blockng one)
  if (currentMode == PROGRAM_MODE) {
    if (currentProgramPos == -1) {
      //at boot we need to recover what the next position is
      getClosestProgramPosition(currentProgramPos, currentBitString, motionEnabled);
      Serial.println("Recovering next program position: " + String(currentProgramPos));
    }
    if (currentProgramPos >= 0 && lastReachedProgramPos < currentProgramPos && millis() - lastMovementMillis > SECONDS_BETWEEN_STEPS * 1000) {
      motionEnabled = true;
      digitalWrite(ENABLE_PIN, motionEnabled);
      //move to the desired position
      setStopSelector(COMMANDS[program[currentProgramPos]], currentBitString);
      //not running, maybe requered stop was received
      if (digitalRead(RUNNING_PIN) == LOW) {
        stopBlind(currentBitString);
        motionEnabled = false;
        digitalWrite(ENABLE_PIN, motionEnabled);
        byte currentBlindPosition = readBlindPosition(currentBitString, motionEnabled);
        //requested stop reached
        if (currentBlindPosition == currentBitString) {
          digitalWrite(LED_PIN, LOW);
          //advance one step and handle rollover to program start
          lastReachedProgramPos = currentProgramPos;
          currentProgramPos = currentProgramPos == ((sizeof(program) / sizeof(program[0])) - 1) ? 0 : currentProgramPos + 1;
          lastReachedProgramPos = currentProgramPos > 0 ? lastReachedProgramPos : -1;
        } else {
          Serial.println("Requested position not reached, but  blind not moving, possible malfunction");
          digitalWrite(LED_PIN, HIGH);
        }
        Serial.println("Stopped at: " + getPaddedBin(currentBlindPosition) + " bitString: " + getPaddedBin(currentBitString));
        lastMovementMillis = millis();
      } else {
        //I'm running
      }
    } else {
      Serial.println("ERROR, could not recover program position");
      digitalWrite(LED_PIN, HIGH);
      delay(2000);
    }
  } else if (currentMode == MANUAL_MODE) {
    //IMPLEMENT MANUAL MODE
  } else {
    Serial.println("ERROR, unknown mode: " + String(currentMode));
    digitalWrite(LED_PIN, HIGH);
    delay(2000);
  }
}

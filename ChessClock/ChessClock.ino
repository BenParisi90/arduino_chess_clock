#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

// Total time of the game
unsigned long gameTime = 600000;
// Amount of time returned to the player each move
unsigned long gameTimeBack = 0;
// The millis of the previous frame. Used to calculate delta time
unsigned long prevTime = 0;
// Is the game clock currently counting down?
bool gameActive = false;

unsigned long possibleGameTimes[] = {
  60000, 120000, 180000, 300000, 600000, 900000, 1800000, 3600000
};
int numPossibleGameTimes = sizeof(possibleGameTimes) / sizeof(possibleGameTimes[0]);
int gameTimeIndex = 5;

unsigned long possibleGameTimeBacks[] = {
  0, 1000, 2000, 3000, 5000, 10000, 15000, 60000, 300000
};
int numPossibleGameTimeBacks = sizeof(possibleGameTimeBacks) / sizeof(possibleGameTimeBacks[0]);
int gameTimeBackIndex = 0;

// Pin definitions
const int PLAYER_PINS[] = {13, 12};
const int PLAYER_LIGHT_PINS[] = {3, 2};
const int ENCODER_PINS[] = {6, 7};
const int MODE_PIN = 8;

// Push button bool for mode pin btn
bool modePinPressed = false;
// Push button bools for player pins
bool playerPinsPressed[] = {false, false};

// Remaining time in the game for each player
long playerTimes[2] = {gameTime, gameTime};

// -1 for no turn, 0 for player 1, 1 for player 2
int activePlayer = -1;

// Modes are:
// 0 - game mode with clocks
// 1 - select game time
// 2 - select seconds back
int currentMode = 0;
const int NUM_MODES = 3;

// When time selecting, blink the part you are currently selecting, hrs, mins, or seconds back
const int BLINK_ON_TIME = 800;
const int BLINK_OFF_TIME = 200;
bool blinkOn = true;
int blinkCurrentTime = 0;

// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(115200);
  Serial.println("BEGIN CHESS CLOCK");
  // Initialize the LCD
  lcd.begin();
  
  pinMode(MODE_PIN, INPUT);
  for(int i = 0; i < 2; i ++)
  {
    pinMode(PLAYER_PINS[i], INPUT);
    pinMode(ENCODER_PINS[i], INPUT);
    pinMode(PLAYER_LIGHT_PINS[i], OUTPUT);
  }

  InitializeMode(currentMode);
}

void loop() {
  // Calculate delta time
  unsigned long deltaTime = millis() - prevTime;
  prevTime = millis();

  // Read mode pin
  ModeSwitching();

  // Update game clock
  GameClock(deltaTime);

  // Select time
  TimeSelect();

  // Handle blinking
  Blinking(deltaTime);
}

void StartGame(int startingPlayer) {
  gameActive = true;
  playerTimes[0] = gameTime;
  playerTimes[1] = gameTime;
  SetActivePlayerTurn(startingPlayer == 0 ? 1 : 0);
}

void EndGame(int endingPlayer) {
  playerTimes[endingPlayer] = 0;
  gameActive = false;
  blinkOn = true;
  blinkCurrentTime = BLINK_ON_TIME;
}

void ResetGame() {
  gameActive = false;
  SetActivePlayerTurn(-1);
  playerTimes[0] = gameTime;
  playerTimes[1] = gameTime;
  PrintPlayerTimes();
}

void SetActivePlayerTurn(int player) {
  activePlayer = player;
  playerTimes[player == 0 ? 1 : 0] += gameTimeBack;
  for (int i = 0; i < 2; i++) {
    digitalWrite(PLAYER_LIGHT_PINS[i], i == player ? HIGH : LOW);
  }
}

void ModeSwitching() {
  int modePinValue = digitalRead(MODE_PIN);
  if (modePinValue == 0 && !modePinPressed) {
    modePinPressed = true;
    if (gameActive) {
      ResetGame();
    } else {
      if (activePlayer == -1) {
        currentMode = (currentMode + 1) % NUM_MODES;
        InitializeMode(currentMode);
      } else {
        ResetGame();
      }
    }
    Serial.println(currentMode);
  } else if (modePinValue == 1 && modePinPressed) {
    modePinPressed = false;
  }
}

void InitializeMode(int mode) {
  if (mode == 0) {
    lcd.setCursor(0, 0);
    lcd.print("Player1  Player2");
    // Set the cursor to the second row
    lcd.setCursor(0, 1);
    lcd.print(readableTime(playerTimes[0]) + "      " + readableTime(playerTimes[1]));
  } else if (mode == 1) {
    lcd.setCursor(0, 0);
    lcd.print("Game    Timeback");
  }
}

void GameClock(unsigned long deltaTime) {
  if (currentMode != 0) {
    return;
  }
  for(int i = 0; i < 2; i ++) {
    if(!digitalRead(PLAYER_PINS[i])) {
      playerPinsPressed[i] = false;
    }
  }
  if (gameActive) {
    int otherPlayer = activePlayer == 0 ? 1 : 0;
    playerTimes[activePlayer] -= deltaTime;
    if (playerTimes[activePlayer] <= 0) {
      EndGame(activePlayer);
    }
    if(digitalRead(PLAYER_PINS[activePlayer]) && !playerPinsPressed[activePlayer]) {
      SetActivePlayerTurn(otherPlayer);
      playerPinsPressed[activePlayer] = true;
    }
    PrintPlayerTimes();
  } else if (activePlayer == -1) {
    for (int i = 0; i < 2; i++) {
      if (digitalRead(PLAYER_PINS[i])) {
        StartGame(i);
        break;
      }
    }
  }
}

void PrintPlayerTimes() {
  lcd.setCursor(0, 1);
  lcd.print(readableTime(playerTimes[0]) + "      " + readableTime(playerTimes[1]));
}

void TimeSelect() {
  if (currentMode == 0) {
    return;
  }

  int encoderOutputA = ENCODER_PINS[0];
  int encoderOutputB = ENCODER_PINS[1];

  int aState = digitalRead(encoderOutputA);
  static int aLastState = aState;

  if (aState != aLastState) {
    blinkCurrentTime = 0;
    int direction = digitalRead(encoderOutputB) != aState ? 1 : -1;

    if (currentMode == 1) {
      gameTimeIndex = (gameTimeIndex + direction + numPossibleGameTimes) % numPossibleGameTimes;
      gameTime = possibleGameTimes[gameTimeIndex];
      playerTimes[0] = gameTime;
      playerTimes[1] = gameTime;
    } else if (currentMode == 2) {
      gameTimeBackIndex = (gameTimeBackIndex + direction + numPossibleGameTimeBacks) % numPossibleGameTimeBacks;
      gameTimeBack = possibleGameTimeBacks[gameTimeBackIndex];
    }
  }
  aLastState = aState;

  String returnString = readableTime(gameTime) + "      " + readableTime(gameTimeBack);
  // Hours select
  if (currentMode == 1 && !blinkOn) {
    returnString[0] = ' ';
    returnString[1] = ' ';
    returnString[3] = ' ';
    returnString[4] = ' ';
  } else if (currentMode == 2 && !blinkOn) {
    returnString[11] = ' ';
    returnString[12] = ' ';
    returnString[14] = ' ';
    returnString[15] = ' ';
  }
  lcd.setCursor(0, 1);
  lcd.print(returnString);
}

String readableTime(unsigned long milliseconds) {
  String readableString = "";
  unsigned long seconds = milliseconds / 1000; // Convert milliseconds to seconds
  unsigned long minutes = seconds / 60; // Calculate total minutes
  seconds = seconds % 60; // Remaining seconds after dividing by 60
  
  if (minutes < 10) { // Ensure minutes are displayed with two digits
    readableString  += "0";
  }
  readableString += minutes;
  readableString += ":";
  if (seconds < 10) { // Ensure seconds are displayed with two digits
    readableString += "0";
  }
  readableString += seconds;
  return readableString;
}

void Blinking(unsigned long deltaTime) {
  blinkCurrentTime += deltaTime;
  if ((blinkOn && blinkCurrentTime >= BLINK_ON_TIME) || (!blinkOn && blinkCurrentTime >= BLINK_OFF_TIME)) {
    blinkOn = !blinkOn;
    blinkCurrentTime = 0;
    // Blink the lights if the game is over and stuck on someone's turn
    if (!gameActive) {
      digitalWrite(PLAYER_LIGHT_PINS[activePlayer], blinkOn ? HIGH : LOW);
    }
  }
}

//#include "ServoTimer2.h"
#include<Servo.h>
#define FASTLED_ALLOW_INTERRUPTS 1


#include <FastLED.h>

const int servoOnePin = 5;
//const int buttonOnePin = 2;

const int lightOnePin = 28;
const int finishLineDistance = 12000;

/////////////////CONTROLLER PINS////////////////////////////
const int controller1Gear1Pin = 22;
const int controller1Gear2Pin = 23;
const int controller1Gear3Pin = 24;
const int controller1Gear4Pin = 25;
const int controller1GasPin = 13;


////////////////////NEOPIXELS/////////////////////////////
const int LED_COUNT = 30;

CRGB stripOne[LED_COUNT];
const int stripOnePin = 29;









class Controller {
public:


  int gasPin;
  //because our physical joystick is gonna rely on being IN a current gear, not a button,
  //we should track it in Controller, not car.
  int currentGear = 1;

  bool waitingToGetInGearOne = false;

  int gearPins[4];


  void setupController(int g1Pin, int g2Pin, int g3Pin, int g4Pin, int g5Pin, int g6Pin, int theGasPin) {

    Serial.println("SET UP CONTROLLER");

    gearPins[0] = g1Pin;
    gearPins[1] = g2Pin;
    gearPins[2] = g3Pin;
    gearPins[3] = g4Pin;
    gasPin = theGasPin;


    //gasPin = theGasPin;
  }

  int getCurrentGear() {
    //checked every frame from main car loop

    int gearShifterPosition = 0;
    //i corresponds to array value.
    for (int i = 0; i < 4; i++) {
      bool status = digitalRead(gearPins[i]);


      if (status == 0) {
        //its pressed down.
        gearShifterPosition = i + 1;
      }
    }  //should it be required that its one up from the last gear???
    if (gearShifterPosition != 0) {
      currentGear = gearShifterPosition;
    } else {
      currentGear = 0;
    }
    //if the shifter is in limbo, currentGear will be 0

    return currentGear;
  }
  void waitForPlayerToResetToGearOne(int lightPin) {
    bool lightStatus = 0;
    Serial.println("GET IN GEAR 1. NOW.");
    int i = 0;
    while (getCurrentGear() != 1) {
      i++;
      if (i % 4 == 0) {
        lightStatus = !lightStatus;
        //Serial.print(lightStatus);
      }

      digitalWrite(lightPin, lightStatus);

      
    }

    digitalWrite(lightPin, LOW);
    return;
  }
  bool getGasPedal() {

    return !digitalRead(gasPin);
  }
};


class Car {
public:

  Servo needle;


  double needleIncrement = 0;
  double lastNeedleIncrement = 0;
  int currentCarGear;
  int lastCarGear = 1;

  bool burnout = false;
  int burnoutTimer = 0;

  const int SAFE_SHIFT_THRESHOLD = 90;
  const int SPEED_BOOST_THRESHOLD = 165;

  Controller controller;

  CRGB *lightStripPointer;

  bool boost = false;

  int lightPin;

  

  double totalDistance = 0;
  double velocity = 0;

 

  void resetStrip(bool win) {
    int num = 0;
    if (win) {
      num = 100;
    }
    for (int i = 0; i < LED_COUNT; i++) {
      lightStripPointer[i] = CRGB(0, 0, num);
    }
  }

  void setupCar(int servoPin, Controller theController, int theLightPin, CRGB *theStrip) {

    controller = theController;
    lightStripPointer = theStrip;

    lightPin = theLightPin;

    
    resetStrip(false);
    needle.attach(servoPin);
    Serial.println("setup");
    needle.write(0);
  }

  void resetCar() {
    totalDistance = 0;
    velocity = 0;
    needleIncrement = 0;
    lastCarGear = 0;
    currentCarGear = 0;
    boost = false;
    resetStrip(false);
  }

  void setLightDistance() {
  
    int ledsToShow = LED_COUNT / (finishLineDistance/totalDistance);//a ratio from 0 to 1 indicating completion.
    for (int i = 0; i < ledsToShow; i++) {
      lightStripPointer[i] = CRGB(0, 100, 0);
    }
  }

  



  bool loopCar(float dt) {
    
    bool neutral = false;


    currentCarGear = controller.getCurrentGear();

    bool gas = controller.getGasPedal();


    if (burnout == true) {
      //count car out of burnout.
      burnoutTimer++;
      if (burnoutTimer >= 350) {
        //out of burnout mode!!!
        burnoutTimer = 0;
        burnout = false;
        
        //Okay, lets reset the player.
        //WE GOTTA FORCE PLAYER TO GO BACK TO GEAR 1 SOMEHOW
        controller.waitForPlayerToResetToGearOne(lightPin);
        digitalWrite(lightPin, LOW);
      
      } 
      else {
        return false;
      }

    }

    if (currentCarGear == 0) {
      neutral = true;
      currentCarGear = lastCarGear;
    }

    //Serial.print(currentCarGear);
    //Serial.print(" last gear was ");
    //Serial.println(lastCarGear);

    //determine current needle zone.


    
    if (needleIncrement < 180 && currentCarGear == lastCarGear) {
      //Nothing happening, just keep stepping
      if (gas == true && neutral == false) {

        needleIncrement += (0.2 * (currentCarGear / 2.0)) * dt * 100.0;
      } else if ((needleIncrement - 0.1 * (currentCarGear / 2.0)) > 0) {
        //rpms will fall off if were in neutral OR theres no gas.
        needleIncrement -= (0.1 * (currentCarGear / 2.0)) * dt * 100.0;
      } else {
        needleIncrement = needleIncrement;  //keep it the same.
      }

      velocity = (needleIncrement / 50.0) * currentCarGear;

      totalDistance = totalDistance + velocity;
      if (boost == true) {
        boost = false;
        totalDistance = totalDistance + 200;
      }
      setLightDistance();
      if (totalDistance >= finishLineDistance) {
        return true;
        //END THE GAME. YOU WON!!!
      }
      if (needleIncrement != lastNeedleIncrement) {
        Serial.println(needleIncrement);
        needle.write(needleIncrement);  //values between 0 and 180
      }
      

      
      digitalWrite(lightPin, LOW);
    } else if (needleIncrement >= 180) {
      burnout = true;
    }

    if (currentCarGear != lastCarGear && burnout == false) {
      //Something has changed........
      if (needleIncrement > SAFE_SHIFT_THRESHOLD) {
        if (gas == true) {
          //thats bad. BURNOUT.
          burnout = true;

        } else {
          //Safely shifted.
          if (needleIncrement > SPEED_BOOST_THRESHOLD) {
            Serial.println("SPEED BOOST");
            digitalWrite(lightPin, HIGH);
            boost = true;
          }
          needleIncrement = 0;
         
        
          needle.write(needleIncrement);
          Serial.println("Shift to Gear" + String(currentCarGear));
          //delay(350);  //time for needle to reset. //I dont want delays.
        }



      } else {
        //SHIFTED TOO EARLY!
        burnout = true;
      }
    }

    if (burnout == true) {
      //BURN OUT. either needle got too high. or shifted before 90.
      needleIncrement = 0;
      
      velocity = 0;
      needle.write(needleIncrement);
      Serial.println("YOU BURNT OUTt");
      digitalWrite(lightPin, HIGH);
      return false;
    }


    lastNeedleIncrement = needleIncrement;
    lastCarGear = currentCarGear;
    return false;  //car not at finish line.
  }
};

Car carOne = Car();
Controller controllerOne = Controller();



class Game {
public:
  bool gameIsInProgress = true;
  void startNewGame() {
    gameIsInProgress = false;
    //reset old game
    carOne.resetCar();

    waitForPlayers();

  }

  void waitForPlayers() {
    Serial.println("WAITING FOR PLAYERS.....");
    delay(90000);
    //start new game
    controllerOne.waitForPlayerToResetToGearOne(lightOnePin);
    //do the same for player two.

    gameIsInProgress = true;
  }

  bool getGameStatus() {
    return gameIsInProgress;
  }
};

Game currentGame = Game();



void setup() {
  delay(1000);
  Serial.begin(9600);
  Serial.println("begin");
  //pinMode(buttonOnePin, INPUT);
  pinMode(lightOnePin, OUTPUT);
  pinMode(controller1Gear1Pin, INPUT_PULLUP);
  pinMode(controller1Gear2Pin, INPUT_PULLUP);
  pinMode(controller1Gear3Pin, INPUT_PULLUP);
  pinMode(controller1Gear4Pin, INPUT_PULLUP);
  pinMode(controller1GasPin, INPUT_PULLUP);

  FastLED.addLeds<WS2812, stripOnePin>(stripOne, LED_COUNT);
  
  controllerOne.setupController(controller1Gear1Pin, controller1Gear2Pin, controller1Gear3Pin, controller1Gear4Pin, 0, 0, controller1GasPin);
  
  carOne.setupCar(servoOnePin, controllerOne, lightOnePin, stripOne);

  controllerOne.waitForPlayerToResetToGearOne(carOne.lightPin);
}


unsigned long lastUpdate = 0;
const unsigned long frameInterval = 10;  // 10 ms = 100 FPS update

void loop() {
    unsigned long now = millis();

    if (now - lastUpdate >= frameInterval) {
        lastUpdate = now;

        if (currentGame.gameIsInProgress) {
            bool carOneFinished = carOne.loopCar(frameInterval / 1000.0);
            if (carOneFinished) {
                Serial.println("GAME FINISHED!");
                carOne.resetStrip(true);
                currentGame.startNewGame();
            }
        }
    }

    FastLED.show();
}

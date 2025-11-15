//#include "ServoTimer2.h"
#include<Servo.h>
#define FASTLED_ALLOW_INTERRUPTS 1


#include <FastLED.h>

const int servoOnePin = 4;
const int servoTwoPin = 11;
//const int buttonOnePin = 2;
const int LED_COUNT = 22;


const int finishLineDistance = 12000;

/////////////////CONTROLLER PINS////////////////////////////
const int controller1Gear1Pin = 40;
const int controller1Gear2Pin = 41;
const int controller1Gear3Pin = 42;
const int controller1Gear4Pin = 43;
const int controller1Gear5Pin = 44;
const int controller1Gear6Pin = 45;
const int controller1GasPin = 46;

const int lightOnePin = 47;

CRGB stripOne[LED_COUNT];
const int stripOnePin = 48;


const int controller2Gear1Pin = 22;
const int controller2Gear2Pin = 23;
const int controller2Gear3Pin = 24;
const int controller2Gear4Pin = 25;
const int controller2Gear5Pin = 26;
const int controller2Gear6Pin = 27;
const int controller2GasPin = 28;

const int lightTwoPin = 29;

CRGB stripTwo[LED_COUNT];
const int stripTwoPin = 30;

////////////////////NEOPIXELS/////////////////////////////


////STRIPS












class Controller {
public:

  unsigned long lastToggle = 0;
  bool ledState = false;
  int gasPin;
  int lightPin;
  //because our physical joystick is gonna rely on being IN a current gear, not a button,
  //we should track it in Controller, not car.
  int currentGear = 1;

  bool waitingToGetInGearOne = false;

  int gearPins[6];


  void setupController(int g1Pin, int g2Pin, int g3Pin, int g4Pin, int g5Pin, int g6Pin, int theGasPin, int theLightPin) {

    Serial.println("SET UP CONTROLLER");

    gearPins[0] = g1Pin;
    gearPins[1] = g2Pin;
    gearPins[2] = g3Pin;
    gearPins[3] = g4Pin;
    gearPins[4] = g5Pin;
    gearPins[5] = g6Pin;
    gasPin = theGasPin;
    lightPin = theLightPin;


    //gasPin = theGasPin;
  }

  int getCurrentGear() {
    //checked every frame from main car loop

    int gearShifterPosition = 0;
    //i corresponds to array value.
    for (int i = 0; i < 6; i++) {
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


  
  void waitForPlayerToResetToGearOne() {
    //Designed to be passed to EVERY FRAME
    
  
    //Serial.println("GET IN GEAR 1. NOW.");
    if (getCurrentGear() == 1) {
      Serial.println("This player is now in gear.");
      waitingToGetInGearOne = false;
    } 
    else {

    }
    unsigned long now = millis();
    if (now - lastToggle >= 500) {
        lastToggle = now;
        ledState = !ledState;
        digitalWrite(lightPin, ledState);
    }
    return;
  }
  bool getGasPedal() {

    return !digitalRead(gasPin);
  }
};


class Car {
public:

  Servo needle;
  CRGB teamColor = CRGB::Green;

  double needleIncrement = 0;
  double lastNeedleIncrement = 0;
  int currentCarGear;
  int lastCarGear = 1;

  bool burnout = false;
  int burnoutTimer = 0;

  const int SAFE_SHIFT_THRESHOLD = 90;
  const int SPEED_BOOST_THRESHOLD = 165;

  Controller *controller;

  CRGB *lightStripPointer;

  bool boost = false;

  int lightPin;

  

  double totalDistance = 0;
  double velocity = 0;

 

  void resetStrip(bool win) {
    int num = 0;
    if (win) {
      num = 255;
    }
    for (int i = 0; i < LED_COUNT; i++) {
      lightStripPointer[i] = CRGB(0, num, 0);
    }
  }

  void setupCar(int servoPin, Controller* theController, int theLightPin, CRGB *theStrip) {

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
    lastNeedleIncrement = 0;
    lastCarGear = 1;
    
    boost = false;
    burnout = false;
    burnoutTimer = 0;
    resetStrip(false);
  }

  void setLightDistance() {
    
    int ledsToShow = LED_COUNT / (finishLineDistance/totalDistance);//a ratio from 0 to 1 indicating completion.
    for (int i = 0; i < ledsToShow; i++) {
      lightStripPointer[i] = teamColor;
    }
  }

  



  bool loopCar(float dt) {

    if (controller->waitingToGetInGearOne == true) {
      //hand off the loop to the waiter function
      controller->waitForPlayerToResetToGearOne();
      return false;
    }
    
    bool neutral = false;


    currentCarGear = controller->getCurrentGear();

    bool gas = controller->getGasPedal();


    if (burnout == true) {
      //count car out of burnout.
      burnoutTimer++;
      if (burnoutTimer >= 150) {
        //out of burnout mode!!!
        burnoutTimer = 0;
        burnout = false;
        lastCarGear = 1;
        lastNeedleIncrement = 0;
        needleIncrement = 0;
        velocity = 0;
        
        //Okay, lets reset the player.
        //WE GOTTA FORCE PLAYER TO GO BACK TO GEAR 1 SOMEHOW
        controller->waitingToGetInGearOne = true;
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
        float gearFactor = pow(currentCarGear, 0.7); // 0.7 makes higher gears less punishing
        needleIncrement += (1 * (1.0/gearFactor)) * dt * 100.0;
      } else if ((needleIncrement - 0.1 * (currentCarGear / 2.0)) > 0) {
        //rpms will fall off if were in neutral OR theres no gas.
        needleIncrement -= (0.8 * (1.0/currentCarGear)) * dt * 100.0;
      } else {
        needleIncrement = needleIncrement;  //keep it the same.
      }

      velocity = (needleIncrement / 50.0) * currentCarGear; //location of needle * car gear

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

      //
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
          //Safely shifted!!!!
          //if (needleIncrement > SPEED_BOOST_THRESHOLD) {
            //Serial.println("SPEED BOOST");
            //digitalWrite(lightPin, HIGH);
            //boost = true;
          //}
          needleIncrement = currentCarGear * 10; //Dont want to reset ALL the way.
         
        
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

Car carTwo = Car();
Controller controllerTwo = Controller();




class Game {
public:
  bool gameIsInProgress = false;
  bool oneTimeResets = false;

  void startNewGame() {
    //CALLED ONCE each time a new game starts!
    //gameIsInProgress = false;
    //reset old game
    carOne.resetCar();
    carTwo.resetCar();
    controllerOne.waitingToGetInGearOne = true;
    controllerTwo.waitingToGetInGearOne = true;

    waitForPlayers();

  }

  bool waitForPlayers() {
    bool someoneIsHoldingUsUp = false;
    //Serial.println("WAITING FOR PLAYERS.....");
    
    if (controllerOne.waitingToGetInGearOne == true) {
      //hand off the loop to the waiter function
      controllerOne.waitForPlayerToResetToGearOne();
      someoneIsHoldingUsUp = true;
    }
    if (controllerTwo.waitingToGetInGearOne == true) {
      //hand off the loop to the waiter function
      controllerTwo.waitForPlayerToResetToGearOne();
      someoneIsHoldingUsUp = true;
    }
    //do the same for player two.
    if (!someoneIsHoldingUsUp) {
      //everyone is ready!
      Serial.println("EVeryone is ready for game to start.");
      return true;
    } 
    else {
      return false;
    }
    
  }

  void tryingToStartNewGame() {
    //CALLED EVERY FRAME THAT THE GAME IS NOT IN PROGRESS.
    if (oneTimeResets) {
      oneTimeResets = false;
      startNewGame();
    }
    bool ready = waitForPlayers();
    if (ready) {
      gameIsInProgress = true;
    }
    return;

  }

  bool getGameStatus() {
    return gameIsInProgress;
  }
};

Game currentGame = Game();



void setup() {
  //delay(100);
  Serial.begin(9600);
  Serial.println("begin");
  //pinMode(buttonOnePin, INPUT);
  pinMode(lightOnePin, OUTPUT);
  pinMode(controller1Gear1Pin, INPUT_PULLUP);
  pinMode(controller1Gear2Pin, INPUT_PULLUP);
  pinMode(controller1Gear3Pin, INPUT_PULLUP);
  pinMode(controller1Gear4Pin, INPUT_PULLUP);
  pinMode(controller1Gear5Pin, INPUT_PULLUP);
  pinMode(controller1Gear6Pin, INPUT_PULLUP);
  pinMode(controller1GasPin, INPUT_PULLUP);

  /////c2
  pinMode(lightTwoPin, OUTPUT);
  pinMode(controller2Gear1Pin, INPUT_PULLUP);
  pinMode(controller2Gear2Pin, INPUT_PULLUP);
  pinMode(controller2Gear3Pin, INPUT_PULLUP);
  pinMode(controller2Gear4Pin, INPUT_PULLUP);
  pinMode(controller2Gear5Pin, INPUT_PULLUP);
  pinMode(controller2Gear6Pin, INPUT_PULLUP);
  pinMode(controller2GasPin, INPUT_PULLUP);

  FastLED.addLeds<WS2812, stripOnePin, GRB>(stripOne, LED_COUNT);
  FastLED.addLeds<WS2812, stripTwoPin, GRB>(stripTwo, LED_COUNT);
  
  controllerOne.setupController(controller1Gear1Pin, controller1Gear2Pin, controller1Gear3Pin, controller1Gear4Pin, controller1Gear5Pin, controller1Gear6Pin, controller1GasPin, lightOnePin);
  controllerTwo.setupController(controller2Gear1Pin, controller2Gear2Pin, controller2Gear3Pin, controller2Gear4Pin, controller2Gear5Pin, controller2Gear6Pin, controller2GasPin, lightTwoPin);
  
  carOne.setupCar(servoOnePin, &controllerOne, lightOnePin, stripOne);
  carOne.teamColor = CRGB::Blue;
  carTwo.setupCar(servoTwoPin, &controllerTwo, lightTwoPin, stripTwo);
  carTwo.teamColor = CRGB::Red;


  controllerOne.waitingToGetInGearOne = true;
  controllerTwo.waitingToGetInGearOne = true;

  //controllerOne.waitForPlayerToResetToGearOne(carOne.lightPin);
  //currentGame.startNewGame();
}


unsigned long lastUpdate = 0;
const unsigned long frameInterval = 10;  // 10 ms = 100 FPS update

void loop() {
    unsigned long now = millis();

    if (now - lastUpdate >= frameInterval) {
        lastUpdate = now;

        if (currentGame.gameIsInProgress) { //will turn back on when game is ready.
            bool carOneFinished = carOne.loopCar(frameInterval / 1000.0);
            bool carTwoFinished = carTwo.loopCar(frameInterval / 1000.0);
            bool endGame = false;
            if (carOneFinished) {             
                carOne.resetStrip(true);
                carTwo.resetStrip(false);
                FastLED.show();
                endGame = true;                
            }
            if (carTwoFinished) {             
                carTwo.resetStrip(true);
                carOne.resetStrip(false); //loser
                FastLED.show();
                endGame = true;                
            }

            if (endGame) {
              Serial.println("GAME FINISHED!");
              delay(3000);
              currentGame.gameIsInProgress = false;
              currentGame.oneTimeResets = true;
            }
        } 
        else {
          currentGame.tryingToStartNewGame();
        }
    }

    FastLED.show();
}

// TFT eSPI lib (link for setup https://www.youtube.com/watch?v=rq5yPJbX_uk ) define pins in user_setup.h file of lib
#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>

#include <Stepper.h>

// Define number of steps per rotation and mm (+ is up, - is down):
const int stepsPerRevolution = 2048;
const int mmSteps = (10 * stepsPerRevolution) / 27;

// exposure times
const int base_time = 180000;
const int layer_time = 60000;

//number of base layers (test)
const int num_base_layers = 5;

//number of layers (test)
const int num_layers = 20;

//layer size
const float layer_size = 0.25;

//step up
const float step_up = 3;

// Stepper Wiring:
// Pin 13 to IN1 on the ULN2003 driver
// Pin 14 to IN2 on the ULN2003 driver
// Pin 12 to IN3 on the ULN2003 driver
// Pin 27 to IN4 on the ULN2003 driver
// Create stepper object called 'myStepper', note the pin order:
Stepper myStepper = Stepper(stepsPerRevolution, 13, 14, 12, 27);

//setup TFT
TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

//current test, just drawing a square, will need to read from sd card for any real design
void drawNextLayer() {
  tft.drawRect(120, 75, 100, 100, TFT_WHITE);
  tft.fillRect(120, 75, 100, 100, TFT_WHITE);
}

//moves to next layer by lifting up, and then lowering back down to correct layer height
void moveToNextLayer() {
  tft.fillScreen(TFT_BLACK);
  myStepper.step(step_up * mmSteps);
  delay(500);

  myStepper.step(int(-1 * float(step_up - layer_size) * mmSteps));
}

void setup() {
  // Set the speed to 5 rpm on stepper
  myStepper.setSpeed(5);
  
  // Begin Serial communication at a baud rate of 9600:
  Serial.begin(9600);

  // Initialize display
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  delay(500);

  //loop for base layers
  for(int i = 0; i < num_base_layers; i++) {
    drawNextLayer();
    delay(base_time);
    moveToNextLayer();
  }
}

void loop() {
  //loop for the rest of the layers
  for(int i = 0; i < num_layers; i++) {
    drawNextLayer();
    delay(layer_time);
    moveToNextLayer();
  }

  //lift up to finish
  myStepper.step(20 * mmSteps);

  while(true){;}
}

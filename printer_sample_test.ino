// TFT eSPI lib (link for setup https://www.youtube.com/watch?v=rq5yPJbX_uk ) define pins in user_setup.h file of lib
#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>
#include <Adafruit_GFX.h>

#include <SD.h>
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
// Create stepper object called 'zStep', note the pin order:
Stepper zStep = Stepper(stepsPerRevolution, 13, 14, 12, 27);

//setup TFT
TFT_eSPI tft = TFT_eSPI();       // Invoke custom library

// drawNextLayer function from https://github.com/IdleHandsProject/msla_christmas_ornament/tree/master/Firmware

void drawNextLayer(int filenumber, String folder) {
  String file = "";
  String itemNumber = String(filenumber);
  file += folder;
  file += "/";
  file += itemNumber;
  file += ".bmp";
  char charBuf[20];
  file.toCharArray(charBuf, 20);
  bmpDraw(charBuf, 0, 0);
}

//moves to next layer by lifting up, and then lowering back down to correct layer height
void moveToNextLayer() {
  tft.fillScreen(TFT_BLACK);
  zStep.step(step_up * mmSteps);
  delay(500);

  zStep.step(int(-1 * float(step_up - layer_size) * mmSteps));
}

void setup() {
  // Set the speed to 5 rpm on stepper
  zStep.setSpeed(5);
  
  // Begin Serial communication at a baud rate of 9600:
  Serial.begin(9600);

  // Initialize display
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  delay(500);

  //loop for base layers
  for(int i = 0; i < num_base_layers; i++) {
    drawNextLayer(i+1, "/printFiles/");
    delay(base_time);
    moveToNextLayer();
  }
}

void loop() {
  //loop for the rest of the layers
  for(int i = 0; i < num_layers; i++) {
    drawNextLayer(i+1, "/printFiles/");
    delay(layer_time);
    moveToNextLayer();
  }

  //lift up to finish
  zStep.step(20 * mmSteps);
  while(true){;}
}

// bmpDraw function from https://github.com/IdleHandsProject/msla_christmas_ornament/tree/master/Firmware

#define BUFFPIXEL 60

void bmpDraw(char *filename, uint8_t x, uint16_t y) {

  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3 * BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();

  if ((x >= tft.width()) || (y >= tft.height())) return;

  Serial.println();
  Serial.print(F("Loading image '"));
  Serial.print(filename);
  Serial.println('\'');

  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    Serial.print(F("File not found"));
    Serial.println("Print Finished");
    return;
  }

  // Parse BMP header
  if (read16(bmpFile) == 0x4D42) { // BMP signature
    Serial.print(F("File size: ")); Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print(F("Image Offset: ")); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print(F("Header size: ")); Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if (read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print(F("Bit Depth: ")); Serial.println(bmpDepth);
      if ((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        Serial.print(F("Image size: "));
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if (bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if ((x + w - 1) >= tft.width())  w = tft.width()  - x;
        if ((y + h - 1) >= tft.height()) h = tft.height() - y;

        // Set TFT address window to clipped image bounds
        tft.startWrite();
        tft.setAddrWindow(x, y, w, h);

        for (row = 0; row < h; row++) { // For each scanline...

          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if (flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if (bmpFile.position() != pos) { // Need seek?
            tft.endWrite();
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          for (col = 0; col < w; col++) { // For each pixel...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
              tft.startWrite();
            }

            // Convert pixel from BMP to TFT format, push to display
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            tft.pushColor(tft.color565(r, g, b));
          } // end pixel
        } // end scanline
        tft.endWrite();
        Serial.print(F("Loaded in "));
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      } // end goodBmp
    }
  }

  bmpFile.close();
  if (!goodBmp) Serial.println(F("BMP format not recognized."));
}


// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

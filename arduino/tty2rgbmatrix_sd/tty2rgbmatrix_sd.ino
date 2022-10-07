///////////////////////////////////////////////////////////////
/* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
* You can download the latest version of this code from:
* https://github.com/h3llb3nt/tty2rgbmatrix
*///////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////
/* tty2rgbmatrix sdcard edition 2022/09/18
 * loads gif files from an sdcard and play them on an rgb matrix based on serial input from MiSTer fpga
 * those code is know to work with arduino IDE 1.8.19 with the ESP32 package version 2.0.4 installed */
//////////////////////////////////////////////////////////////

// versions of the libraries used are commented below
// use other versions at your own peril
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>  //v2.0.7 by mrfaptastic verifed to work
#include <AnimatedGIF.h>                      //v1.4.7 by larry bank verifed to work

#define FILESYSTEM SD
#include "SD.h"                               //v1.2.4
#include "SPI.h"

// Micro SD Card Module Pinout                // these pins below are known to work with this config on esp32 trinity boards by brian lough
//sd  to tri
//3v3 to 3v3
//gnd to gnd
//clk to 33
//do  to 32
//di  to sda
//cs  to scl

//#define HSPI_MISO 32
//#define HSPI_MOSI 21 //trinity pin labeled SDA
//#define HSPI_SCLK 33 
//#define HSPI_CS 22   //trinity pin labeled SCL

//my pin setup so my sdcard adapter (adafruit sdcard adapter) can just slot into the pin headers on the trinity board
#define HSPI_MISO 21 //trinity pin labeled SDA
#define HSPI_MOSI 32 
#define HSPI_SCLK 22 //trinity pin labeled SCL
#define HSPI_CS 33   

SPIClass *spi = NULL;

// ----------------- RGB MATRIX CONFIG ----------------- 
// more panel setup is found in the void setup() function!

const int panelResX = 64;        // Number of pixels wide of each INDIVIDUAL panel module.
const int panelResY = 32;        // Number of pixels tall of each INDIVIDUAL panel module.
const int panels_in_X_chain = 2; // Total number of panels in X
const int panels_in_Y_chain = 1; // Total number of panels in Y

const int totalWidth  = panelResX * panels_in_X_chain;  //used in span function
const int totalHeight = panelResY * panels_in_Y_chain;  //used in span function

MatrixPanel_I2S_DMA *dma_display = nullptr;

uint16_t myBLACK = dma_display->color565(0, 0, 0);
uint16_t myWHITE = dma_display->color565(255, 255, 255);
uint16_t myRED = dma_display->color565(255, 0, 0);
uint16_t myGREEN = dma_display->color565(0, 255, 0);
uint16_t myBLUE = dma_display->color565(0, 0, 255);


// ----------------- STRING READ CONFIG ----------
String newCORE = "";             // Received Text, from MiSTer without "\n\r" currently (2021-01-11)
String currentCORE = "";         // Buffer String for Text change detection
char newCOREArray[30]="";        // Array of char needed for some functions, see below "newCORE.toCharArray"
String gifPath ="";
String subFolder ="";
char chosenGIF[256] = { 0 };


// ----------------- ANIMATEDGIF LIBRARY STUFF -----------
AnimatedGIF gif;
bool animated_flag;
File f;
int x_offset, y_offset;
int16_t xPos = 0, yPos = 0; // Top-left pixel coord of GIF in matrix space


// ----------------- GIF DRAW Gif Draw Functions -------------------------

// Copy a horizontal span of pixels from a source buffer to an X,Y position
// in matrix back buffer, applying horizontal clipping. Vertical clipping is
// handled in GIFDraw() below -- y can safely be assumed valid here.
void span(uint16_t *src, int16_t x, int16_t y, int16_t width) {
  if (x >= totalWidth) return;     // Span entirely off right of matrix
  int16_t x2 = x + width - 1;      // Rightmost pixel
  if (x2 < 0) return;              // Span entirely off left of matrix
  if (x < 0) {                     // Span partially off left of matrix
    width += x;                    // Decrease span width
    src -= x;                      // Increment source pointer to new start
    x = 0;                         // Leftmost pixel is first column
  }
  if (x2 >= totalWidth) {      // Span partially off right of matrix
    width -= (x2 - totalWidth + 1);
  }
  while(x <= x2) {
    dma_display->drawPixel(x++, y, *src++);
  } 
} /* void span() */

// Draw a line of image directly on the LCD
void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *s;
  uint16_t *d, *usPalette, usTemp[320];
  int x, y;

  y = pDraw->iY + pDraw->y; // current line

  // Vertical clip
  int16_t screenY = yPos + y; // current row on matrix
  if ((screenY < 0) || (screenY >= totalHeight)) return;

  usPalette = pDraw->pPalette;

  s = pDraw->pPixels;
  
  // Apply the new pixels to the main image
  if (pDraw->ucHasTransparency)   // if transparency used
  {
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    int x, iCount;
    pEnd = s + pDraw->iWidth;
    x = 0;
    iCount = 0;                   // count non-transparent pixels
    while (x < pDraw->iWidth) 
    {
      c = ucTransparent - 1;
      d = usTemp;
      while (c != ucTransparent && s < pEnd) 
      {
        c = *s++;
        if (c == ucTransparent)   // done, stop
        { 
          s--;                    // back up to treat it like transparent
        } 
        else                      // opaque
        {                 
          *d++ = usPalette[c];
          iCount++;
        }
      }                           // while looking for opaque pixels
      if (iCount)                 // any opaque pixels?
      {               
        span(usTemp, xPos + pDraw->iX + x, screenY, iCount);
        x += iCount;
        iCount = 0;
      }
      
      // no, look for a run of transparent pixels
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd) 
      {
        c = *s++;
        if (c == ucTransparent)
          iCount++;
        else
          s--;
      }
      if (iCount) 
      {
        x += iCount;              // skip these
        iCount = 0;
      }
    }
  } 
  else                            //does not have transparency
  {                         
    s = pDraw->pPixels;
    // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
    for (x = 0; x < pDraw->iWidth; x++) 
      usTemp[x] = usPalette[*s++];
    span(usTemp, xPos + pDraw->iX, screenY, pDraw->iWidth);
  }
} /* GIFDraw() */

void * GIFOpenFile(const char *fname, int32_t *pSize)
{
  Serial.print("Playing gif: ");
  Serial.println(fname);
  f = FILESYSTEM.open(fname);
  if (f)
  {
    *pSize = f.size();
    return (void *)&f;
  }
  return NULL;
} /* GIFOpenFile() */

void GIFCloseFile(void *pHandle)
{
  File *f = static_cast<File *>(pHandle);
  if (f != NULL)
     f->close();
} /* GIFCloseFile() */

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
    int32_t iBytesRead;
    iBytesRead = iLen;
    File *f = static_cast<File *>(pFile->fHandle);
    // Note: If you read a file all the way to the last byte, seek() stops working
    if ((pFile->iSize - pFile->iPos) < iLen)
       iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
    if (iBytesRead <= 0)
       return 0;
    iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
    pFile->iPos = f->position();
    return iBytesRead;
} /* GIFReadFile() */

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition)
{ 
  int i = micros();
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  i = micros() - i;
  //Serial.printf("Seek time = %d us\n", i);
  return pFile->iPos;
} /* GIFSeekFile() */

unsigned long start_tick = 0;

void ShowGIF(char *name, bool animated)
{
  start_tick = millis();
   
  if (gif.open(name, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw))
  {
    x_offset = (MATRIX_WIDTH - gif.getCanvasWidth()) / 2;
    if (x_offset < 0) x_offset = 0;
    y_offset = (MATRIX_HEIGHT - gif.getCanvasHeight()) / 2;
    if (y_offset < 0) y_offset = 0;
    Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
    Serial.flush();
    if (animated) 
    {
      Serial.println("animated gif flag found, playing whole gif");
      while(gif.playFrame(true, NULL))
      {
        //keep on playing unless...
        if (Serial.available()) 
        {
          newCORE = Serial.readStringUntil('\n');                  // Read string from serial until NewLine "\n" (from MiSTer's echo command) is detected or timeout (1000ms) happens.
          Serial.println(newCORE);
          if (newCORE!=currentCORE) 
          {
            break;
          }
        }
      }
    }
    else 
    {
      Serial.println("static gif flag found, playing 1st frame of gif");
      while (!gif.playFrame(true, NULL))
      { // leaving this break in here incase i need it for interrupting the the current playing gif in a future rev     
        if ( (millis() - start_tick) > 60000) // play first frame of non-animated gif and wait 10 seconds
        { 
          //Serial.println("times up! breaking from play loop!");
          break;
        }
        else
        {
          Serial.print(".");
          if (Serial.available())
          {
            newCORE = Serial.readStringUntil('\n');  // Read string from serial until NewLine "\n" (from MiSTer's echo command) is detected or timeout (1000ms) happens.
            Serial.println(newCORE);
            if (newCORE!=currentCORE)
            {
              break;
            }
          }
          gif.reset();
        }
      }
    }
    Serial.println("closing gif file");
    gif.close();
  }

} /* ShowGIF() */

void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels -1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

//String gifDir = "/"; // play all GIFs in this directory on the SD card
//char filePath[256] = { 0 };
//File root, gifFile;
  
void setup() {
  Serial.begin(115200);
  Serial.println();
  
  //Mount SD Card, display some info and list files in /gifs folder to serial output
  Serial.println("Micro SD Card Mounting...");

  spi = new SPIClass(HSPI);
  spi->begin(HSPI_SCLK, HSPI_MISO, HSPI_MOSI, HSPI_CS);

  SD.begin(HSPI_CS, *spi);
  delay(500);

  if (!SD.begin(HSPI_CS, *spi)) {
    Serial.println("Card Mount Failed");
    return;
  }
  
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");

  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
  // list files in /gifs folder to serial output
  listDir(SD, "/", 2); 
  Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));

  // initialize gif object
  gif.begin(LITTLE_ENDIAN_PIXELS);

  // start display
  HUB75_I2S_CFG mxconfig(
    panelResX,           // module width
    panelResY,           // module height
    panels_in_X_chain    // Chain length
  );

  // If you are using a 64x64 matrix you need to pass a value for the E pin
  // The trinity connects GPIO 18 to the typical pin E.
  // This can be commented out for any smaller displays (but should work fine with it)
  //mxconfig.gpio.e = 18;

  //swap green and blue for my specific rgb panels
  mxconfig.gpio.b1 = 26;
  mxconfig.gpio.b2 = 12;

  mxconfig.gpio.g1 = 27;
  mxconfig.gpio.g2 = 13;

  // May or may not be needed depending on your matrix
  // Example of what needing it looks like:
  // https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/134#issuecomment-866367216
  //mxconfig.clkphase = false;

  // Some matrix panels use different ICs for driving them and some of them have strange quirks.
  // If the display is not working right, try this.
  //mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(128); //0-255
  dma_display->clearScreen();
  
  //screen startup test (watch for dead or misfiring pixels)
  dma_display->fillScreen(myBLACK);
  delay(500);
  dma_display->fillScreen(myRED);
  delay(500);
  dma_display->fillScreen(myGREEN);
  delay(500);
  dma_display->fillScreen(myBLUE);
  delay(500);
  dma_display->fillScreen(myWHITE);
  delay(500);
  dma_display->clearScreen();
  delay(500);
  
  dma_display->setCursor(0, 0);
  dma_display->println("MiSTer FPGA");
  delay(1000);

  dma_display->fillScreen(myBLACK);
  
  // setup initial core to default to menu
  currentCORE = "NULL";
  newCORE = "MENU";
} /* void setup() */


void loop() {
  
  if (Serial.available()) {
    newCORE = Serial.readStringUntil('\n');                  // Read string from serial until NewLine "\n" (from MiSTer's echo command) is detected or timeout (1000ms) happens.
    delay(10);
    Serial.printf("%s is oldcore, %s is newcore\n", String(currentCORE), String(newCORE));
  }
    
  if (newCORE!=currentCORE)           // Proceed only if Core Name changed
  {                                    
    Serial.printf("Running a check because %s is oldcore, %s is newcore\n", String(currentCORE), String(newCORE));
    //Serial.printf("setting animated flag to 1 since we assume animated");
    animated_flag = true;                                   // i assume animated gif is what you want to we default to that
    // -- First Transmission --
    if (newCORE.endsWith("QWERTZ"));  // TESTING: Process first Transmission after PowerOn/Reboot.                 
    
    // -- Menu Core --
    else if (newCORE=="MENU")         {strcpy(chosenGIF, "/animated/M/menu.gif"); }
    //else if (newCORE=="hellbent")     {strcpy(chosenGIF, "/animated/H/h3llb3nt.gif"); animated_flag=!animated_flag; }
    else if (newCORE=="error")        {strcpy(chosenGIF, "/animated/E/error.gif"); }
        
    // -- Test Commands --
    else if (newCORE=="cls")          ;//do something
    else if (newCORE=="sorg")         ;//do something
    else if (newCORE=="bye")          ;//do something

    else 
    {
      subFolder = String(newCORE[0]); //first letter of core is the subfolder to look in
      subFolder.toUpperCase();        //subfolders are capital, and numbers don't care about this function
      gifPath = String("/animated/" + subFolder + "/" + newCORE + ".gif");    //animated gif path string creation "/animated/X/x.gif"
      //Serial.print("animated gif path string is: ");Serial.println(gifPath);
  
      gifPath.toCharArray(chosenGIF,gifPath.length() +1);                     //sd.exists wants a character array, not a string...
  
      //Serial.print("char array chosenGIF is: ");Serial.println(chosenGIF);
      //Serial.printf("checking if animated image at %s exists", chosenGIF);Serial.println();
      if (SD.exists(chosenGIF))                                               //check if path for animated file is extant or not! if so great!
      {
        Serial.printf("char array chosenGIF %s exists!", chosenGIF);Serial.println();
      }
      else                                                                    //animated gif path did not return a 1, so file does not exist, see if a static image is there
      {
        //Serial.printf("%s did not exist, checking if a static image does", chosenGIF);Serial.println();
        gifPath = String("/static/" + subFolder + "/" + newCORE + ".gif");    //static gif path string creation "/static/X/x.gif"
        //Serial.print("static gif path string is: ");Serial.println(gifPath);
        
        gifPath.toCharArray(chosenGIF,gifPath.length() +1);                   //sd.exists wants a character array, not a string...
        
        //Serial.print("char array chosenGIF is: ");Serial.println(chosenGIF);
        //Serial.printf("checking if static image at %s exists", chosenGIF);Serial.println();
        if (SD.exists(chosenGIF))                                             //check if path for static file is extant or not! if so great!
        {
          //Serial.println("checked if static image exists...");
          //Serial.printf("char array chosenGIF %s exists!", chosenGIF);Serial.println();
          animated_flag = false;                                              //set static image boolean so gifdraw function knows its a single frame
        }
        else                                                                  //static gif path did not return a 1 either, so no file at all.
        {
          //Serial.println("checked if static image exists...");
          //Serial.printf("no animated or static file exists for newCORE: %s", newCORE);Serial.println();
          strcpy(chosenGIF, "no-match");                                      
          newCORE.toCharArray(newCOREArray, newCORE.length()+1);
        }
      }
    } // end newCORE!=currentCORE
  }
  currentCORE=newCORE;  // Update Buffer

  //no core change, show the current currentCORE gif
  //Serial.printf("%s is oldcore, %s is newcore, %s is chosenGIF\n", String(currentCORE), String(newCORE), chosenGIF);

  if (strcmp(chosenGIF,"no-match")) 
  {
    if (SD.exists(chosenGIF)) // show gif!
    {
      ShowGIF(chosenGIF,animated_flag);
    }
    else //gif not found
    {
      Serial.printf("IMAGE FILE %s NOT FOUND!\n", chosenGIF);
      dma_display->clearScreen();
      dma_display->setCursor(0, 0);
      dma_display->print(chosenGIF);
      dma_display->println(" not found");
      delay(3000);
    }
  } 
  else // show text on display of current gif
  {
    dma_display->clearScreen();
    dma_display->setCursor(0, 0);
    dma_display->println(newCOREArray);
    delay(3000);
  }  
} /* void loop() */
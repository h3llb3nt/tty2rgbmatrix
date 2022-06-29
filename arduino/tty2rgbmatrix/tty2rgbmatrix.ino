/*******************************************************************
    ESP32 Trinity - https://github.com/witnessmenow/ESP32-Trinity
 *******************************************************************/

// ----------------------------
// Additional Libraries - each one of these will need to be installed.
// ----------------------------

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

// This is the library for interfacing with the display

// Can be installed from the library manager (Search for "ESP32 MATRIX DMA")
// https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA

#include <AnimatedGIF.h>

// Library for decoding GIFs on Arduino

// Search for "AnimatedGIF" in the Arduino Library manager
// https://github.com/bitbank2/AnimatedGIF

// ----------------------------
// Dependency Libraries - each one of these will need to be installed.
// ----------------------------

// Adafruit GFX library is a dependency for the matrix Library
// Can be installed from the library manager
// https://github.com/adafruit/Adafruit-GFX-Library


// Library for accessing SPIFFS storage on Arduino
#define FILESYSTEM SPIFFS
#include <SPIFFS.h>

// -------------------------------------
// -------   Matrix Config   ------
// -------------------------------------

const int panelResX = 64;        // Number of pixels wide of each INDIVIDUAL panel module.
const int panelResY = 32;        // Number of pixels tall of each INDIVIDUAL panel module.
const int panels_in_X_chain = 2; // Total number of panels in X
const int panels_in_Y_chain = 1; // Total number of panels in Y

const int totalWidth  = panelResX * panels_in_X_chain;
const int totalHeight = panelResY * panels_in_Y_chain;

MatrixPanel_I2S_DMA *dma_display = nullptr;

// See the "displaySetup" method for more display config options

// -------------------------------------
// ------- String read config ----------
// -------------------------------------

// Strings

String newCORE = "";             // Received Text, from MiSTer without "\n\r" currently (2021-01-11)
String currentCORE = "";             // Buffer String for Text change detection
char newCOREArray[30]="";        // Array of char needed for some functions, see below "newCORE.toCharArray"

//------------------------------------------------------------------------------------------------------------------


// ANIMATEDGIF LIBRARY STUFF -----------------------------------------------

AnimatedGIF gif;
File f;
int16_t xPos = 0, yPos = 0; // Top-left pixel coord of GIF in matrix space

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
  if (pDraw->ucHasTransparency) { // if transparency used
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    int x, iCount;
    pEnd = s + pDraw->iWidth;
    x = 0;
    iCount = 0;                   // count non-transparent pixels
    while (x < pDraw->iWidth) {
      c = ucTransparent - 1;
      d = usTemp;
      while (c != ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent) { // done, stop
          s--;                    // back up to treat it like transparent
        } else {                  // opaque
          *d++ = usPalette[c];
          iCount++;
        }
      }                           // while looking for opaque pixels
      if (iCount) {               // any opaque pixels?
        span(usTemp, xPos + pDraw->iX + x, screenY, iCount);
        x += iCount;
        iCount = 0;
      }
      // no, look for a run of transparent pixels
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd) {
        c = *s++;
        if (c == ucTransparent)
          iCount++;
        else
          s--;
      }
      if (iCount) {
        x += iCount; // skip these
        iCount = 0;
      }
    }
  } else {                         //does not have transparency
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
//  Serial.printf("Seek time = %d us\n", i);
  return pFile->iPos;
} /* GIFSeekFile() */

unsigned long start_tick = 0;

void ShowGIF(char *name)
{
  start_tick = millis();
   
  if (gif.open(name, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw))
  {
    Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
    Serial.flush();
    while (gif.playFrame(true, NULL))
    {      
      // if ( (millis() - start_tick) > 8000) { // we'll get bored after about 8 seconds of the same looping gif
      //  break;
      // }
    }
    gif.close();
  }

} /* ShowGIF() */

void displaySetup() {
  HUB75_I2S_CFG mxconfig(
    panelResX,           // module width
    panelResY,           // module height
    panels_in_X_chain    // Chain length
  );

  // If you are using a 64x64 matrix you need to pass a value for the E pin
  // The trinity connects GPIO 18 to E.
  // This can be commented out for any smaller displays (but should work fine with it)
  mxconfig.gpio.e = 18;

  //swap green and blue for my specific rgb panels
  mxconfig.gpio.b1 = 26;
  mxconfig.gpio.b2 = 12;

  mxconfig.gpio.g1 = 27;
  mxconfig.gpio.g2 = 13;

  // May or may not be needed depending on your matrix
  // Example of what needing it looks like:
  // https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/issues/134#issuecomment-866367216
  mxconfig.clkphase = false;

  // Some matrix panels use different ICs for driving them and some of them have strange quirks.
  // If the display is not working right, try this.
  //mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(128); //0-255
  dma_display->clearScreen();
} /* displaySetup() */

String gifDir = "/gifs"; // play all GIFs in this directory on the SD card
char filePath[256] = { 0 };
File root, gifFile;
char choosenGIF[256] = { 0 };
  
void setup() {
  Serial.begin(115200);
  Serial.println();   

  // Start filesystem
  Serial.println(" * Loading SPIFFS");
  if(!SPIFFS.begin()){
        Serial.println("SPIFFS Mount Failed");
  }

  

  // initialize gif object
  gif.begin(LITTLE_ENDIAN_PIXELS);

  // start display
  displaySetup();

  dma_display->setCursor(0, 0);
  dma_display->println("MiSTer FPGA");
  
  // setup initial core to default to menu
  currentCORE = "NULL";
  newCORE = "MENU";
} /* void setup() */


void loop() {
  // put your main code here, to run repeatedly:

  if (Serial.available()) {
    newCORE = Serial.readStringUntil('\n');                  // Read string from serial until NewLine "\n" (from MiSTer's echo command) is detected or timeout (1000ms) happens.
    delay(10);
    Serial.printf("%s is oldcore, %s is newcore\n", String(currentCORE), String(newCORE));
  }
    
  if (newCORE!=currentCORE) {                                    // Proceed only if Core Name changed
    Serial.printf("Running a check because %s is oldcore, %s is newcore\n", String(currentCORE), String(newCORE));
    // -- First Transmission --
    if (newCORE.endsWith("QWERTZ"))   ;                      // TESTING: Process first Transmission after PowerOn/Reboot.                 
    
    // -- Menu Core --
    else if (newCORE=="MENU")       {Serial.println("read MENU");       strcpy(choosenGIF, "/gifs/mister_logo_a.gif"); }
    
    // -- Arcade Cores with images--
    else if (newCORE=="1942")         {Serial.println("read 1942s");     strcpy(choosenGIF, "/gifs/1942.gif"); }
    else if (newCORE=="atetris")      {Serial.println("read atetris");   strcpy(choosenGIF, "/gifs/Tetris_ATARI.gif"); }
    else if (newCORE=="centiped")     {Serial.println("read centipede"); strcpy(choosenGIF, "/gifs/centipede.gif"); }
    else if (newCORE=="centiped3")    {Serial.println("read centipede"); strcpy(choosenGIF, "/gifs/centipede.gif"); }
    else if (newCORE=="dkong")        {Serial.println("read dkong");     strcpy(choosenGIF, "/gifs/donkeykong.gif"); }
    else if (newCORE=="digdug")       {Serial.println("read digdug");    strcpy(choosenGIF, "/gifs/digdug.gif"); }
    else if (newCORE=="galagamw")     {Serial.println("read galaga");    strcpy(choosenGIF, "/gifs/galaga.gif"); }
    else if (newCORE=="mario")        {Serial.println("read mario");    strcpy(choosenGIF, "/gifs/mariobros.gif"); }
    else if (newCORE=="sinistar")     {Serial.println("read sinistar");  strcpy(choosenGIF, "/gifs/sinistar.gif"); }
    else if (newCORE=="spyhunt")      {Serial.println("read spyhunter"); strcpy(choosenGIF, "/gifs/spyhunter.gif"); }
    else if (newCORE=="tapper")       {Serial.println("read tapper");    strcpy(choosenGIF, "/gifs/tapper.gif"); }
    else if (newCORE=="zaxxon")       {Serial.println("read zaxxon");    strcpy(choosenGIF, "/gifs/zaxxon.gif"); }

    // -- Computer Cores --
    else if (newCORE=="AcornAtom")    ;//do something
    else if (newCORE=="AO486")        ;//do something
    else if (newCORE=="APPLE-I")      ;//do something
    else if (newCORE=="Apple-II")     ;//do something
    else if (newCORE=="ARCHIE")       ;//do something
    else if (newCORE=="AtariST")      ;//do something
    else if (newCORE=="ATARI800")     ;//do something
    else if (newCORE=="C64")          ;//do something
    else if (newCORE=="Minimig")      ;//do something
    else if (newCORE=="MSX")          ;//do something
    else if (newCORE=="PET2001")      ;//do something
    else if (newCORE=="VIC20")        ;//do something

    // -- Console Cores --
    else if (newCORE=="ATARI2600")    ;//do something
    else if (newCORE=="ATARI5200")    ;//do something
    else if (newCORE=="ATARI7800")    ;//do something
    else if (newCORE=="AtariLynx")    ;//do something
    else if (newCORE=="Astrocade")    ;//do something
    else if (newCORE=="ChannelF")     ;//do something
    else if (newCORE=="Coleco")       ;//do something
    else if (newCORE=="GAMEBOY")      ;//do something
    else if (newCORE=="GBA")          ;//do something
    else if (newCORE=="Genesis")      {Serial.println("read Genesis");    strcpy(choosenGIF, "/gifs/sega.gif"); }
    else if (newCORE=="MEGACD")       ;//do something
    else if (newCORE=="NEOGEO")       ;//do something
    else if (newCORE=="NES")          ;//do something
    else if (newCORE=="ODYSSEY")      ;//do something
    else if (newCORE=="ODYSSEY")      ;//do something
    else if (newCORE=="Playstation")  ;//do something
    else if (newCORE=="SMS")          ;//do something
    else if (newCORE=="SNES")         ;//do something
    else if (newCORE=="TGFX16")       ;//do something
    else if (newCORE=="VECTREX")      ;//do something

    // -- Other Cores --
    else if (newCORE=="Chess")        ;//do something

    // -- Other --
    else if (newCORE=="MEMTEST")      ;//do something
    
    // -- Test Commands --
    else if (newCORE=="cls")          ;//do something
    else if (newCORE=="sorg")         ;//do something
    else if (newCORE=="bye")          ;//do something
    
    // -- Unidentified Core Name --
    else {
      strcpy(choosenGIF, "no-match");
      newCORE.toCharArray(newCOREArray, newCORE.length()+1);
    }  
    
  } // end newCORE!=currentCORE

  currentCORE=newCORE;  // Update Buffer

  //no core change, show the current currentCORE gif
  Serial.printf("%s is oldcore, %s is newcore, %s is choosenGIF\n", String(currentCORE), String(newCORE), choosenGIF);
  /*
  if (gif.open((uint8_t *)spyhunter, sizeof(spyhunter), GIFDraw)) {
    while (gif.playFrame(true, NULL))      
    gif.close();
  }
  */
  if (strcmp(choosenGIF,"no-match")) {
    ShowGIF(choosenGIF);
  } else {
    dma_display->clearScreen();
    dma_display->setCursor(0, 0);
    dma_display->println(newCOREArray);
    delay(3000);
  }

  /*
  newCORE.toCharArray(newCOREArray, newCORE.length()+1);
  dma_display->clearScreen();
  dma_display->setCursor(0, 0);
  dma_display->println(newCOREArray);
  delay(3000);  
  */
  
  
} /* void loop() */

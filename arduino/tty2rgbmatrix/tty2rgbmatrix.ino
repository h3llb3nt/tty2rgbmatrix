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
        if ( (millis() - start_tick) > 10000) // play first frame of non-animated gif and wait 10 seconds
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
  if (!root) {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
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

String gifDir = "/gifs"; // play all GIFs in this directory on the SD card
char filePath[256] = { 0 };
File root, gifFile;
char chosenGIF[256] = { 0 };
  
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
  Serial.println();

  // list files in /gifs folder to serial output
  listDir(SD, "/gifs", 0);
  listDir(SD, "/gifs/elluigi", 0);
  listDir(SD, "/gifs/pixelcade", 0); 

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
  
  gif.begin(LITTLE_ENDIAN_PIXELS);
  
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
    Serial.printf("setting animated flag to 1 since we assume animated");
    animated_flag = true;
    // -- First Transmission --
    if (newCORE.endsWith("QWERTZ"));  // TESTING: Process first Transmission after PowerOn/Reboot.                 
    
    // -- Menu Core --
    else if (newCORE=="MENU")         {Serial.println("read MENU");       strcpy(chosenGIF, "/animated/M/menu.gif"); }
    else if (newCORE=="hellbent")     {Serial.println("read hellbent");   strcpy(chosenGIF, "/static/H/h3llb3nt.gif"); animated_flag=!animated_flag; }
    else if (newCORE=="error")        {Serial.println("read error");      strcpy(chosenGIF, "/animated/error.gif"); } //this file doesn't exist on purpose to show error handling on screen
        
    // -- Arcade Cores with images by h3llb3nt--
    else if (newCORE=="1942")         {Serial.println("read 1942");      strcpy(chosenGIF, "/animated/1/1942.gif"); }
    else if (newCORE=="atetris")      {Serial.println("read atetris");   strcpy(chosenGIF, "/animated/A/atetris.gif"); }
    else if (newCORE=="blktiger")     {Serial.println("read blktiger");  strcpy(chosenGIF, "/animated/B/blktiger.gif"); }
    else if (newCORE=="btime")        {Serial.println("read btime");     strcpy(chosenGIF, "/animated/B/btime.gif"); }
    else if (newCORE=="centiped")     {Serial.println("read centipede"); strcpy(chosenGIF, "/animated/C/centiped.gif"); }
    else if (newCORE=="centiped3")    {Serial.println("read centipede"); strcpy(chosenGIF, "/animated/C/centiped.gif"); }
    else if (newCORE=="defender")     {Serial.println("read defender");  strcpy(chosenGIF, "/animated/D/defender.gif"); }
    else if (newCORE=="dkong")        {Serial.println("read dkong");     strcpy(chosenGIF, "/animated/D/dkong.gif"); }
    else if (newCORE=="digdug")       {Serial.println("read digdug");    strcpy(chosenGIF, "/animated/D/digdug.gif"); }
    else if (newCORE=="galagamw")     {Serial.println("read galaga");    strcpy(chosenGIF, "/animated/G/galagamw.gif"); }
    else if (newCORE=="mario")        {Serial.println("read mario");     strcpy(chosenGIF, "/animated/M/mario.gif"); }
    else if (newCORE=="mrdo")         {Serial.println("read mrdo");      strcpy(chosenGIF, "/animated/M/mrdo.gif"); }
    else if (newCORE=="sinistar")     {Serial.println("read sinistar");  strcpy(chosenGIF, "/animated/S/sinistar.gif"); }
    else if (newCORE=="spyhunt")      {Serial.println("read spyhunt");   strcpy(chosenGIF, "/animated/S/spyhunt.gif"); }
    else if (newCORE=="tapper")       {Serial.println("read tapper");    strcpy(chosenGIF, "/animated/T/tapper.gif"); }
    else if (newCORE=="zaxxon")       {Serial.println("read zaxxon");    strcpy(chosenGIF, "/animated/Z/zaxxon.gif"); }
    else if (newCORE=="arkanoid")     {Serial.println("read arkanoid");  strcpy(chosenGIF, "/animated/A/arkanoid.gif"); }

    // -- Arcade Cores with images by eLLuigi
    else if (newCORE=="aliensyn")     {Serial.println("read aliensyn");    strcpy(chosenGIF, "/animated/A/aliensyn.gif"); }
    else if (newCORE=="avsp")         {Serial.println("read avsp");        strcpy(chosenGIF, "/animated/A/avsp.gif"); }
    else if (newCORE=="altbeast")     {Serial.println("read altbeast");    strcpy(chosenGIF, "/animated/A/altbeast.gif"); }
    else if (newCORE=="ddragon")      {Serial.println("read ddragon");     strcpy(chosenGIF, "/animated/D/ddragon.gif"); }
    else if (newCORE=="dstlk")        {Serial.println("read dstlk");       strcpy(chosenGIF, "/animated/D/dstlk.gif"); }
    else if (newCORE=="pacman")       {Serial.println("read pacman");      strcpy(chosenGIF, "/animated/P/pacman.gif"); }
    else if (newCORE=="pengo")        {Serial.println("read pengo");       strcpy(chosenGIF, "/animated/P/pengo.gif"); }
    else if (newCORE=="roadf")        {Serial.println("read roadf");       strcpy(chosenGIF, "/animated/R/roadf.gif"); }
    else if (newCORE=="rtype")        {Serial.println("read rtype");       strcpy(chosenGIF, "/animated/R/rtype.gif"); }
    else if (newCORE=="rygar")        {Serial.println("read rygar");       strcpy(chosenGIF, "/animated/R/rygar.gif"); }
    else if (newCORE=="shinobi")      {Serial.println("read shinobi");     strcpy(chosenGIF, "/animated/S/shinobi.gif"); }
    else if (newCORE=="sf")           {Serial.println("read sf");          strcpy(chosenGIF, "/animated/S/sf.gif"); }
    else if (newCORE=="shangon")      {Serial.println("read superhangon"); strcpy(chosenGIF, "/animated/S/shangon.gif"); }
    else if (newCORE=="tigeroad")     {Serial.println("read tigeroad");    strcpy(chosenGIF, "/animated/T/tigeroad.gif"); }
    else if (newCORE=="timeplt")      {Serial.println("read timeplt");     strcpy(chosenGIF, "/animated/T/timeplt.gif"); }
    else if (newCORE=="vigilant")     {Serial.println("read vigilant");    strcpy(chosenGIF, "/animated/V/vigilant.gif"); }
    else if (newCORE=="willow")       {Serial.println("read willow");      strcpy(chosenGIF, "/animated/W/willow.gif"); }
    else if (newCORE=="contra")       {Serial.println("read contra");      strcpy(chosenGIF, "/animated/C/contra.gif"); }
    
    // NEOGEO Cores with images by eLLuigi
    else if (newCORE=="MetalSlug")      {Serial.println("read MetalSlug");       strcpy(chosenGIF, "/animated/M/mslug.gif"); }
    else if (newCORE=="NeoTurfMasters") {Serial.println("read NeoTurfMasters");  strcpy(chosenGIF, "/animated/T/turfmast.gif"); }

    // -- Arcade Cores with PixelCade images
    else if (newCORE=="baddudes")       {Serial.println("read baddudes");   strcpy(chosenGIF, "/animated/B/badduds.gif"); }
    else if (newCORE=="dkongjr")        {Serial.println("read dkongjr");    strcpy(chosenGIF, "/animated/D/dkongjr.gif"); }
    else if (newCORE=="gunsmoke")       {Serial.println("read gunsmoke");   strcpy(chosenGIF, "/animated/G/gunsmoke.gif"); }
    else if (newCORE=="punisher")       {Serial.println("read punisher");   strcpy(chosenGIF, "/animated/P/punish.gif"); }
    else if (newCORE=="robocop")        {Serial.println("read robocop");    strcpy(chosenGIF, "/animated/R/robocop.gif"); }
    else if (newCORE=="sf2")            {Serial.println("read sf2");        strcpy(chosenGIF, "/animated/S/sf2.gif"); }
    else if (newCORE=="xmvsf")          {Serial.println("read xmvsf");      strcpy(chosenGIF, "/animated/X/xmvsf.gif"); }

    // -- non-animated images for arcade cores
    else if (newCORE=="aerofgt")        {Serial.println("read aerofgt");    strcpy(chosenGIF, "/static/A/aerofgts.gif"); animated_flag=!animated_flag; }
    else if (newCORE=="afighter")       {Serial.println("read afighter");   strcpy(chosenGIF, "/static/A/afighter.gif"); animated_flag=!animated_flag; }
    else if (newCORE=="alibaba")        {Serial.println("read alibaba");    strcpy(chosenGIF, "/static/A/alibaba.gif"); animated_flag=!animated_flag; }
    else if (newCORE=="alphamis")       {Serial.println("read alphamis");   strcpy(chosenGIF, "/static/A/alphamis.gif"); animated_flag=!animated_flag; }
    else if (newCORE=="amatelas")       {Serial.println("read amatelas");   strcpy(chosenGIF, "/static/A/amatelas.gif"); animated_flag=!animated_flag; }
    else if (newCORE=="amidars")        {Serial.println("read amidars");    strcpy(chosenGIF, "/static/A/amidars.gif"); animated_flag=!animated_flag; }
    else if (newCORE=="armorcar")       {Serial.println("read armorcar");   strcpy(chosenGIF, "/static/A/armorcar.gif"); animated_flag=!animated_flag; }
    else if (newCORE=="armwar")         {Serial.println("read armwar");     strcpy(chosenGIF, "/static/A/armwar.gif"); animated_flag=!animated_flag; }
    else if (newCORE=="a.astdelux")     {Serial.println("read astdelux");   strcpy(chosenGIF, "/static/A/a.astdelux.gif"); animated_flag=!animated_flag; }
    else if (newCORE=="asteroid")       {Serial.println("read asteroid");   strcpy(chosenGIF, "/static/A/asteroid.gif"); animated_flag=!animated_flag; }
    else if (newCORE=="athena")         {Serial.println("read athena");     strcpy(chosenGIF, "/static/A/athena.gif"); animated_flag=!animated_flag; }
    else if (newCORE=="aurail")         {Serial.println("read aurail");     strcpy(chosenGIF, "/static/A/aurail.gif"); animated_flag=!animated_flag; }
    else if (newCORE=="azurian")        {Serial.println("read azurian");    strcpy(chosenGIF, "/static/A/azurian.gif"); animated_flag=!animated_flag; }
    else if (newCORE=="bagman")         {Serial.println("read bagman");     strcpy(chosenGIF, "/static/B/bagman.gif"); animated_flag=!animated_flag; }
    else if (newCORE=="ballbomb")       {Serial.println("read ballbomb");   strcpy(chosenGIF, "/static/B/ballbomb.gif"); animated_flag=!animated_flag; }
    else if (newCORE=="bayroute")       {Serial.println("read bayroute");   strcpy(chosenGIF, "/static/B/bayroute.gif"); animated_flag=!animated_flag; }
    else if (newCORE=="berzerk")        {Serial.println("read berzerk");    strcpy(chosenGIF, "/static/B/berzerk.gif"); animated_flag=!animated_flag; }
    else if (newCORE=="birdiy")         {Serial.println("read birdiy");     strcpy(chosenGIF, "/static/B/birdiy.gif"); animated_flag=!animated_flag; }
    else if (newCORE=="birdtry")        {Serial.println("read birdtry");    strcpy(chosenGIF, "/static/B/birdtry.gif"); animated_flag=!animated_flag; }
    else if (newCORE=="bosconian")      {Serial.println("read bosconian");  strcpy(chosenGIF, "/static/B/bosco.gif"); animated_flag=!animated_flag; } //no core yet but i love this game
    else if (newCORE=="mspacman")       {Serial.println("read mspacman");   strcpy(chosenGIF, "/static/M/mspacman.gif"); animated_flag=!animated_flag; }
    

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
    else if (newCORE=="Genesis")      {Serial.println("read Genesis");    strcpy(chosenGIF, "/animated/G/Genesis.gif"); }
    else if (newCORE=="MEGACD")       ;//do something
    else if (newCORE=="NEOGEO")       {Serial.println("read NEOGEO");    strcpy(chosenGIF, "/animated/N/NEOGEO.gif"); }
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
    else 
    {
      strcpy(chosenGIF, "no-match");
      newCORE.toCharArray(newCOREArray, newCORE.length()+1);
    }  
    
  } // end newCORE!=currentCORE

  currentCORE=newCORE;  // Update Buffer

  //no core change, show the current currentCORE gif
  //Serial.printf("%s is oldcore, %s is newcore, %s is chosenGIF\n", String(currentCORE), String(newCORE), chosenGIF);

  if (strcmp(chosenGIF,"no-match")) 
  {
    if (SD.exists(chosenGIF)) 
    {
      ShowGIF(chosenGIF,animated_flag);
    }
    else
    {
      Serial.printf("IMAGE FILE %s NOT FOUND!\n", chosenGIF);
      dma_display->clearScreen();
      dma_display->setCursor(0, 0);
      dma_display->print(chosenGIF);
      dma_display->println(" not found");
      delay(3000);
    }
  } 
  else 
  {
    dma_display->clearScreen();
    dma_display->setCursor(0, 0);
    dma_display->println(newCOREArray);
    delay(3000);
  }  
} /* void loop() */

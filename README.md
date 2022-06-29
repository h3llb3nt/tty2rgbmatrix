# tty2rgbmatrix
MiSTer fpga add-on showing text, pictures or animated gifs on an RGB matrix panel powered by an ESP32

this project is adapted from https://github.com/venice1200/MiSTer_tty2oled project to show mister core information on a separate display. i had an RGB matrix panel from another project lying around and figured i'd see what i could do.

# hardware
what hardware you'll need:
- some kind of ESP32 (or equivalent) to interface with the MiSTer via USB and with the RGB matrix or matrices. im using a Trinity EPS32 board made by Brian Lough -> https://esp32trinity.com/
- a HUB75 compatible RGB matrix or matrices. im using (2) 64x32 rgb panels from aliexpress -> https://www.aliexpress.com/item/3256801502846969.html
- a big enough powersupply to run them both
- optional: a sheet or piece of LED diffusing acrlyic. you can get these from TAP plastics or other places online.

# software
what software you'll need:
- arduino IDE or equivalent
- associated libraries:
	- ESP32 HUB75 library by mrfaptastic -> https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA
	- AnimatedGIF library by Larry Bank -> https://github.com/bitbank2/AnimatedGIF
- tty2oled code installed on your MiSTer. this is what sends the core info the MiSTer is running and the ESP32 reads.
- something else i'm sure i'm forgetting

# Work In Progress
items i'm still working on:
- ADD MORE GIFS OF ARCADE CORES
- add font library so that text that displays for cores that do not have images created are shown in a nicer way, perhaps scroll from right to left
- add optional SDcard adapter to ESP32 board so microcontroller can access images/gifs externally rather than using storage on the microcontroller itself.

# Demo
https://youtu.be/un_bDXi2HBI
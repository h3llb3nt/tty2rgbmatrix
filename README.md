# tty2rgbmatrix
MiSTer fpga add-on showing text, pictures or animated gifs on an RGB matrix panel powered by an ESP32

![mister_logo](docs/images/mister_logo.jpeg "mister_logo")

this project is adapted from https://github.com/venice1200/MiSTer_tty2oled project to show mister core information on a separate display. i had an RGB matrix panel from another project lying around and figured i'd see what i could do.

# hardware
what hardware you'll need:
- some kind of ESP32 (or equivalent) to interface with the MiSTer via USB and with the RGB matrix or matrices. im using a Trinity EPS32 board made by Brian Lough -> https://esp32trinity.com/

![esp32 trinity board](docs/images/esp32trinity.jpeg "esp32 trinity board")

- a HUB75 compatible RGB matrix or matrices. im using (2) 64x32 rgb panels from aliexpress -> https://www.aliexpress.com/item/3256801502846969.html

![hub75 rgb panel](docs/images/example_hub75_panel.jpeg "hub75 rgb panel")

![hub75 rgb panel reverse](docs/images/example_hub75_panel_reverse.jpeg "hub75 rgb panel reverse")

- a big enough powersupply to run them both

![power supply](docs/images/powersupply.jpeg "powerbrick")

- optional: a sheet or piece of LED diffusing acrlyic. you can get these from TAP plastics or other places online.

# software
what software you'll need:
- arduino IDE or equivalent
- associated libraries:
	- ESP32 HUB75 library by mrfaptastic -> https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA
	- AnimatedGIF library by Larry Bank -> https://github.com/bitbank2/AnimatedGIF
- tty2oled code installed on your MiSTer. this is what sends the core info the MiSTer is running and the ESP32 reads.
- something else i'm sure i'm forgetting

# setup
- follow venice's instructions on setting up tty2oled. tty2rgbmatrix arduino code watches for the same serial outputs. -> https://github.com/venice1200/MiSTer_tty2oled/wiki/Installation
- setup arduino IDE including adding ESP32 support, and the libraries mentioned above.
- current version looks for image files on the microcontroller's storage. this requires using SPIFFS and manually uploading the images via an ESP32 Sketch Data Upload Tool in the Arduino IDE:
	- https://github.com/me-no-dev/arduino-esp32fs-plugin
	- This tool will upload the contents of the data/ directory in the sketch folder onto the ESP32 itself.
- flash your ESP32 with the tty2rgbmatrix.ino


# Work In Progress
items i'm still working on:
- ADD MORE GIFS OF ARCADE CORES
- add font library so that text that displays for cores that do not have images created are shown in a nicer way, perhaps scroll from right to left
- esp32 usually only comes with 4MB flash and a portion of that is used for code storage. current images are already filling up available space so i need to either a) add optional SDcard adapter to ESP32 board so microcontroller can access images/gifs externally rather than using storage on the microcontroller itself or b) pull images directly from MiSTer like tty2oled does

# Future Options (aka not any time soon)
- resize images/gifs that are not 128x32 'on the fly'
- add 4 more panels to make 192x64 a possibility

# Demo
https://youtu.be/un_bDXi2HBI
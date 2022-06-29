# tty2rgbmatrix
MiSTer fpga add-on showing text, pictures or animated gifs on an RGB matrix panel powered by an ESP32

this project is adapted from https://github.com/venice1200/MiSTer_tty2oled project to show mister core information on a separate display. i had an RGB matrix panel from another project lying around and figured i'd see what i could do.

what hardware you'll need:
- some kind of ESP32 (or equivalent) to interface with the MiSTer via USB and with the RGB matrix or matrices. im using a Trinity EPS32 board made by Brian Lough -> https://esp32trinity.com/
- a HUB75 compatible RGB matrix or matrices. im using (2) 64x32 rgb panels from aliexpress -> https://www.aliexpress.com/item/3256801502846969.html
- a big enough powersupply to run them both
- optional: a sheet or piece of LED diffusing acrlyic. you can get these from TAP plastics or other places online.



https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA

https://github.com/bitbank2/AnimatedGIF

https://www.youtube.com/watch?v=un_bDXi2HBI
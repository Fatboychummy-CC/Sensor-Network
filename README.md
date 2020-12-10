# Sensor Network
This project was done for my CPSC 4710 class in which we were to make a wireless sensor network.

This system was prototyped using ComputerCraft (Code available in [/Client_MC](https://github.com/Fatboychummy-CC/Sensor-Network/tree/master/Client_MC)), and then implemented (albeit with a good number of bugs and unimplemented portions) on Arduino (Code available in [/Client_Arduino](https://github.com/Fatboychummy-CC/Sensor-Network/tree/master/Client_Arduino)), using an ESP8266 and some proximity sensors.

The server backend is written in Lua using Lapis.

# Usage
## Server
To run the server, first install lua and luarocks. Then, run the command:

    luarocks install lapis

Clone the repository, CD into the `Server` folder, and simply type `lapis server`. Depending on your setup, you may need to `sudo` it as well.

You can edit `conf.lua` to change the port if you wish.

## Arduino:

* Pins 2 and 3 (D2 and D3 on the nano) are used as RX and TX, respectively, to communicate with the ESP8266.
* Pin 4 (D4 on the nano) is used to control the ESP8266 enable.
* Pins 9 and 10 (D9 and D10 on the nano) are used as inputs from the proximity sensors (top and bottom, respectively).

You will need to tweak the program to use a different IP address. There are 3 lines which need to be changed, at the very top. They have a bunch of `#`'s. Replace that with the IP address of your server. If you wish, you can change the port as well.

Upload the code to the arduino, turn on the server, then you should be good to go.

## Computercraft:
Place a computer, then a block in front of it, and a block below that block. Place a redstone torch underneath of the computer on the side of the block you placed. Finally, connect tripwires to the two blocks you placed.

You will need to tweak the program to use a different IP address/port.

### IMPORTANT
The TX pin needs to go to a 5V->3.3V voltage divider! It is possible that 5V will kill the ESP8266 (though sometimes it works, better to be safe than sorry though).

Similarly, pin 4 needs to go to a voltage regulator designed for 3.3v.

# Notes
I used an extra voltage divider in my setup because my arduino's 3.3v output was shot.

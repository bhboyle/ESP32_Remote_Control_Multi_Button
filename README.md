This is the code for my ESP32 based WIFI remote control with four buttons that send either MQTT messages when pressed or make a HTTP request when a button is pressed.

I have found that sending MQTT messages can fail one out of ten times. By switching to getting an HTTP url it seems to be more reliable. I have created php pages that send the mqtt messages as a workaround and everything seems to work very well.

The project currently uses a Lolin32 lite connected to a daughter board that has the buttons, LEDs and the voltage devider for the battery monitor

The project is ment to be totaly portable and powered by a Liion battery.

The Lolin32 lite board is very energy efficitent and I expect to get at least 80 days or so on a full charge out of a 2000Mah battery. I may change to a flat lipo battery that has less capacity but that will be determined by how long I get from the 18650 I am currently using.

I also may make a complete board that has only an ESP32 module rather than using the Lolin32 board. Not sure yet.

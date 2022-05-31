This is the code for my ESP32 based WIFI remote control with four buttons that send either MQTT messages when pressed or make a HTTP request when a button is pressed.

I have found that sending MQTT messages can fail one out of ten times. By switching to getting an HTTP url it seems to be more reliable. I have created php pages that send the mqtt messages as a workaround and everything seems to work very well. I should also add that I have found that using HTTP is much faster. I have found that MQTT triggers can take between 3-4 seconds to happen while HTTP triggers can take between 1-2 seconds to happen. In both cases I was using a static IP so networking was not a factor in those tests.

The "HTTP_Remote_Control" file is the current file. I have left in the old one that used MQTT for the meesages for referance. Like I said the MQTT version was not always 100% reliable and so I have switched over to using http get requests. The HTTP get can send data to the PHP file that is sitting on a machine runnng Lighttpd and executes system commands that send the MQTT messages. This is more complex than I would like but for me reliability is more important than complexity.

The PHP folder contains the PHP scripts that are called by the ESP32 over http. Most have no data sent to them are only triggger actions by theier being called. The Details.php file is used to update the databse via MQTT with the battery voltage and WIFI signal strength of the remote contorl.

The project currently uses a Lolin32 lite connected to a daughter board that has the buttons, LEDs and the voltage devider for the battery monitor

The project is ment to be totaly portable and powered by a Liion battery.

The Lolin32 lite board is very energy efficitent and I expect to get at least 120 days or so on a full charge out of a flat  1200Mah battery. 

I also may make a complete board that has only an ESP32 module rather than using the Lolin32 board. Not sure yet.

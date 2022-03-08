/*
MQTT remote control multi button version

Each button sends a different MQTT message
The ESP32 sits in deep sleep until the button is pressed.
=====================================
Created Feb 14 2022
Based on the Deep Sleep example here:
https://randomnerdtutorials.com/esp32-external-wake-up-deep-sleep/
*/

#include <PubSubClient.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define failedLED 12 // Digital pin for LED that will be used to indicate a failed connection to WIFI
#define connectedLED 13 // Digital pin for LED that will be used to indicate a sucessful connection to the MQTT server
#define batteryVOLTAGE 34 // The analog pin of the voltage divider that reads the battery voltage
#define thresholdVoltage 3.4 // The voltage that starts the blinking to idicate a low voltage in the battery

// This next one needs to be calibrated before use
#define BATTERYMULTIPLIER 0.0017808680994522 // this is the multiplier that is used to multiply the analog reading in to a battery voltage. This was calibrated initially with my multimeter

// This next definition is a bitmask use the set the interupt pins for waking up the esp32 from deep sleep 
#define BUTTON_PIN_BITMASK 0x308008000 // GPIOs 15, 27, 32 and 33 -- Used for defining what GPIO pins are used to wake up the ESP32

// define the GPIO pins and MQTT messages of the buttons. Used by the select case  
#define button1 15
//#define button1Topic "outTopic"
#define button1Topic "door/control"
#define button1Message "left"

#define button2 27
#define button2Topic "outTopic"
#define button2Message "right"

#define button3 32
#define button3Topic "time/sunset"
#define button3Message "on"

#define button4 33
#define button4Topic "time/sunset"
#define button4Message "off"

//#define batteryMessage "Remote1/Battery/Voltage"
//#define wifiSignal "Remote1/Wifi/strength"

bool batteryStatus = false; // True if the battery is below 3.4 volts
bool connectStatus = false;

char deviceName[] = "Remote1";
char batteryMessage[] = "/Battery/Voltage";
char wifiSignal[] = "/Wifi/strength";

char ssid[] = "gallifrey";   // WIFI network SSID name
char pass[] = "rockstar";    // WIFI network password 

char clientID[] = "mqttRemoteControl"; // this should be changed for any additonal devices
char username[] = "garage"; // what username is used by the MQTT server
char password[] = "Hanafranastan1!";  // what password is used by the MQTT server

int port = 1883;  // TCPIP port used by the MQTT server

IPAddress server(172, 17, 17, 10); // IP address of the MQTT server

//The server url for the HTTP request
String serverName = "http://192.168.1.106:1880/update-sensor";

// Set your Static IP address, gateway and submet mask
IPAddress local_IP(172, 17, 17, 250); 
IPAddress gateway(172, 17, 17, 1);
IPAddress subnet(255, 255, 255, 0);

// instaniaite the wifi object and the MQTT client
WiFiClient wifiClient;
PubSubClient client(wifiClient);

void setup(){

  //Serial.begin(115200);

  pinMode(failedLED, OUTPUT); // set the failedLED pin to output
  pinMode(connectedLED, OUTPUT); // set the connectedLED pin to output
  pinMode(button1, INPUT_PULLDOWN); // set the GPIO inputs correctly and turn on the internal pull down resistor
  pinMode(button2, INPUT_PULLDOWN);
  pinMode(button3, INPUT_PULLDOWN);
  pinMode(button4, INPUT_PULLDOWN);

  // Configures static IP address
  if (!WiFi.config(local_IP, gateway, subnet)) {
    //Serial.println("STA Failed to configure");
  }

  WiFi.mode(WIFI_STA); // set the mode the WIFI is using 
  WiFi.begin(ssid, pass); // start up the WIFI 

  int count =0;   // create a variable that is used to set a timer for connecting to the WIFI 
  while (WiFi.status() != WL_CONNECTED) { // check if it is connected to the AP
      delay(50); // wait 50 miliseconds
      count++;  //Increment the timer variable 
      if (count > 60) { break;} // it is had been trying to connect to the WIFI for 60 * 50 milliseconds the stop trying
  }

   if (WiFi.status() != WL_CONNECTED) { // if it can not connet to the wifi
            // light red LED
            digitalWrite(failedLED, HIGH);  // turn on the failedLED

            delay(5000);  // wait five seconds
            // turn off LED
            digitalWrite(failedLED, LOW); // turn off the failedLED
            
            esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK,ESP_EXT1_WAKEUP_ANY_HIGH); // set the correct deep sleep mode

            esp_deep_sleep_start(); // go to deep sleep

    } 

  // turn on connected LED
  digitalWrite(connectedLED, HIGH);

  delay(200); // give the WIFI some time to settle

  // get the wifi signal strenth to send to the MQTT server
  int wifiStrength = WiFi.RSSI();

  // read the battery voltage via an analog read and convert it to a basic float
  float volts = getBatteryVoltage();

  // if the battery voltage is below 3.4 volts the set the batteryStatus flag
  if (volts < thresholdVoltage) {
    batteryStatus = true;
  }

  // convert the wifi signal strength to a char array for sending to the MQTT server
  char cstr[16]; // buffer to hold the conversion of the int to char
  itoa(wifiStrength, cstr, 10); // do the actual conversion



  // set the server deatils for the MQTT object
  client.setServer(server, 1883);
  //connect to MQTT server
  client.connect(clientID, username, password);

  delay(250); // give the MQTT client code time to settle

  //call the function that will react to button pushes
  print_GPIO_wake_up();

  char result[8]; // Buffer to convert battery voltage float in to a Char array
  dtostrf(volts, 6, 2, result); // do the actual conversion from Float to Char

  // the following code adds deviceName and batteryMessage together into a buffer to use as the MQTT topic
  char buf[30]; // create the buffer
  strcpy(buf, deviceName);  // put deviceName into it
  strcpy(buf + strlen(deviceName), batteryMessage); // add the battery message to the deviceName

  // publish battery voltage to MQTT server
  client.publish(buf , result);

  // reset and do the same thing for the wifiSignal topic
  strcpy(buf, deviceName);  // reset and copy deviceName into the buffer
  strcpy(buf + strlen(deviceName), wifiSignal); // add wifiSignal to the buffer

  //send the wifi signal strength to the MQTT server
  client.publish(buf , cstr);

  // if the batteryStatus flag is set, flash the failed LED five times to indicate a low battery
  if (batteryStatus) {

      int temp =0;  // create a temp variable to track the blinking loop
      while(temp < 5) { // while loop start 
        digitalWrite(failedLED, HIGH);  // turn the LED on
        delay(500); // wait a short period
        digitalWrite(failedLED, LOW); // turn the LED off
        delay(100); // wait again
        temp++; // increment the temp variable
      }
      

  }

  client.disconnect();

  //ste the correct deep sleep mode
  esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK,ESP_EXT1_WAKEUP_ANY_HIGH);
  
  //Go to sleep now
  delay(1500);
  
  esp_deep_sleep_start();

}

void loop(){
  //This is not going to be called
}


/*
  Functiont that reads the battery voltage by doing an analog read and then converting the 
  reading into a floating point and returning it.
*/
float getBatteryVoltage() {
    // read the voltage of the battery
    int batteryValue = analogRead(batteryVOLTAGE);  

    // convert it to a true voltage floting point value
    float temp = batteryValue * BATTERYMULTIPLIER; 

    return(temp);
}

/*
    To be called if a button is to be used to call a web page.
*/
void sendHTTPrequest() {

      HTTPClient http;  // create the HTTP client object

      String serverPath = serverName + ""; //add more to the URL here if needed
      
      // Your Domain name with URL path or IP address with path
      http.begin(serverPath.c_str());
      
      // Send HTTP GET request
      int httpResponseCode = http.GET();
      
      if (httpResponseCode>0) {
          //Serial.print("HTTP Response code: ");
          //Serial.println(httpResponseCode);
          String payload = http.getString();
          //Serial.println(payload);
      }
      else {
          //Serial.print("Error code: ");
          //Serial.println(httpResponseCode);
      }
      // Free resources
      http.end();

}

/*
Function to react to which GPIO that triggered the wakeup. OR expressed differently, what button was pushed
*/
void print_GPIO_wake_up(){
  
    uint64_t GPIO_reason = esp_sleep_get_ext1_wakeup_status(); // find out what button was pushed
    int reason = (log(GPIO_reason))/log(2); // convert that data to a GPIO number

    switch(reason) // Take the result and react depending on what button was pressed.
    {
      case button1:
          // Send MQTT messaage for opening and closing the shed door
          client.publish(button1Topic, button1Message);
          break;
      case button2:
          // Send MQTT messaage for opening and closing the left garage door
          client.publish(button2Topic, button2Message);
          break;      
      case button3:
          // Send MQTT messaage for opening and closing the right garage door
          client.publish(button3Topic, button3Message);
          break;
      case button4:
          // Do nothing becuase this button has not yet be given a use
          client.publish(button4Topic, button4Message);
          // sendHTTPrequest(); //call the HTTP request function
          break;
      default :
          // Do nothing if anything else causes a boot of the ESP32
          break;

    }

}
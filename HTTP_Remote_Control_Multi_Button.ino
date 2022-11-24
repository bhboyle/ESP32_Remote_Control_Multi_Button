
/*
MQTT remote control multi button version
Each button sends a different MQTT message

This code has been change to use HTTP GET commands rather than MQTT messages
The MQTT connection would fail once in every 10 or so connections. I was
not able to determine a cause for this behaviour and so I went with HTTP GETs
to increase reliability.

The ESP32 sits in deep sleep until the button is pressed.
=====================================
Created Feb 14 2022
Based on the Deep Sleep example here:
https://randomnerdtutorials.com/esp32-external-wake-up-deep-sleep/
*/

//#include <PubSubClient.h>
#include <WiFi.h>
#include <esp_wifi.h>
//#include <esp_deep_sleep.h>
#include <HTTPClient.h>
//#include <MQTT.h>
#include <Adafruit_NeoPixel.h>
#include <EEPROM.h> // needed to store the boot type and count

#define failedLED 32         // Digital pin for LED that will be used to indicate a failed connection to WIFI
#define LED 33               // Digital pin for LED that will be used to indicate a successful connection to the WIFI
#define batteryVOLTAGE 35    // The analog pin of the voltage divider that reads the battery voltage
#define thresholdVoltage 3.4 // The voltage that starts the blinking to indicate a low voltage in the battery
#define MOSFETpin 12         // This is the power pin of the Addressable LED. Used to power down the LED while in sleep mode
#define NUMPIXELS 1          // Popular NeoPixel ring size

// Comment out to use single LED rather than Neopixel
//#define NEOPIXEL TRUE // to be used to determine what to compile, individual LEDs or a single Neopixel

// This next one needs to be calibrated before use
#define BATTERYMULTIPLIER 0.0035020920502092 // this is the multiplier that is used to multiply the analog reading in to a battery voltage. This was calibrated initially with my multimeter

// This next definition is a bitmask use the set the interrupt pins for waking up the esp32 from deep sleep
//#define BUTTON_PIN_BITMASK 0x308008000 // GPIOs 15, 27, 32 and 33 -- Used for defining what GPIO pins are used to wake up the ESP32
//#define BUTTON_PIN_BITMASK 0x308010000 // GPIOs 16, 27, 32 and 33 -- Used for defining what GPIO pins are used to wake up the ESP32
//#define BUTTON_PIN_BITMASK 0xB08000000
//#define BUTTON_PIN_BITMASK 0x8C0020

// define the GPIO pins and MQTT messages of the buttons. Used by the select case
// The pin number definitions here must match the Pin BITMASK above for the device to function correctly
// *** Since I have switched to HTTP Get command rather than MQTT the following Defines for Topics and Messages
// no longer matter as much but are left in just in case I feel the need to switch back.
#define button1 4
#define button1Topic "door/control"
#define button1Message "left"

#define button2 14
#define button2Topic "door/control"
#define button2Message "right"

#define button3 27
#define button3Topic "time/sunset"
#define button3Message "on"

#define button4 15
#define button4Topic "time/sunset"
#define button4Message "off"

#define EEPROM_SIZE 12  // this creates the storage area that holds the boot count and type
#define bootCountAddr 0 // this is the location in EEPROM to start how many times we reboot
#define bootTypeAddr 5  // this is the EEPROM address where we store the boot type for resets

//#define batteryMessage "Remote1/Battery/Voltage"
//#define wifiSignal "Remote1/Wifi/strength"

unsigned int bootType = 0;    // used to check if this a full reboot or a warmboot
unsigned int bootCounter = 0; // used to check if this a full reboot or a warmboot

bool batteryStatus = false; // True if the battery is below 3.4 volts
bool connectStatus = false; // True if the WIF connects successfully
char BatteryVoltageChar[8]; // Buffer to convert battery voltage float in to a Char array

const char deviceName[] = "Remote1";        // This is the host name of this device
char batteryMessage[] = "/Battery/Voltage"; // This is the MQTT topic of use to send the battery voltage
char wifiSignal[] = "/Wifi/strength";       // This is the MQTT topic use to send the WIFI signal strength.
char wifistr[16];                           // buffer to hold the conversion of the int to char

char ssid[] = "gallifrey"; // WIFI network SSID name
char pass[] = "rockstar";  // WIFI network password

const char username[] = "remote1";         // what username is used by the MQTT server
const char password[] = "Hanafranastan1!"; // what password is used by the MQTT server

int port = 1883; // TCPIP port used by the MQTT server

IPAddress server(172, 17, 17, 10); // IP address of the MQTT server

// The server url for the HTTP request
String serverName = "http://172.17.17.12/";

// Set your Static IP address, gateway and subnet mask
IPAddress local_IP(172, 17, 17, 251);
IPAddress gateway(172, 17, 17, 1);
IPAddress subnet(255, 255, 255, 0);

// instantiate the wifi object and the MQTT client
WiFiClient wifiClient;
// PubSubClient client(wifiClient);
// MQTTClient client;

#ifdef NEOPIXEL
Adafruit_NeoPixel pixels(NUMPIXELS, LED, NEO_GRB + NEO_KHZ800);
#endif

void setup()
{

#ifdef NEOPIXEL
    pinMode(MOSFETpin, OUTPUT); //  set the output used by the Addressable LED to output
    delay(50);                  // wait a little bit of time to give the neopixels time to settle.
    pixels.begin();             // INITIALIZE NeoPixel strip object (REQUIRED)
    pixels.clear();             // Set all pixel colors to 'off'
#else
    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW);
    pinMode(failedLED, OUTPUT);
    digitalWrite(failedLED, LOW);
#endif

    EEPROM.begin(EEPROM_SIZE); // start the EEPROM

    bootCounter = readIntFromEEPROM(bootCountAddr); // get the current location from EEPROM for the counter variable
    bootType = readIntFromEEPROM(bootTypeAddr);     // get the current
    if (bootType)
    {
        writeIntIntoEEPROM(bootTypeAddr, 0); // if the boot type was a softboot reset the value in EEPROM for the next boot
        if (bootCounter > 2)
        {
            bootCounter = 0;
            writeIntIntoEEPROM(bootCountAddr, 0);
            writeIntIntoEEPROM(bootTypeAddr, 0);
#ifdef NEOPIXEL
            SetLEDColor(255, 0, 0);
            delay(1500); // wait five seconds
            // turn off LED
            SetLEDColor(0, 0, 0);
#else
            digitalWrite(failedLED, HIGH); // turn on the failedLED
            delay(1500);
            digitalWrite(failedLED, LOW); // turn off the failedLED
#endif
            gotosleep();
        }
    }

    esp_wifi_start();
    Serial.begin(9600);
    Serial.println("Starting....");
    Serial.print(" Boot Type = ");
    Serial.println(bootType);
    Serial.print(" Boot Count =");
    Serial.println(bootCounter);

    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);
    pinMode(button1, INPUT_PULLDOWN); // set the GPIO inputs correctly and turn on the internal pull down resistor
    pinMode(button2, INPUT_PULLDOWN);
    pinMode(button3, INPUT_PULLDOWN);
    pinMode(button4, INPUT_PULLDOWN);

#ifdef NEOPIXEL
    pinMode(MOSFETpin, OUTPUT); //  set the output used by the Addressable LED to output
    delay(50);                  // wait a little bit of time to give the neopixels time to settle.
    pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
    pixels.clear(); // Set all pixel colors to 'off'
#else
    pinMode(LED, OUTPUT);
    digitalWrite(LED, LOW);
    pinMode(failedLED, OUTPUT);
    digitalWrite(failedLED, LOW);
#endif

    // Configures static IP address. We use a static IP because it connects to the network faster
    if (!WiFi.config(local_IP, gateway, subnet))
    {
        // Serial.println("STA Failed to configure");
    }

    WiFi.reconnect();
    WiFi.mode(WIFI_STA);    // set the WIFI mode to Station
    WiFi.begin(ssid, pass); // start up the WIFI

    WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);

    int count = 0; // create a variable that is used to set a timer for connecting to the WIFI
    while (WiFi.status() != WL_CONNECTED)
    {              // check if it is connected to the AP
        delay(100); // wait 100 miliseconds
        count++;   // Increment the timer variable
        if (count > 30)
        {
            break;
        } // it is had been trying to connect to the WIFI for 30 * 100 milliseconds then stop trying
    }

    // This second WIFI status is used to do indicate a failed connection and then put the ESP32 in deep sleep
    if (WiFi.status() != WL_CONNECTED)
    { // if it can not connect to the wifi
      // write the boot verion and count into eeprom and reboot

        // gotosleep();
        bootCounter++;
        bootType = 1;
        writeIntIntoEEPROM(bootCountAddr, bootCounter);
        writeIntIntoEEPROM(bootTypeAddr, bootType);
        Serial.println("******RESTARTING****");
        Serial.print(" Boot Type = ");
        Serial.println(bootType);
        Serial.print(" Boot Count =");
        Serial.println(bootCounter);
        ESP.restart();
    }

    // turn on connected LED
#ifdef NEOPIXEL
    SetLEDColor(0, 255, 0);
#else
    digitalWrite(LED, HIGH);
#endif
    delay(500); // *** added to try and make connecting to WIFI better and more consistant
    // Serial.println("WIFI connection active");

    // get the wifi signal strength to send to the MQTT server
    int wifiStrength = WiFi.RSSI();

    // convert the wifi signal strength to a char array for sending to the MQTT server
    itoa(wifiStrength, wifistr, 10); // do the actual conversion

    // read the battery voltage via an analog read and convert it to a basic float
    float volts = getBatteryVoltage();

    dtostrf(volts, 4, 2, BatteryVoltageChar); // convert from Float to Char

    //**** temp string here delete after testing
    // String BatteryVoltageString = String(analogRead(batteryVOLTAGE));

    // if the battery voltage is below 3.4 volts then set the batteryStatus flag
    if (volts < thresholdVoltage)
    {
        batteryStatus = true;
    }

    // call the function that will react to button pushes
    print_GPIO_wake_up(); // this is the hart of this program

    // send the current battery voltage and WIFI signal strength
    sendHTTPrequest("details.php?remote=" + String(deviceName) +
                    "&wifi=" + String(wifistr) +
                    "&battery=" + String(BatteryVoltageChar)); // call the HTTP request function
                                                               //"&battery=" + String(BatteryVoltageString));

    // if the batteryStatus flag is set, flash the failed LED five times to indicate a low battery
    if (batteryStatus)
    {

        int temp = 0;    // Create a temp variable to track the blinking loop
        while (temp < 5) // Here we blink the LED Red quickly to indicate a low battery situation
        {                // While loop start

#ifdef NEOPIXEL
            SetLEDColor(255, 0, 0);
            delay(500); // wait a short period
            SetLEDColor(0, 0, 0);
            delay(100); // wait again
            temp++;     // increment the temp variable
#else
            digitalWrite(failedLED, HIGH); // turn the LED on
            delay(500);
            digitalWrite(failedLED, LOW); // turn the LED off
            delay(100);
            temp++;
#endif
        }
    }

    gotosleep();
}

void loop()
{
    // This is not going to be called
}

/*
  Function that reads the battery voltage by doing an analog read and then converting the
  reading into a floating point and returning it.
*/
float getBatteryVoltage()
{
    // read the voltage of the battery
    int batteryValue = analogRead(batteryVOLTAGE);

    Serial.print("Battery reading = ");
    Serial.println(batteryValue);

    // convert it to a true voltage floating point value
    float temp = batteryValue * BATTERYMULTIPLIER;

    return (temp);
}

/*
    To be called if a button is to be used to call a web page.
*/
void sendHTTPrequest(String command)
{

    HTTPClient http; // create the HTTP client object

    String serverPath = serverName + command; // add more to the URL here if needed

    // Your Domain name with URL path or IP address with path
    http.begin(serverPath.c_str());

    // Send HTTP GET request
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0)
    {

        String payload = http.getString();
    }
    else
    {
        // Serial.print("Error code: ");
        // Serial.println(httpResponseCode);
    }
    // Free resources
    http.end();
}

/*
Function to react to which GPIO that triggered the wakeup. OR expressed differently, what button was pushed
*/
void print_GPIO_wake_up()
{

    uint64_t GPIO_reason = esp_sleep_get_ext1_wakeup_status(); // find out what button was pushed
    int reason = (log(GPIO_reason)) / log(2);                  // convert that data to a GPIO number

    int temp = 0; // used for tracking how many times the While loops happen

    switch (reason) // Take the result and react depending on what button was pressed.
    {
    case button1:
        // Send MQTT message for Button 1 VIA HTTP

        sendHTTPrequest("garage.php"); // call the HTTP request function
        break;

    case button2:
        // Send MQTT message for Button 2 VIA HTTP

        sendHTTPrequest("shop.php"); // call the HTTP request function
        break;

    case button3:
        // Send MQTT message for Button 3 VIA HTTP

        sendHTTPrequest("on.php"); // call the HTTP request function
        break;

    case button4:
        // Send MQTT message for Button 4 VIA HTTP

        sendHTTPrequest("off.php"); // call the HTTP request function
        // sendHTTPrequest("on.php"); //call the HTTP request function
        break;

    default:
        // Do nothing if anything else causes a boot of the ESP32
        break;
    }
}

// disconnect from the MQTT server and put the ESP32 in deep sleep
void gotosleep()
{

    wifiClient.stop(); // turn off the wifi before sleeping
    WiFi.disconnect(true, false);

    // Serial.println("Going to Sleep now");
    delay(500);

    digitalWrite(MOSFETpin, LOW); // Turn off the Addressable LED

    // set the correct deep sleep mode

    esp_sleep_enable_ext1_wakeup(((1ULL << button1) | (1ULL << button2) | (1ULL << button3) | (1ULL << button4)), ESP_EXT1_WAKEUP_ANY_HIGH);
    // esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_HIGH);
    // Go to sleep now
    esp_deep_sleep_start();
}

#ifdef NEOPIXEL
// sets the addressable LED to the color and shows the result
void SetLEDColor(int red, int green, int blue)
{

    // pixels.Color() takes RGB values, from 0,0,0 up to 255,255,255
    pixels.setPixelColor(0, pixels.Color(red, green, blue));

    pixels.show(); // Send the updated pixel colors to the hardware.
}
#endif

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    Serial.println("Connected to AP successfully!");
}

/*
    This function reads data from the onboard EEPROM
*/
int readIntFromEEPROM(int address)
{

    int val = EEPROM.readInt(address);
    return (val);
}

/*
    This function saves data into the onboard EEPROM
*/
void writeIntIntoEEPROM(int address, int number)
{
    int count = number;

    EEPROM.writeInt(address, count);
    EEPROM.commit();

    delay(50);
}
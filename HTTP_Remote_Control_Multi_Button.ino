
/*
MQTT remote control multi button version
Each button sends a different MQTT message

This code has been change to use HTTP GET commands rather than MQTT messages
The MQTT connection would fail once in every 10 or so connections. I was
not able to determine a cose for this behavour and so I wnt with HTTP GETs
to increase reliabilty.

The ESP32 sits in deep sleep until the button is pressed.
=====================================
Created Feb 14 2022
Based on the Deep Sleep example here:
https://randomnerdtutorials.com/esp32-external-wake-up-deep-sleep/
*/

//#include <PubSubClient.h>
#include <WiFi.h>
#include <HTTPClient.h>
//#include <MQTT.h>
#include <Adafruit_NeoPixel.h>

#define failedLED 22         // Digital pin for LED that will be used to indicate a failed connection to WIFI
#define LED 23               // Digital pin for LED that will be used to indicate a sucessful connection to the MQTT server
#define batteryVOLTAGE 34    // The analog pin of the voltage divider that reads the battery voltage
#define thresholdVoltage 3.4 // The voltage that starts the blinking to idicate a low voltage in the battery
#define MOSFETpin 12         // This is the power pin of the Addressable LED. Used to power down the LED while in sleep mode
#define NUMPIXELS 1          // Popular NeoPixel ring size

// Comment out to use singel LED rather than Neopixel
//#define NEOPIXEL TRUE // to be used to determine what to compile, individual LEDs or a single Neopixel

// This next one needs to be calibrated before use
#define BATTERYMULTIPLIER 0.0017808680994522 // this is the multiplier that is used to multiply the analog reading in to a battery voltage. This was calibrated initially with my multimeter

// This next definition is a bitmask use the set the interupt pins for waking up the esp32 from deep sleep
//#define BUTTON_PIN_BITMASK 0x308008000 // GPIOs 15, 27, 32 and 33 -- Used for defining what GPIO pins are used to wake up the ESP32
//#define BUTTON_PIN_BITMASK 0x308010000 // GPIOs 16, 27, 32 and 33 -- Used for defining what GPIO pins are used to wake up the ESP32
#define BUTTON_PIN_BITMASK 0xB08000000

// define the GPIO pins and MQTT messages of the buttons. Used by the select case
// The pin number definitions here must match the Pin BITMASK above for the device to function correctly
#define button1 32
#define button1Topic "door/control"
#define button1Message "left"

#define button2 33
#define button2Topic "door/control"
#define button2Message "right"

#define button3 27
#define button3Topic "time/sunset"
#define button3Message "on"

#define button4 35
#define button4Topic "time/sunset"
#define button4Message "off"

//#define batteryMessage "Remote1/Battery/Voltage"
//#define wifiSignal "Remote1/Wifi/strength"

bool batteryStatus = false; // True if the battery is below 3.4 volts
bool connectStatus = false; // True if the WIF connects succesfully
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

// instaniaite the wifi object and the MQTT client
WiFiClient wifiClient;
// PubSubClient client(wifiClient);
// MQTTClient client;

#ifdef NEOPIXEL
Adafruit_NeoPixel pixels(NUMPIXELS, LED, NEO_GRB + NEO_KHZ800);
#endif

void setup()
{
    // Serial.begin(9600);
    // Serial.println("Starting....");

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

    // Configures static IP address
    if (!WiFi.config(local_IP, gateway, subnet))
    {
        // Serial.println("STA Failed to configure");
    }

    WiFi.mode(WIFI_STA);    // set the mode the WIFI is using
    WiFi.begin(ssid, pass); // start up the WIFI

    int count = 0; // create a variable that is used to set a timer for connecting to the WIFI
    while (WiFi.status() != WL_CONNECTED)
    {              // check if it is connected to the AP
        delay(50); // wait 50 miliseconds
        count++;   // Increment the timer variable
        if (count > 60)
        {
            break;
        } // it is had been trying to connect to the WIFI for 60 * 50 milliseconds the stop trying
    }

    // This second WIFI status is used to do indicate a failed connection and then ut the ESP32 in deep sleep
    if (WiFi.status() != WL_CONNECTED)
    { // if it can not connet to the wifi
      // light a red LED

#ifdef NEOPIXEL
        SetLEDColor(255, 0, 0);
        delay(5000); // wait five seconds
        // turn off LED
        SetLEDColor(0, 0, 0);

#else
        digitalWrite(failedLED, HIGH); // turn on the failedLED
        delay(5000);
        digitalWrite(failedLED, LOW); // turn off the failedLED
#endif
        gotosleep();
    }

    // turn on connected LED
#ifdef NEOPIXEL
    SetLEDColor(0, 255, 0);
#else
    digitalWrite(LED, HIGH);
#endif

    // Serial.println("WIFI connection active");

    // get the wifi signal strenth to send to the MQTT server
    int wifiStrength = WiFi.RSSI();

    // convert the wifi signal strength to a char array for sending to the MQTT server

    itoa(wifiStrength, wifistr, 10); // do the actual conversion

    // read the battery voltage via an analog read and convert it to a basic float
    float volts = getBatteryVoltage();

    dtostrf(volts, 4, 2, BatteryVoltageChar); // convert from Float to Char

    // if the battery voltage is below 3.4 volts then set the batteryStatus flag
    if (volts < thresholdVoltage)
    {
        batteryStatus = true;
    }

    // call the function that will react to button pushes
    print_GPIO_wake_up(); // this is the hart of this program

    sendHTTPrequest("details.php?remote=" + String(deviceName) +
                    "&wifi=" + String(wifistr) +
                    "&battery=" + String(BatteryVoltageChar)); // call the HTTP request function

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
  Functiont that reads the battery voltage by doing an analog read and then converting the
  reading into a floating point and returning it.
*/
float getBatteryVoltage()
{
    // read the voltage of the battery
    int batteryValue = analogRead(batteryVOLTAGE);

    // convert it to a true voltage floting point value
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
        // Send MQTT messaage for Button 1 VIA HTTP

        sendHTTPrequest("garage.php"); // call the HTTP request function
        break;

    case button2:
        // Send MQTT messaage for Button 2 VIA HTTP

        sendHTTPrequest("shop.php"); // call the HTTP request function
        break;

    case button3:
        // Send MQTT messaage for Button 3 VIA HTTP

        sendHTTPrequest("on.php"); // call the HTTP request function
        break;

    case button4:
        // Send MQTT messaage for Button 4 VIA HTTP

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

    // Serial.println("Going to Sleep now");
    delay(1500);

    digitalWrite(MOSFETpin, LOW); // Turn off the Addressable LED

    wifiClient.stop(); // turn off the wifi before sleeping

    // set the correct deep sleep mode
    esp_sleep_enable_ext1_wakeup(BUTTON_PIN_BITMASK, ESP_EXT1_WAKEUP_ANY_HIGH);
    // Go to sleep now
    esp_deep_sleep_start();
}

#ifdef NEOPIXEL
// sets the adressable LED to the color and shows the result
void SetLEDColor(int red, int green, int blue)
{

    // pixels.Color() takes RGB values, from 0,0,0 up to 255,255,255
    pixels.setPixelColor(0, pixels.Color(red, green, blue));

    pixels.show(); // Send the updated pixel colors to the hardware.
}
#endif
#include "config.h"

#include "Adafruit_Si7021.h" 
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h> //provides the ability to parse and construct JSON objects

Adafruit_Si7021 sensor = Adafruit_Si7021(); //create the Adafruit_Si7021 object

//include for ssd1306 128x32 i2c
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16

//LED
#define LED_PIN 13

const char* ssid = "Half-G Guest";  //connecting to wifi
const char* pass = "BeOurGuest";

typedef struct { //we created a new data type definition to hold the new data
  String bs;
  String us;      //each name value pair is coming from the weather service, these create slots to hold the incoming data
} Fixer;

Fixer rates;//created a MetData and variable conditions"

// set up the 'temperature' and 'humidity' feeds
AdafruitIO_Feed *temperature = io.feed("temperature");
AdafruitIO_Feed *humidity = io.feed("humidity");
// set up the 'digital' feed
AdafruitIO_Feed *Digital = io.feed("Digital");

void setup() {
  
  // set led pin as a digital output
  pinMode(LED_PIN, OUTPUT);
  
  Serial.begin(115200);
  
  // wait for serial port to open
  while (!Serial) {
    delay(10);
  }

  Serial.print("This board is running: ");
  Serial.println(F(__FILE__));
  Serial.print("Compiled: ");
  Serial.println(F(__DATE__ " " __TIME__));
  
  Serial.print("Connecting to "); Serial.println(ssid);
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("Si7021 test!");
  
  if (!sensor.begin()) {
    Serial.println("Did not find Si7021 sensor!");
    while (true);
  }

  Serial.print("Found model ");
  switch(sensor.getModel()) {
    case SI_Engineering_Samples:
      Serial.print("SI engineering samples"); break;
    case SI_7013:
      Serial.print("Si7013"); break;
    case SI_7020:
      Serial.print("Si7020"); break;
    case SI_7021:
      Serial.print("Si7021"); break;
    case SI_UNKNOWN:
    default:
      Serial.print("Unknown");
  }
  Serial.print(" Rev(");
  Serial.print(sensor.getRevision());
  Serial.print(")");
  Serial.print(" Serial #"); Serial.print(sensor.sernum_a, HEX); Serial.println(sensor.sernum_b, HEX);
  Serial.println();
  
  //setup for the SSD1306 display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); //initailize with the i2c addre 0x3C
  display.clearDisplay();                    //Clears any existing images
  display.setTextSize(1);                    //Set text size
  display.setTextColor(WHITE);               //Set text color to white
  display.setCursor(0,0);                    //Puts cursor back on top left corner
  display.println("Starting up...");         //Test and write up
  display.display();                         //Displaying the display

  // connect to io.adafruit.com
  Serial.print("Connecting to Adafruit IO");
  io.connect();
  
  // set up a message handler for the 'digital' feed.
  // the handleMessage function (defined below)
  // will be called whenever a message is
  // received from adafruit io.
  Digital->onMessage(handleMessage);
  
  // wait for a connection
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  // we are connected
  Serial.println();
  Serial.println(io.statusText());
  Digital->get();
  
  getFixer();
  Serial.println();
  Serial.println("1 " + rates.bs + " Dollar =");
  Serial.println(rates.us + " US Dollars"  );
  Serial.println();

}

void loop() {
  // io.run(); is required for all sketches.
  // it should always be present at the top of your loop
  // function. it keeps the client connected to
  // io.adafruit.com, and processes any incoming data.
  io.run();

  float hum = sensor.readHumidity();
  float temp = sensor.readTemperature();
  
  Serial.print("Humidity: ");
  Serial.print(sensor.readHumidity(), 2);
  Serial.print("   Temperature: ");
  Serial.println(sensor.readTemperature(), 2);
  

  // saves temperature to Adafruit IO
  temperature->save(temp);
  // save humidity to Adafruit IO
  humidity->save(hum);
  delay(5000);

  
  display.clearDisplay();
  display.setCursor(0,0);
  display.print("Temperature: ");
  display.println(temp);
  display.print("Humidity: ");
  display.print(hum);
  display.println("%");
  display.println();
  display.display();
  delay(2000);
  display.clearDisplay();

  display.setCursor(0,0);
  display.println("1 Euro Dollar equals: ");
  display.print(rates.us);
  display.print(" US Dollars");
  display.display();
}


String getIP() {
  HTTPClient theClient;
  String ipAddress;

  theClient.begin("http://api.ipify.org/?format=json");
  int httpCode = theClient.GET();

  if (httpCode > 0) {
    if (httpCode == 200) {

      DynamicJsonBuffer jsonBuffer;

      String payload = theClient.getString();
      JsonObject& root = jsonBuffer.parse(payload);
      ipAddress = root["ip"].as<String>();

    } else {
      Serial.println("Something went wrong with connecting to the endpoint.");
      return "error";
    }
  }
  return ipAddress;
}

void getFixer() {
  HTTPClient theClient;
  Serial.println("Making HTTP request");
  theClient.begin("http://data.fixer.io/api/latest?access_key=b13f6e301336635b9dd81ad6db67f2d4"); //returns IP as .json object
  int httpCode = theClient.GET();
  
  if (httpCode > 0) {
    if (httpCode == 200) {
      Serial.println("Received HTTP payload.");
      DynamicJsonBuffer jsonBuffer;
      String payload = theClient.getString();
      Serial.println("Parsing...");
      JsonObject& root = jsonBuffer.parse(payload);

      // Test if parsing succeeds.
      if (!root.success()) {
        Serial.println("parseObject() failed");
        Serial.println(payload);
        return;
      }

      rates.bs = root["base"].as<String>();    //casting these values as Strings because the metData "slots" are Strings
      rates.us = root["rates"]["USD"].as<String>();
    }
  }
  else {
    Serial.printf("Something went wrong with connecting to the endpoint in getFixer().");
  }
}

// this function is called whenever an 'digital' feed message
// is received from Adafruit IO. it was attached to
// the 'digital' feed in the setup() function above.
void handleMessage(AdafruitIO_Data *data) {
 
  Serial.print("received <- ");
 
  if(data->toPinLevel() == HIGH)
    Serial.println("HIGH");
  else
    Serial.println("LOW");
 
  // write the current state to the led
  digitalWrite(LED_PIN, data->toPinLevel());
 
}

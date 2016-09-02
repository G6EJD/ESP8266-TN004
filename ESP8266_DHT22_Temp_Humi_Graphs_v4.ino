// (C) D Bird 2016
// DHT22 temperature and humidity display using graphs
/*
Processor pin ---> Display Pin
------------------------------
ESP8266 3.3V  ---> TFT Reset  (Reset - needs to be high, or device is reset, this is really NOT RESET meaning a low resets the device)
ESP8266 3.3V  ---> TFT Vin    (Supply)
ESP8266 Gnd   ---> TFT Gnd    (Ground)
ESP8266 D8    ---> TFT CS     (TFT/Display chip select)
ESP8266 D7    ---> TFT MOSI   (Master Out Slave In, used to send commands and data to TFT and SD Card
ESP8266 D6    ---> N/C        MISO   (Master In Slave Out, used to get data from SD Card)
ESP8266 D5    ---> TFT SCK    (Clock)
ESP8266 D4    ---> TFT D/C    (Data or Command selector)
ESP8266 D3    ---> N/C        SD CS  (SD Card chip select)
ESP8266 D2    ---> DHT22 Data
ESP8266 D1    ---> not used
ESP8266 D0    ---> TFT Backlight (Used to strobe the LED backlight and reduce power consumption, can be set High permanently too, 3.3V would do)

ESP8266 3.3V  ---> DHT22 Vin  (Supply)
ESP8266 D2    ---> DHT22 Data
              ---> DHT22 Not connected
ESP8266 Gnd   ---> DHT22 Gnd  (Ground)
*/

#include <Wire.h>
#include <DHT.h>
#include <SPI.h>
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include "ESP8266WiFi.h" // Needed to reduce power consumption, by turning WiFi radio off

//----------------------------------------------------------------------------------------------------
// For the Adafruit shield, these are the default.
#define TFT_DC D4
#define TFT_CS D8

// Use hardware SPI (on Uno use #13, #12, #11) and the above for CS/DC
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

#define DHTTYPE DHT22         // or DHT11
#define DHTPIN  D2            // Remember D0 is in use to control screen brightness so can't be used as a data input
DHT dht(DHTPIN, DHTTYPE, 11); // 11 works fine for ESP8266

float humidity, temperature;  // Values read from sensor
float min_temp = 100, max_temp = -100, min_humi = 100, max_humi = -100;

#define max_readings 200

float temp_readings[max_readings+1] = {0};
float humi_readings[max_readings+1] = {0};
int   reading = 1;

#define temp_error_offset   1
#define humi_error_offset   1
#define autoscale_on  true
#define autoscale_off false
#define barchart_on   true
#define barchart_off  false

// Assign names to common 16-bit color values:
#define BLACK       0x0000
#define BLUE        0x001F
#define RED         0xF800
#define GREEN       0x07E0
#define CYAN        0x07FF
#define YELLOW      0xFFE0
#define WHITE       0xFFFF

//----------------------------------------------------------------------------------------------------
 /* (C) D L BIRD
 *  This function will draw a graph on a TFT / LCD display, it requires an array that contrains the data to be graphed.
 *  The variable 'max_readings' determines the maximum number of data elements for each array. Call it with the following parametric data:
 *  x_pos - the x axis top-left position of the graph
 *  y_pos - the y-axis top-left position of the graph, e.g. 100, 200 would draw the graph 100 pixels along and 200 pixels down from the top-left of the screen
 *  width - the width of the graph in pixels
 *  height - height of the graph in pixels
 *  Y1_Max - sets the scale of plotted data, for example 5000 would scale all data to a Y-axis of 5000 maximum
 *  data_array1 is parsed by value, externally they can be called anything else, e.g. within the routine it is called data_array1, but externally could be temperature_readings
 *  auto_scale - a logical value (TRUE or FALSE) that switches the Y-axis autoscale On or Off
 *  barchart_on - a logical value (TRUE or FALSE) that switches the drawing mode between barhcart and line graph
 *  barchart_colour - a sets the title and graph plotting colour
 *  If called with Y!_Max value of 500 and the data never goes above 500, then autoscale will retain a 0-500 Y scale, if on, it will increase the scale to match the data to be displayed, and reduce it accordingly if required.
 *  auto_scale_major_tick, set to 1000 and autoscale with increment the scale in 1000 steps.
 */
 void DrawGraph(int x_pos, int y_pos, int width, int height, int Y1Max, String title, float DataArray[max_readings], boolean auto_scale, boolean barchart_mode, int graph_colour) {
  #define auto_scale_major_tick 5 // Sets the autoscale increment, so axis steps up in units of e.g. 5
  #define yticks 5                // 5 y-axis division markers
  int maxYscale = 0;
  if (auto_scale) {
    for (int i=1; i <= max_readings; i++ ) if (maxYscale <= DataArray[i]) maxYscale = DataArray[i];
    maxYscale = ((maxYscale + auto_scale_major_tick + 2) / auto_scale_major_tick) * auto_scale_major_tick; // Auto scale the graph and round to the nearest value defined, default was Y1Max
    if (maxYscale < Y1Max) Y1Max = maxYscale; 
  }
  //Graph the received data contained in an array
  // Draw the graph outline
  tft.drawRect(x_pos,y_pos,width+2,height+3,WHITE);
  tft.setTextSize(2);
  tft.setTextColor(graph_colour);
  tft.setCursor(x_pos + (width - title.length()*12)/2,y_pos-20); // 12 pixels per char assumed at size 2 (10+2 pixels)
  tft.print(title);
  tft.setTextSize(1);
  // Draw the data
  int x1,y1,x2,y2;
  for(int gx = 1; gx <= max_readings; gx++){
    x1 = x_pos + gx * width/max_readings; 
    y1 = y_pos + height;
    x2 = x_pos + gx * width/max_readings; // max_readings is the global variable that sets the maximum data that can be plotted 
    y2 = y_pos + height - constrain(DataArray[gx],0,Y1Max) * height / Y1Max + 1;
    if (barchart_mode) {
      tft.drawLine(x1,y1,x2,y2,graph_colour);
    } else {
      tft.drawPixel(x2,y2,graph_colour);
      tft.drawPixel(x2,y2-1,graph_colour); // Make the line a double pixel height to emphasise it, -1 makes the graph data go up!
    }
  }
  //Draw the Y-axis scale
  for (int spacing = 0; spacing <= yticks; spacing++) {
    #define number_of_dashes 40
    for (int j=0; j < number_of_dashes; j++){ // Draw dashed graph grid lines
      if (spacing < yticks) tft.drawFastHLine((x_pos+1+j*width/number_of_dashes),y_pos+(height*spacing/yticks),width/(2*number_of_dashes),WHITE);
    }
    tft.setTextColor(YELLOW);
    tft.setCursor((x_pos-20),y_pos+height*spacing/yticks-4);
    tft.print(Y1Max - Y1Max / yticks * spacing);
  }
}
//----------------------------------------------------------------------------------------------------

void setup(){
  Serial.begin(9600);
  tft.begin(); // Start the TFT display
  dht.begin(); // Start the DHT22 temperature and humidity module
  delay(300);  // Wait for DHT to start
  tft.setRotation(3);
  tft.setTextSize(2);
  tft.setTextColor(WHITE);
  tft.fillScreen(BLACK);   // Clear the screen
  analogWriteFreq(500);    // Enable TFT display brightness
  analogWrite(D0, 750);    // Set display brightness using D0 as driver pin, connects to TFT Backlight pin
  WiFi.forceSleepBegin();  // Switch off WiFi to reduce power consumption to ~15MA (ESP8266) + 5mA display = ~20mA
  //WiFi.forceSleepWake(); // use to renable Wifi if needed
  for (int x = 0; x <= max_readings; x++){
    temp_readings[x] = 0;
    humi_readings[x] = 0;
  } // Clears the arrays
  // Preset the max and min values of temperature and humidity
}

void loop(){
  get_temperature_and_humidity();
  tft.setTextColor(YELLOW);
  tft.setTextSize(2);
  tft.setCursor(0,50);
  tft.print(temperature,1);
  tft.print(char(247));            //Deg-C symbol
  tft.print("C");
  tft.setCursor(0,180);
  tft.print(humidity,1);
  tft.print("%");
  temp_readings[reading] = temperature;
  humi_readings[reading] = humidity;
  if (temperature > max_temp) max_temp = temperature;
  if (temperature < min_temp) min_temp = temperature;
  if (humidity    > max_humi) max_humi = humidity;
  if (humidity    < min_humi) min_humi = humidity;
  tft.setTextSize(1);
  // Now display max and min temperature readings
  tft.setTextColor(RED);
  tft.setCursor(0,30);
  tft.print(max_temp,1);
  tft.print(char(247));          // Deg-C symbol
  tft.print("C max");
  tft.setCursor(0,160);
  tft.print(max_humi,1);
  tft.print("% max");
  tft.setTextColor(GREEN);
  tft.setCursor(0,77);
  tft.print(min_temp,1);
  tft.print(char(247));          // Deg-C symbol
  tft.print("C min");
  tft.setCursor(0,207);
  tft.print(min_humi,1);
  tft.print("% min");
  tft.setTextColor(WHITE);
  
  // Display temperature readings on graph
  // DrawGraph(int x_pos, int y_pos, int width, int height, int Y1_Max, String title, float data_array1[max_readings], boolean auto_scale, boolean barchart_mode, int colour)
  DrawGraph(105,20, 200,80,50, "Temperature",temp_readings, autoscale_on,  barchart_off, RED);
  DrawGraph(105,150,200,80,100,"Humidity",   humi_readings, autoscale_off, barchart_on,  BLUE);

  reading = reading + 1;
  if (reading > max_readings) { // if number of readings exceeds max_readings (e.g. 100) then shift all array data to the left to effectively scroll the display left
    reading = max_readings;
    for (int i = 1; i < max_readings; i++) {
      temp_readings[i] = temp_readings[i+1];
      humi_readings[i] = humi_readings[i+1];
    }
    temp_readings[reading] = temperature;
    humi_readings[reading] = humidity;
  }
  delay(5000); // 432000 = 24hrs / 200 readings = 432 secs / reading
  tft.fillScreen(BLACK);
}  

void get_temperature_and_humidity() {
  temperature = dht.readTemperature() + temp_error_offset; // Read temperature as C
  humidity    = dht.readHumidity()    + humi_error_offset; // Read humidity as %rh
  // Check if any reads failed and exit early (to try again).
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  Serial.print(temperature,2);
  Serial.print("  ");
  Serial.println(humidity,2);
}

/*  Examples of various display commands
  tft.setTextSize(2); 
  tft.fillScreen(colour);
  tft.setTextColor(txt_colour);
  tft.setCursor(x, y);
  tft.println("text...");
  tft.drawCircle(x,y,diameter,colour);
  tft.fillCircle(x,y,diameter,colour);
  tft.drawRoundRect(x1,y1,x2,y2,curve_diameter,colour);
  tft.drawLine(x1, y1, x2, y2,colour);
  tft.drawChar(x,y,char,bgcolour,color,size);
*/

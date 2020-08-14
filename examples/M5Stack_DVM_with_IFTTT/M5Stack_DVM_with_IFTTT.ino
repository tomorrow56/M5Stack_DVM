/***************************************************
 * M5Stack_DVM_with_IFTTT.ino
 * 2020/08/11
 * Customized for M5Stack by @tomorrow56
****************************************************/

#include <M5Stack.h>
#include "M5StackUpdater.h"

// WiFi instanse
#include <WiFi.h>
#include <DNSServer.h>     // https://github.com/zhouhan0126/DNSServer---esp32
#include <WebServer.h>     // https://github.com/zhouhan0126/WebServer-esp32
#include "WiFiManager.h"   // https://github.com/zhouhan0126/WIFIMANAGER-ESP32

WiFiManager wifiManager;
WiFiClient client;
IPAddress ipadr;

// IFTTT key and event triggers are  
const char* server = "maker.ifttt.com";  // Server URL
String makerKey = ""; // MakerKey of Webhooks

File myFile;
String Event[3];
boolean sendFlag[3];
String cfgFile = "/M5IFTTT.cfg";
String makerEvent;

#define TFT_GREY 0x5AEB

// DVM Pin config
#define ADC_PIN0        35
#define ADC_PIN1        36
#define RANGE_PIN1      16
#define RANGE_PIN2      17
#define RANGE_PIN3      12
#define RANGE_PIN4      13

// Resister value (unit: kohm)
float SERIES_RES = 100;
float TERM_RES1 = 300;
float TERM_RES2 = 50;
float TERM_RES3 = 30;

// Frequency of beep sound
#define NOTE 1000

// Define meter size as 1 for M5.Lcd.rotation(0) or 1.3333 for M5.Lcd.rotation(1)
#define M_SIZE 1.3333

// valiables for DVM
float PARA_RES;
float Vdc = 0;
float VdcCalc;
float VdcDisp = 0;
uint8_t VdcLCD = 0;
uint16_t interval = 10;  // Update interval
uint16_t Vread;

float ltx = 0;    // Saved x coord of bottom of needle
uint16_t osx = M_SIZE*120, osy = M_SIZE*120; // Saved x & y coords
uint32_t updateTime = 0;       // time for next update

int old_analog =  -999; // Value last displayed

String MeterLabel[5] = {"0", "1", "2", "3", "4"};
boolean RangeChange = false;
boolean Hold = false;
uint8_t RANGE = 0;

void setup(void) {
  M5.begin();

  if(digitalRead(BUTTON_A_PIN) == 0) {
    Serial.println("Will Load menu binary");
    updateFromFS(SD);
    ESP.restart();
  }

  // DVM pin initialize
  pinMode(RANGE_PIN1, OUTPUT);
  pinMode(RANGE_PIN2, OUTPUT);
  pinMode(RANGE_PIN3, OUTPUT);
  pinMode(RANGE_PIN4, OUTPUT);

  digitalWrite(RANGE_PIN1, LOW); 
  digitalWrite(RANGE_PIN2, LOW); 
  digitalWrite(RANGE_PIN3, LOW); 
  digitalWrite(RANGE_PIN4, LOW); 

  // read IFTTT config from SD
  loadConfig(cfgFile);

  // set makerEvent from SD data
  makerEvent = Event[0];


  // WiFiManager debug message enabled
  wifiManager.setDebugOutput(true);

  // WiFiManager auto connect setting check
  wifiManager.autoConnect("M5StackAP");

  //if you get here you have connected to the WiFi
  ipadr = WiFi.localIP();
  Serial.println("connected!");
  Serial.println(WiFi.SSID());
  Serial.println(ipadr);

  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextColor(GREEN);
  M5.Lcd.setCursor(0, 0);

  M5.Lcd.println("Wifi Connected!");
  M5.Lcd.println("");
  M5.Lcd.println(" SSID: " + WiFi.SSID());
  M5.Lcd.println(" IP adrs: " + (String)ipadr[0] + "." + (String)ipadr[1] + "." + (String)ipadr[2] + "." + (String)ipadr[3]);

  delay(500);

  // M5.Lcd.setRotation(1);
  M5.Lcd.fillScreen(TFT_BLACK);

  analogMeter(); // Draw analogue meter

  M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
  M5.Lcd.drawCentreString("HOLD", 70, 220, 2);
  M5.Lcd.drawCentreString("IFTTT", 160, 220, 2);

  updateTime = millis(); // Next update time
}

void loop() {
  if (updateTime <= millis()) {
    updateTime = millis() + interval; // Update interval

    if(Hold == false){
      Vread = analogRead(ADC_PIN0);  // read data from CH1
    }

    // Convert the read data into voltage
    if(Vread < 5){
      Vdc = 0;
    }else if(Vread <= 1084){
      Vdc = 0.11 + (0.89 / 1084) * Vread;
    }else if(Vread <= 2303){
      Vdc = 1.0 + (1.0 / (2303 - 1084)) * (Vread - 1084);
    }else if(Vread <= 3179){
      Vdc = 2.0 + (0.7 / (3179 - 2303)) * (Vread - 2303);
    }else if(Vread <= 3659){
      Vdc = 2.7 + (0.3 / (3659 - 3179)) * (Vread - 3179);
    }else if(Vread <= 4071){
      Vdc = 3.0 + (0.2 / (4071 - 3659)) * (Vread - 3659);
    }else{
      Vdc = 3.2;
    }

    // Auto range switching
    if(Vdc > 3.0 && RANGE < 2){
      RANGE = RANGE + 1;
      RangeChange = true;
    }

    if(Vdc < 0.75 && RANGE > 0){
      RANGE = RANGE - 1;
      RangeChange = true;
    }

    switch (RANGE){
      case 0:
        PARA_RES = TERM_RES1;
        MeterLabel[0] = "0";
        MeterLabel[1] = "1";
        MeterLabel[2] = "2";
        MeterLabel[3] = "3";
        MeterLabel[4] = "4";
        digitalWrite(RANGE_PIN1, LOW); 
        digitalWrite(RANGE_PIN1, LOW); 
        VdcDisp = VdcCalc * (100 / 4);
        break;
      case 1:
        PARA_RES = 1/ ((1 / TERM_RES1) + (1 / TERM_RES2));
        MeterLabel[0] = "0";
        MeterLabel[1] = "2.5";
        MeterLabel[2] = "5";
        MeterLabel[3] = "7.5";
        MeterLabel[4] = "10";
        digitalWrite(RANGE_PIN1, HIGH); 
        digitalWrite(RANGE_PIN2, LOW); 
        VdcDisp = VdcCalc * (100 / 10);
        break;
      case 2:
        PARA_RES = 1/ ((1 / TERM_RES1) + (1 / TERM_RES2) + (1 / TERM_RES3));
        MeterLabel[0] = "0";
        MeterLabel[1] = "5";
        MeterLabel[2] = "10";
        MeterLabel[3] = "15";
        MeterLabel[4] = "20";
        digitalWrite(RANGE_PIN1, HIGH); 
        digitalWrite(RANGE_PIN2, HIGH); 
        VdcDisp = VdcCalc * (100 / 20);
        break;
    }

    VdcCalc = Vdc / PARA_RES * (SERIES_RES + PARA_RES); 

    if(VdcCalc <= 20.0){
      M5.Lcd.setTextColor(TFT_BLACK, TFT_WHITE);
    }else{
      M5.Lcd.setTextColor(TFT_RED, TFT_WHITE);
    }
    M5.Lcd.drawRightString(String(VdcCalc), M_SIZE*40, M_SIZE*(119 - 20), 2);

    if(VdcDisp > VdcLCD + 1){
      VdcLCD = VdcLCD + 1;
    }else if(VdcDisp < VdcLCD - 1){
      VdcLCD = VdcLCD - 1;      
    }

    M5.update();

    Serial.println(String(Vread) + ", " + String(VdcCalc) + "Vdc");

    if(M5.BtnA.wasPressed()){
      Hold  = !Hold;
      if(Hold == true){
        M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
      }else{
        M5.Lcd.setTextColor(TFT_BLACK, TFT_BLACK);
      }
      M5.Lcd.drawCentreString("DATA HOLDING...", 160, 180, 2);
    }

    if(M5.BtnB.wasPressed()){
      sendIFTTT(makerEvent, String(VdcCalc), "data2", "data3");
      M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
      M5.Lcd.drawCentreString("DATA SENDING...", 160, 180, 2);
      delay(1000);
      M5.Lcd.setTextColor(TFT_BLACK, TFT_BLACK);
      M5.Lcd.drawCentreString("DATA SENDING...", 160, 180, 2);
    }

    if(RangeChange == true){
      M5.Speaker.tone(NOTE, 1);
      analogMeter();
      old_analog =  -999;
      RangeChange = false;
    }

    plotNeedle(VdcLCD, 0); // It takes between 2 and 12ms to replot the needle with zero delay

  }
}

// #########################################################################
//  Draw the analogue meter on the screen
// #########################################################################
void analogMeter(){

  // Meter outline
  M5.Lcd.fillRect(0, 0, M_SIZE*239, M_SIZE*126, TFT_GREY);
  M5.Lcd.fillRect(5, 3, M_SIZE*230, M_SIZE*119, TFT_WHITE);

  M5.Lcd.setTextColor(TFT_BLACK);  // Text colour

  // Draw ticks every 5 degrees from -50 to +50 degrees (100 deg. FSD swing)
  for (int i = -50; i < 51; i += 5) {
    // Long scale tick length
    int tl = 15;

    // Coordinates of tick to draw
    float sx = cos((i - 90) * 0.0174532925);
    float sy = sin((i - 90) * 0.0174532925);
    uint16_t x0 = sx * (M_SIZE*100 + tl) + M_SIZE*120;
    uint16_t y0 = sy * (M_SIZE*100 + tl) + M_SIZE*140;
    uint16_t x1 = sx * M_SIZE*100 + M_SIZE*120;
    uint16_t y1 = sy * M_SIZE*100 + M_SIZE*140;

    // Coordinates of next tick for zone fill
    float sx2 = cos((i + 5 - 90) * 0.0174532925);
    float sy2 = sin((i + 5 - 90) * 0.0174532925);
    int x2 = sx2 * (M_SIZE*100 + tl) + M_SIZE*120;
    int y2 = sy2 * (M_SIZE*100 + tl) + M_SIZE*140;
    int x3 = sx2 * M_SIZE*100 + M_SIZE*120;
    int y3 = sy2 * M_SIZE*100 + M_SIZE*140;

    // Short scale tick length
    if(i % 25 != 0){
      tl = 8;
    }

    // Recalculate coords incase tick lenght changed
    x0 = sx * (M_SIZE*100 + tl) + M_SIZE*120;
    y0 = sy * (M_SIZE*100 + tl) + M_SIZE*140;
    x1 = sx * M_SIZE*100 + M_SIZE*120;
    y1 = sy * M_SIZE*100 + M_SIZE*140;

    // Draw tick
    M5.Lcd.drawLine(x0, y0, x1, y1, TFT_BLACK);

    // Check if labels should be drawn, with position tweaks
    if(i % 25 == 0){
      // Calculate label positions
      x0 = sx * (M_SIZE*100 + tl + 10) + M_SIZE*120;
      y0 = sy * (M_SIZE*100 + tl + 10) + M_SIZE*140;
      switch (i / 25){
        case -2: M5.Lcd.drawCentreString(MeterLabel[0], x0, y0 - 12, 2); break;
        case -1: M5.Lcd.drawCentreString(MeterLabel[1], x0, y0 - 9, 2); break;
        case 0: M5.Lcd.drawCentreString(MeterLabel[2], x0, y0 - 7, 2); break;
        case 1: M5.Lcd.drawCentreString(MeterLabel[3], x0, y0 - 9, 2); break;
        case 2: M5.Lcd.drawCentreString(MeterLabel[4], x0, y0 - 12, 2); break;
      }
    }

    // Now draw the arc of the scale
    sx = cos((i + 5 - 90) * 0.0174532925);
    sy = sin((i + 5 - 90) * 0.0174532925);
    x0 = sx * M_SIZE*100 + M_SIZE*120;
    y0 = sy * M_SIZE*100 + M_SIZE*140;
    // Draw scale arc, don't draw the last part
    if (i < 50) M5.Lcd.drawLine(x0, y0, x1, y1, TFT_BLACK);
  }

  M5.Lcd.drawString("Vdc", M_SIZE*(5 + 230 - 40), M_SIZE*(119 - 20), 2); // Units at bottom right
  M5.Lcd.drawCentreString("Vdc", M_SIZE*120, M_SIZE*70, 4); // Comment out to avoid font 4
  M5.Lcd.drawRect(5, 3, M_SIZE*230, M_SIZE*119, TFT_BLACK); // Draw bezel line

  plotNeedle(0, 0); // Put meter needle at 0
}

// #########################################################################
// Update needle position
// This function is blocking while needle moves, time depends on ms_delay
// 10ms minimises needle flicker if text is drawn within needle sweep area
// Smaller values OK if text not in sweep area, zero for instant movement but
// does not look realistic... (note: 100 increments for full scale deflection)
// #########################################################################
void plotNeedle(int value, byte ms_delay){

//  M5.Lcd.setTextColor(TFT_BLACK, TFT_WHITE);
//  char buf[8]; dtostrf(value, 4, 0, buf);
//  M5.Lcd.drawRightString(buf, M_SIZE*40, M_SIZE*(119 - 20), 2);

  // Limit value to emulate needle end stops
  if(value < -10){
    value = -10;
  }

  if(value > 110){
    value = 110;
  }

  // Move the needle until new value reached
  while(!(value == old_analog)){
    if(old_analog < value){
       old_analog++;
    }else{
       old_analog--;
    }

    // Update immediately if delay is 0
    if(ms_delay == 0){
      old_analog = value;
    }

    float sdeg = map(old_analog, -10, 110, -150, -30); // Map value to angle
    // Calcualte tip of needle coords
    float sx = cos(sdeg * 0.0174532925);
    float sy = sin(sdeg * 0.0174532925);

    // Calculate x delta of needle start (does not start at pivot point)
    float tx = tan((sdeg + 90) * 0.0174532925);

    // Erase old needle image
    M5.Lcd.drawLine(M_SIZE*(120 + 20 * ltx - 1), M_SIZE*(140 - 20), osx - 1, osy, TFT_WHITE);
    M5.Lcd.drawLine(M_SIZE*(120 + 20 * ltx), M_SIZE*(140 - 20), osx, osy, TFT_WHITE);
    M5.Lcd.drawLine(M_SIZE*(120 + 20 * ltx + 1), M_SIZE*(140 - 20), osx + 1, osy, TFT_WHITE);

    // Re-plot text under needle
    M5.Lcd.setTextColor(TFT_BLACK);
    M5.Lcd.drawCentreString("Vdc", M_SIZE*120, M_SIZE*70, 4); // // Comment out to avoid font 4

    // Store new needle end coords for next erase
    ltx = tx;
    osx = M_SIZE*(sx * 98 + 120);
    osy = M_SIZE*(sy * 98 + 140);

    // Draw the needle in the new postion, magenta makes needle a bit bolder
    // draws 3 lines to thicken needle
    M5.Lcd.drawLine(M_SIZE*(120 + 20 * ltx - 1), M_SIZE*(140 - 20), osx - 1, osy, TFT_RED);
    M5.Lcd.drawLine(M_SIZE*(120 + 20 * ltx), M_SIZE*(140 - 20), osx, osy, TFT_MAGENTA);
    M5.Lcd.drawLine(M_SIZE*(120 + 20 * ltx + 1), M_SIZE*(140 - 20), osx + 1, osy, TFT_RED);

    // Slow needle down slightly as it approaches new postion
    if(abs(old_analog - value) < 10){
      ms_delay += ms_delay / 5;
    }

    // Wait before next update
    delay(ms_delay);
  }
}

// #########################################################################
// load IFTTT config from SD
// #########################################################################
void loadConfig(String cfgFile){

  String readLineDummy[100];
  String readLine = "";

  String eventDummy;
  String dataDummy;

  int i = 0;
  int lineCount;

  // open the file for reading:
  myFile = SD.open(cfgFile);

  if (myFile) {
    Serial.println(cfgFile + ":");

    // read from the file until there's nothing else in it:
    while (myFile.available()) {
      readLine.concat(char(myFile.read()));
      if(readLine.endsWith("\n")){
        readLineDummy[i] = readLine.substring(0, readLine.indexOf("\n", 0) - 1);
        readLine = "";
        i++;
      }
    }
    lineCount = i;
    // close the file:
    myFile.close();
  } else {
    // if the file didn't open, print an error:
    Serial.println("error opening " + cfgFile);
    M5.Lcd.setTextColor(TFT_RED, TFT_BLACK);
    M5.Lcd.setCursor(10, 25);
    M5.Lcd.print("error opening " + cfgFile);
  }

  for(i = 0; i < lineCount; i++){
    if(readLineDummy[i].length() > 0){
      eventDummy = readLineDummy[i].substring(0, readLineDummy[i].indexOf("=", 0) - 1);
      eventDummy.trim();
      dataDummy = readLineDummy[i].substring(readLineDummy[i].indexOf("=", 0) + 1, readLineDummy[i].length());
      dataDummy.trim();

      if(eventDummy == "makerKey"){
        makerKey = dataDummy;
      }

      if(eventDummy == "Event1"){
        Event[0] = dataDummy;
      }
      if(eventDummy == "Event2"){
        Event[1] = dataDummy;
      }
      if(eventDummy == "Event3"){
        Event[2] = dataDummy;
      }

      if(eventDummy == "send1"){
        if(dataDummy == "true"){
          sendFlag[0] = true;
        }else{
          sendFlag[0] = false;
        }
      }
      if(eventDummy == "send2"){
        if(dataDummy == "true"){
          sendFlag[1] = true;
        }else{
          sendFlag[1] = false;
        }
      }
      if(eventDummy == "send3"){
        if(dataDummy == "true"){
          sendFlag[2] = true;
        }else{
          sendFlag[2] = false;
        }
      }

    }
  }

  Serial.println("makerKey = " + makerKey);
  for(i = 0; i < 3; i++){
    Serial.println("Event" + String(i + 1) + " = " +Event[i]);
    Serial.println("send" + String(i + 1) + " = " + String(sendFlag[i]));
  }
}

// #########################################################################
// send trigger and data to IFTTT
// #########################################################################
void sendIFTTT(String event, String value1, String value2, String value3){

  Serial.println("\nStarting connection to server...");
  if(!client.connect(server, 80)){
    Serial.println("Connection failed!");
    return;
  }else{
    Serial.println("Connected to " + String(server));

    // Make a HTTP request:
    String url = "/trigger/" + event + "/with/key/" + makerKey;
    url += "?value1=" + value1 + "&value2=" + value2 + "&value3=" + value3;

    client.println("POST " + url + " HTTP/1.1");
    client.print("Host: ");
    client.println(server);

    Serial.println(url);
    delay(100);

    client.println("Connection: close");
    client.println();

    Serial.print("Waiting for response ");
    int count = 0;
    while (!client.available()){
      delay(50); //
      Serial.print(".");
      count++;
      if (count > 20 * 20){ // about 20s
        Serial.println("(send) failed!");
        return;
      }
    }

    // read incoming bytes from the server
    while (client.available()){
      char c = client.read();
      Serial.write(c);
    }

    // if the server's disconnected, stop the client:
    if (!client.connected()){
      Serial.println();
      Serial.println("disconnecting from server.");
      client.stop();
    }
  }
  
  client.stop();
  Serial.println("closing connection");
  Serial.println("");
}

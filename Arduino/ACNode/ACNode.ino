/**************************************************************************/
/*! 

*/
/**************************************************************************/

#include <PN532_HSU.h>
#include <PN532.h>
#include "ACNode.h"
      
PN532_HSU pn532hsu(Serial);
PN532 nfc(pn532hsu);

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

extern "C" {
#include "user_interface.h"
}


MDNSResponder mdns;

WiFiClient client;
const int httpPort = 80;

String lead_display = "CCHS-AC 1.0 - ";
String machine_name = "Unregistered"; // this is set by our status page.
Machine this_machine;

#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, LED, NEO_GRB + NEO_KHZ800);


ESP8266WebServer server ( 80 );

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C	lcd(0x27,2,1,0,4,5,6,7); // 0x27 is the I2C bus address for an unmodified backpack

ButtonLocal buttons[2];

String system_display[10];
String current_display[4];
boolean update_display = true;
String current_card = "dummy";
int card_status = 0; // 0 is off, -1 is bad card, 1 is valid.

unsigned long card_check = 0;
unsigned long button_check = 0;
unsigned long machine_check = 0;

boolean eStop = false;
boolean showStatus = false;

RunTime user_time;

// Create a struct for users.
Users user_list;

String line; // This is the return line from http connection.

void setup(void) {

  pinMode(relay,OUTPUT);

  // Unfortunately the relay I'm using is LOW active.
  // This is kind of bad....
  // It flicks on for a second on boot.
  digitalWrite(relay, HIGH);
  
  Wire.begin();

  WiFi.begin ( ssid, password );

  //Serial.begin(9600);

  lcd.setBacklightPin(3,POSITIVE);
  lcd.setBacklight(1);
    
  lcd.begin (20,4); // for 16 x 2 LCD module
  lcd.setBacklightPin(3,POSITIVE);
  lcd.setBacklight(1);

  // set up our buttons
  buttons[0].pin = 0;
  buttons[0].colour = "green"; // System Status.
  buttons[0].state = false;
  buttons[0].oldState = false;
  buttons[0].lastDebounce = millis();
  buttons[2].pin = 13;
  buttons[2].colour = "trigger"; // Summon Monkey
  buttons[2].state = false;
  buttons[2].oldState = false;
  buttons[2].lastDebounce = millis();

  user_time.minutes = 0;
  user_time.hours = 0;
  user_time.last_update = 0;

  this_machine.name = machine_name;
  this_machine.active = 0;
  this_machine.level = 255;

  pinMode(buttons[0].pin,INPUT);
  pinMode(buttons[1].pin,INPUT);
  pinMode(buttons[2].pin,INPUT);
  
  nfc.begin();

  // lets show some nice boot messages.
  lcd.home();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(F("Booting CCHS-AC 1.0"));
  updateSystemDisplay("Booting CCHS-AC 1.0");
  lcd.setCursor(0,1);

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    lcd.print(F("No PN53x board"));
    updateSystemDisplay("PN53x board found");
    //while (1); // halt
  } else {
    lcd.print(F("PN53x board found"));
    updateSystemDisplay("PN53x board found");
  }

  lcd.setCursor(0,2);
  int server_count = 0;
  while (WiFi.status() != WL_CONNECTED && server_count < 30) {
    delay(500);
    lcd.print(".");
    server_count++;
  }
  
  lcd.setCursor(0,2);
  if (server_count >= 30) {
    lcd.print("Could not set IP");
    updateSystemDisplay("Could not set IP");
  } else {
    lcd.print(WiFi.localIP());
    String tempIP = String(WiFi.localIP()[0]) + "." +
                    String(WiFi.localIP()[1]) + "." +
                    String(WiFi.localIP()[2]) + "." +
                    String(WiFi.localIP()[3]);
    updateSystemDisplay(tempIP);

    Serial.println(tempIP);

    lead_display += machine_name + " - ";
    lead_display += tempIP;
  }
  
  
  

  lcd.setCursor(0,3);
  server_count = 0;
  while (!client.connect(host, httpPort) && server_count < 10) {
    delay(500);
    lcd.print(".");
    server_count++;
  }
  lcd.setCursor(0,3);
  if (server_count >= 10) {
    lcd.print("Server failed");
    updateSystemDisplay("Server Not Found");
  } else {
    lcd.print("Server Success");
    updateSystemDisplay("Server Connected");
  }

  if (mdns.begin("esp8266", WiFi.localIP())) {
    updateSystemDisplay("MDNS responder started");
  }

  server.on ( "/", handleRoot );
  server.on ( "", handleRoot );
  server.begin();

  line.reserve(256);


  // once we have network up, check if the monkeys button is active in the database.
  //hrm. This absolutely refuses to work....
  machineStatus(false);
  
  delay(2000);

  // configure board to read RFID tags
  nfc.SAMConfig();

  pixels.begin();
  
}


void loop(void) {

  //mdns.update();
  server.handleClient();

  // Unlock the machine
  // hey, is this machine even active?!
  if(!eStop && this_machine.active && card_status > 0 && user_list.level >= this_machine.level) {
    // we are in GO mode.
    digitalWrite(relay, LOW);
  } else {
    digitalWrite(relay, HIGH);
  }

  // check with server if card is available.
  if (card_check < millis() - 2000) {
    if(pixels.getPixelColor(0) == pixels.Color(0,0,255))
      pixels.setPixelColor(0, pixels.Color(0,255,0));
    else
      pixels.setPixelColor(0, pixels.Color(0,0,255));
    pixels.show();
    checkForCard(); // see if the card status has changed.
    card_check = millis();
  }

  if (machine_check + 60000 < millis()) {
    // check the machine Status every minute
    machineStatus(false);
    machine_check = millis();
  }

  

  // debounce the buttons.
  for (int i = 0; i<3;i++) {
    if(debounce(i))
      buttonMenu(i);
  }


  if(user_time.last_update + 60000 <= millis()) {
    updateTime();
    user_time.last_update = millis();
    update_display = true;
  }
 

  // log message

  // Display stuff.
  if(update_display) {
    String ac_status;
    
    lcd.clear();
    lcd.print("CCHS-AC 1.0");
    //lcd.print(String(system_get_free_heap_size()));
    //lcd.print(this_machine.name);

    
    if(eStop)
      ac_status = "  [ESTOP]";
    

    // only show this stuff if we're not on status screen
    if (!showStatus) {
      // Update the status display the summon monkeys lock
      // Only do this if we actually have an IP address.
      if(WiFi.status() != WL_CONNECTED) {
        ac_status = " [LOCKED]";
          lcd.setCursor(0,1);
          lcd.print("No IP address.");
      } else if(!this_machine.active) {
        ac_status = " [LOCKED]";
          lcd.setCursor(0,2);
          lcd.print("Machine Disabled.");
          lcd.setCursor(0,3);
          lcd.print("Monkeys Notified.");
        pixels.setPixelColor(0, pixels.Color(255,0,0));
        pixels.show();
      }

      if(card_status == -1) {
        ac_status = "[BADCARD]";
        pixels.setPixelColor(0, pixels.Color(255,0,0));
        pixels.show();
      } else if(card_status == 0) {
        if(this_machine.active)
          ac_status = " [NOCARD]";
          pixels.setPixelColor(0, pixels.Color(0,0,255));
          pixels.show();
      } else {
        if(user_list.level >= this_machine.level) {
          //ac_status = user_list.level;
          ac_status = " [ACTIVE]";
          pixels.setPixelColor(0, pixels.Color(0,255,0));
          pixels.show();
        } else {
          // We don't meet the requirements for this machine.
          ac_status = " [INACTIVE]";
          pixels.setPixelColor(0, pixels.Color(255,0,0));
          pixels.show();
        }
      }

      if(card_status > 0) {
        lcd.setCursor(0,1);
        lcd.print(user_list.username.substring(0,14));


        lcd.setCursor(18,1);
        if(user_time.minutes < 10)
                 lcd.print("0");
        lcd.print(user_time.minutes);

        lcd.setCursor(15,1);
        if(user_time.hours < 10)
          lcd.print("0");
        lcd.print(user_time.hours);
        lcd.print(":");
        
      }

      // Current machine Status
      lcd.setCursor(11,0);
      lcd.print(ac_status);
    }
    
    
    update_display = false;
  }
  
}

void handleRoot() {
  String tempReturn = "Hello from " + machine_name +"\n";
  tempReturn += "Free Ram: ";
  tempReturn += system_get_free_heap_size();
  tempReturn += "\n";
  tempReturn += "Last 10 System messages are:\n";
  for (int i = 0; i < 10; i++){
    tempReturn += system_display[i];
    tempReturn += "\n";
  }
  server.send(200, "text/plain", tempReturn);
}

void updateSystemDisplay(String newLine) {
 // so, basically a pop system.
 // first iteration is probably going to leak like a sieve...
 // Update our back end database here too!
 // a new line has been received.

 for (int i = 0; i < 9; i++){
  system_display[i] = system_display[i+1];
 }
 system_display[9] = newLine.substring(0,20);
}

// Display the system log on the main screen.
void displaySystem(){
  lcd.clear();

  //Always show the IP address
  lcd.setCursor(0,0);
  lcd.print(machine_name + " ");
  lcd.setCursor(0,1);
  lcd.print(WiFi.localIP());
  
  for (int i = 2; i < 4; i++){
    lcd.setCursor(0,i);
    lcd.print(system_display[i+6]);
  }
  update_display = false;
}

void updateTime(){

  user_time.minutes++;

  if(user_time.minutes >= 60) {
    user_time.hours++;
    user_time.minutes = 0;
    lcd.setCursor(0,2);
    lcd.print("Maybe you should take");
    lcd.setCursor(0,3);
    lcd.print(" a break?");
  }
}

// What do our buttons do?
void buttonMenu(int array_position) {

  if (buttons[array_position].colour == "green") {
    if(buttons[array_position].state == true) {
      showStatus = true;
      displaySystem();
    } else {
      showStatus = false;
      update_display = true;
    }
  } else if (buttons[array_position].colour == "trigger") {
    if(buttons[array_position].state == true) {
      this_machine.active = false;
      machineStatus(true);
      updateSystemDisplay("Machine Disabled.");
      update_display = true;
    } else if(user_list.level > 1) {
      // We're an admin user, so the Monkey Call can be disabled.
      // Ugh, has to be on a state change.
      this_machine.active = true;
      machineStatus(true);
      updateSystemDisplay("Machine Enabled.");
      update_display = true;
    }
  }
}

boolean debounce(int array_position){
  long debounceDelay = 50;
  int reading = digitalRead(buttons[array_position].pin);
  boolean stateChange = false;

  if (reading != buttons[array_position].oldState) 
    buttons[array_position].lastDebounce = millis();

  if ( (millis() - buttons[array_position].lastDebounce) > debounceDelay){
    if(reading != buttons[array_position].state) {
      buttons[array_position].state = reading;

       stateChange = true;
    }
  }

  buttons[array_position].oldState = reading;

  // return true when state has changed.
  return stateChange;
}

// lets just pull the card check stuff out in to it's own function.
// Check if we have a card.
void checkForCard() {
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;     

 
  // Check if we have a card.
  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
    // Read the new card UID in to memory.
    String temp;
    nfc.PrintHex(uid, uidLength);
    for (uint8_t i = 0; i < uidLength; i++) {
        temp += String(lowByte(uid[i]), HEX);
    }

    // is this card different from our current card?
    if (current_card != temp) {
      current_card = temp; // update our current card.

      // if the connection is down, restart it.
      if(!client.connected()) {
        client.connect(host, httpPort);
        updateSystemDisplay("Connection Re-established.");
      }

      // This will send the request to the server
      client.println("GET /machines/is_card_valid/" + machine_name + "/" + current_card + "/ HTTP/1.1");
      client.println("Host: " + String(host));
      //client.println("Connection: close");
      client.println();

      delay(100); // wait long enough for the server to generate the response

      boolean test_case = true; // stops the code from continuing to search unexpectedly.
      while(client.available()){
        line = client.readStringUntil('\r');

        pixels.setPixelColor(0, pixels.Color(255,0,0));
        pixels.show();

        // Do we have a 404?  It's either no machine or no card in database.
        // no point continuing here...
        if(line.endsWith("404 NOT FOUND")) {
          card_status = -1;
          test_case = false;
          updateSystemDisplay("Card:"+temp+":Invalid");
        } else if(test_case) {
          // don't have a 404
          // must be a valid card!
          //updateSystemDisplay("Card:"+temp+"-Identfied.");
          card_status = 1;
          

          for (int i = 0; i < line.length(); i++) {
            //lcd.setCursor(0,2);
            //lcd.print(i);
            String line_start = line.substring(i,line.indexOf(":",i));
  
            if(line_start == "\"username\"") {
              user_list.username = line.substring(line.indexOf(":",i)+2,line.indexOf(",",i)-1);
              updateSystemDisplay("Card:"+temp+":"+"Valid");
              updateSystemDisplay("User:"+user_list.username);
            } else if(line_start == "\"level\"") {
              user_list.level = line.substring(line.indexOf(":",i)+2,line.indexOf(",",i)-1).toInt()-0;
              updateSystemDisplay("Level:"+String(user_list.level));
            } else if(line_start == "\"levelName\"") {
              user_list.levelName = line.substring(line.indexOf(":",i)+2,line.indexOf(",",i)-1);
              updateSystemDisplay("LevelName:"+user_list.levelName);
            }
  
            // kind of feel like I should be able to advance i here
            // so it can progress through the string faster...
            // last time I tried, it went out of bounds and crashed....
            
          }
          
        }

      }

      // update the display
      update_display = true;
      // reset how long we've been active.
      user_time.minutes = 0;
      user_time.hours = 0;
    }
    
  } else {
    // We have no card!
    if(card_status != 0) {
      // we used to have a card.  The situation has changed!
      current_card = "dummy"; // if we don't reset it, we'll ignore the next time we see the same card.
      card_status = 0;
      update_display = true;
    }
  }
}

// Check the machine status and update stuff.
void machineStatus(boolean update_status) {

  updateSystemDisplay("Checking Machine Status.");

  // if the connection is down, restart it.
  if(!client.connected()) {
    client.connect(host, httpPort);
    updateSystemDisplay("Connection Re-established.");
  }

  // if we have a valid card/user, we should update that the system is still in use.
  if(card_status > 0) {
   client.println("GET /machines/session/" + current_card + "/ HTTP/1.1");
   delay(1000);
   while(client.available()){
    client.readStringUntil('\r'); // really just clearing the buffer
   }
  }

  if(update_status) {
    client.println("GET /machines/status/" + machine_name + "/" + this_machine.active + "/ HTTP/1.1");
  } else {
    client.println("GET /machines/status/" + machine_name + "/ HTTP/1.1");
  }
  client.println("Host: " + String(host));
  client.println();

  delay(1000);

  boolean test_case = true;
  while(client.available()){
  
    line = client.readStringUntil('\r');
    if(line.endsWith("404 NOT FOUND")) {
      updateSystemDisplay("Machine Not Found");
    } else if(test_case) {
      for (int i = 0; i < line.length(); i++) {
        String line_start = line.substring(i,line.indexOf(":",i));

        if(line_start == "\"name\"") {
          if(machine_name != line.substring(line.indexOf(":",i)+2,line.indexOf(",",i)-1)) {
            machine_name = line.substring(line.indexOf(":",i)+2,line.indexOf(",",i)-1); 
            updateSystemDisplay("MachineName:"+machine_name);
          }
        } else if(line_start == "\"active\"") {
          if(line.substring(line.indexOf(":",i)+2,line.indexOf(",",i)-1) == "True") 
            this_machine.active = 1;
          else
            this_machine.active = 0;
          updateSystemDisplay("MachineActive:"+String(this_machine.active));
        } else if(line_start == "\"minimum_level\"") {
          this_machine.level = (line.substring(line.indexOf(":",i)+2,line.indexOf(",",i)-1)).toInt()-0;
          updateSystemDisplay("Level:"+String(this_machine.level));
        }

        // kind of feel like I should be able to advance i here
        // so it can progress through the string faster...
        // last time I tried, it went out of bounds and crashed....
        
      }
    }
    update_display = true;
  }
}



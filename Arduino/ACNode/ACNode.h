struct ButtonLocal {
  int pin;
  String colour;
  boolean state;
  boolean oldState;
  unsigned long lastDebounce;
};

struct Users {
  String uid;
  String username;
  byte level;
  String levelName;
};

struct Machine {
  String name;
  boolean active;
  byte level;
};

struct RunTime {
  byte minutes;
  byte hours;
  long last_update;
};

// Define our i2c pins
//const byte sclPin = 5;
const byte sclPin = 14;
//const byte sdaPin = 4;
const byte sdaPin = 0;

// Where is our relay
//const int relay = 15;
const int relay = 2;

// Where is our neopixel
const int LED = 4;

// Set up our wifi
//const char *ssid = "CCHS-2.4";
//const char *password = "hackmelb";
const char *ssid = "_mage_net";
const char *password = "worldsgreates";
//const char* host = "192.168.0.202";
const char* host = "192.168.1.20";




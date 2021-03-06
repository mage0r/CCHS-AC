void machineStatus(boolean update_status);
void checkForCard();
boolean debounce(int array_position);
void buttonMenu(int array_position);
void updateTime();
void displaySystem();
void updateSystemDisplay(String newLine);
void handleRoot();


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

// Where is our relay
const int relay = 13;


// Where is our neopixel
const int LED = 2;

// Set up our wifi
//const char *ssid = "CCHS-WiFi";
//const char *password = "hackmelb";
const char *ssid = "_mage_net";
const char *password = "worldsgreates";
//const char* host = "192.168.0.202";
const char* host = "192.168.1.20";




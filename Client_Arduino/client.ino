#include <SoftwareSerial.h>

const char* host = "192.168.1.80";

char START[] = "AT+CIPSTART=\"TCP\",\"192.168.1.80\",80";
char GET[]   = "GET /alive HTTP/1.1\r\nHost: 192.168.1.80\r\n\r\n";
// I wanted to be able to do POSTs dynamically, but was running into too many problems with not enough time.
char POST[]  = "POST /detections/new HTTP/1.1\r\nHost: 192.168.1.80\r\nContent-Length: 41\r\nContent-Type: application/json\r\n\r\n{\"Time_Recorded\":0, \"Sensor_Name\":\"Bruh\"}";
char CLOSE[] = "AT+CIPCLOSE";

unsigned int MAX_TRANSMIT_SIZE = 300;

// Debug codes, useful for if arduino is plugged into a power pack and not a computer.
class DebugCodes {
 public:
  static void Init() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
  }
  static void Throw(unsigned int code) {
    while (true) {
      for (unsigned int i = 0; i < code; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(150);
        digitalWrite(LED_BUILTIN, LOW);
        delay(150);
      }
      delay(1850);
    }
  }
};

// ESP serial on lines 2 and 3 (D2, D3)
SoftwareSerial ESPSerial(2, 3); // RX | TX

// Display debug info, and return what was sent so it can be forwarded to ESP as well.
char* DebugToESP(char* l) {
  Serial.print("->ESP: ");
  Serial.println(l);
  return l;
}

// Unused, kept in here for future testing.
void DebugFromESP(char* l) {
  Serial.println("DEBUG FROM SERIAL:");
  Serial.print("<-ESP: ");
  Serial.println(l);
}

// Arduino's strcpy does not have the `pos` argument, for whatever reason.
bool strcpy2(char* buff, char* clone, unsigned int pos, unsigned int maxSize) {
  if (pos + strlen(clone) >= maxSize)
    return false;
    
  for (int i = 0; i < strlen(clone); i++) {
    buff[pos + i] = clone[i];
  }

  buff[pos + strlen(clone)] = '\0';
  
  return true;
}

// Clears the buffer so that we don't run out of dynamic memory.
void ClearESPBuffer() {
  ESPSerial.setTimeout(1);
  ESPSerial.readString();
  ESPSerial.setTimeout(5000);
}

// opens a connection. Only allows one message every ten seconds.
unsigned long lastTransmit = millis();
bool OpenConnection() {
  Serial.print("Opening...");
  while (millis() < lastTransmit + 10000) {
    delay(100);
    Serial.print(".");
  }
  Serial.println("!");
  lastTransmit = millis();
  Serial.print("Transmit at ");
  Serial.println(lastTransmit);
  ESPSerial.println(DebugToESP(CLOSE));
  delay(2000);
  ESPSerial.println(DebugToESP(START));
  return ESPSerial.find("OK");
}

// Sends a TCP buffer (for http requests)
bool SendBuffer(const char* buff) {
  // CIPSEND: tell ESP how many bytes we want to send over TCP
  char itoaBuffer[3];
  itoa(strlen(buff), itoaBuffer, 10);
  char buffLen[15] = "AT+CIPSEND=";
  strcpy2(buffLen, itoaBuffer, 11, 15);
  ESPSerial.println(DebugToESP(buffLen));

  // If ESP is ready for data transmission
  if (ESPSerial.find("OK")) {
    // Send the buffer
    ESPSerial.print(DebugToESP(buff));

    // Wait for ESP to acknowledge it received all the data.
    if (!ESPSerial.find("Recv")) {
      // if 5 seconds pass, ESP did not receive something.
      Serial.println("ESP expected different buffer length.");
      Serial.println("Flooding ESP to kill buffer...");
      for (int i = 0; i < 100; i++)
        ESPSerial.println("AAAAAAAAAAAAAAAAAAAAAAAA");
      Serial.println("Flooded.");
      return false;
    } 
    
    Serial.println("Buffer was sent to ESP, waiting for ESP response.");
    bool findSend = ESPSerial.find("SEND OK");
    bool findIPD = ESPSerial.find("+IPD,");
    Serial.print("SEND OK: ");
    Serial.println(findSend);
    Serial.print("IPD: ");
    Serial.println(findIPD);
    return findSend && findIPD;
  }
  
  Serial.println("ESP does not want to send stuff yet, it is unhappy for some reason.");
  return false;
}

// determine the response code from ESP's response.
int GetResponseCode() {
  ESPSerial.find("HTTP/1.1 ");
  String resp = ESPSerial.readStringUntil(' ');
  return resp.toInt();
}

// internal poster, creates a connection then transmits the POST buffer.
int _makePOST() {
  if (OpenConnection()) {
    if (SendBuffer(POST)) {
      int r = GetResponseCode();
      ESPSerial.println(DebugToESP(CLOSE));
      return r;
    }

    delay(300);
    ESPSerial.println(DebugToESP(CLOSE));
    return -2; // Failed to send buffer over air
  }
  return -1; // failed to make connection
}

// Wrapper for internal makepost - Clears buffer after making request.
int MakePostRequest() {
  int resp = _makePOST();
  ClearESPBuffer();
  return resp;
}

// internal getter, creates a connection then transmits the GET buffer.
int _makeGET() {
  if (OpenConnection()) {
    if (SendBuffer(GET)) {
      int r = GetResponseCode();
      delay(300);
      ESPSerial.println(DebugToESP(CLOSE));
      
      return r;
    }
    
    delay(300);
    ESPSerial.println(DebugToESP(CLOSE));
    return -2; // failed to send over air (or no response)
  }
  
  return -1; // failed to make connection.
}

// Wrapper for internal makeget - Clears buffer after making request.
int MakeBasicGetRequest() {
  int resp = _makeGET();
  ClearESPBuffer();
  return resp;
}

// Init arduino
void setup() {
  DebugCodes::Init();
  pinMode(4, OUTPUT); // pin 4 goes to ENABLE of ESP
  pinMode(9, INPUT);  // bottom sensor. INPUT should be LOW when detecting an object
  pinMode(10, INPUT); // top sensor. INPUT should be LOW when detecting an object

  // init serial connections
  Serial.begin(115200);
  ESPSerial.begin(9600);
  ESPSerial.setTimeout(5000);

  // reset the wireless module.
  Serial.println("Resetting module...");
  digitalWrite(4, LOW); 
  delay(200);
  digitalWrite(4, HIGH);
  delay(500);
  ESPSerial.println(DebugToESP("AT+RST"));
  Serial.println("Sent reset commands...");

  // if the ESP resets properly...
  if (ESPSerial.find("ready")) {
    Serial.println("Success.");
    
    /*
     * ESP follows basic run:
     * 1. RESET
     * 2. Attempt connection to WiFi (if info is stored in EEPROM)
     * 3. Get IP address from router
     * Before these occur, the ESP cannot transmit any information, so we must wait.
     * 
     * ESP needs to be programmed to your WiFi manually.
     * Commands to do so are as follows:
     * AT+CWLAP 
     *   Lists access points the ESP can reach.
     * AT+CWJAP=<SSID>,<PASS>
     *   Connects to an access point (and saves it's info in EEPROM).
     *   For example, if SSID is "Mom's House WIFI" and Password is "URMOM420", the command you would enter is as follows:
     *     AT+CWJAP="Mom's House WIFI","URMOM420"
     *  
     * Ensure you are using both NL and CR in the terminal.
     */
    Serial.println("Waiting for connection to wifi...");
    ESPSerial.setTimeout(30000);
    if (!ESPSerial.find("WIFI CONNECTED")) {
      Serial.println("Failed to connect in 30 seconds.");
      DebugCodes::Throw(3);
    }
    Serial.println("Got wifi, getting ip.");
    if (!ESPSerial.find("WIFI GOT IP")) {
      Serial.println("Failed to get IP in 30 seconds.");
      DebugCodes::Throw(4);
    }
    ESPSerial.setTimeout(5000); // set timeout to a more reasonable time for http requests
    Serial.println("Success!");

    // Check if the server is running.
    Serial.println("Checking connection to server...");
    int respCode = MakeBasicGetRequest();
    if (respCode == 200) {
      Serial.println("Server is online.");
      delay(4000); // ensure the ESP has time to reset itself
      Serial.println("SYSTEM STARTED.");
      delay(750);
    } else {
      Serial.print("Expected response 200, got ");
      Serial.println(respCode);
      Serial.println("Are you sure the server is running?");
      DebugCodes::Throw(2);
    }
    
  } else {
    // Failed to reset. Is there a problem with the firmware?
    Serial.println("Failure.");
    DebugCodes::Throw(1);
  }
}

// Wrapper to alarm. Kept in it's own function as I may add more to what it does later.
void alarm() {
  MakePostRequest();
}


// LOOP VARIABLES
const unsigned long MAX_OFFSET = 500; // max time offset - If time is greater than lastSense + this number, it is "timed out"
bool isCat = true;
unsigned long lastSenseBot = 0;
unsigned long lastSenseTop = 0;
bool timeOut = false;
bool firstRun = true;

void loop() {
  // Check lower sensor
  // if lower sensor triggers, either cat or human.
  bool bot = !digitalRead(9);
  bool top = !digitalRead(10);
  if (bot) {
    lastSenseBot = millis();
    Serial.print("Bottom Timer Update (");
    Serial.print(lastSenseBot);
    Serial.print(" -> ");
    Serial.print(lastSenseBot + MAX_OFFSET);
    Serial.println(")!");

    timeOut = false;
  }

  // if upper sensor is triggered, human.
  if (top) {
    lastSenseTop = millis();
    Serial.print("Top Timer Update (");
    Serial.print(lastSenseTop);
    Serial.print(" -> ");
    Serial.print(lastSenseTop + MAX_OFFSET);
    Serial.println(")!");
  }

  if ((top || lastSenseTop + MAX_OFFSET >= millis()) && lastSenseBot + MAX_OFFSET >= millis()) { // if top is on, or not past the top timeout (ie: top sensed just before bottom did), and bottom timed out
    isCat = false;
    Serial.println("Human.");
  }

  if (!timeOut && lastSenseBot + MAX_OFFSET < millis()) { // if we haven't already triggered a timeout, and we have timed out, run this.
    timeOut = true; // so we don't check again.

    if (isCat) {
      Serial.println("Cat!");
      alarm();
    } else {
      Serial.println("Not a cat...");
    }

    isCat = true; // reset; "Cat until proven otherwise" logic.
  }
}

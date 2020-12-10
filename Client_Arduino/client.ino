#include <SoftwareSerial.h>

const char* host = "192.168.1.80";

char START[] = "AT+CIPSTART=\"TCP\",\"192.168.1.80\",80";
char GET[]   = "GET /alive HTTP/1.1\r\nHost: 192.168.1.80\r\n\r\n";
char POST[]  = "POST /detections/new HTTP/1.1\r\nHost: 192.168.1.80\r\nContent-Length: 41\r\nContent-Type: application/json\r\n\r\n{\"Time_Recorded\":0, \"Sensor_Name\":\"Bruh\"}";
char CLOSE[] = "AT+CIPCLOSE";

const char* http = "HTTP/1.1";
const char* port = "80";
unsigned int MAX_TRANSMIT_SIZE = 300;

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

SoftwareSerial ESPSerial(2, 3); // RX | TX

char* DebugToESP(char* l) {
  Serial.print("->ESP: ");
  Serial.println(l);
  return l;
}

void DebugFromESP(char* l) {
  Serial.println("DEBUG FROM SERIAL:");
  Serial.print("<-ESP: ");
  Serial.println(l);
}

bool strcpy2(char* buff, char* clone, unsigned int pos, unsigned int maxSize) {
  if (pos + strlen(clone) >= maxSize)
    return false;
    
  for (int i = 0; i < strlen(clone); i++) {
    buff[pos + i] = clone[i];
  }

  buff[pos + strlen(clone)] = '\0';
  
  return true;
}

void ClearESPBuffer() {
  ESPSerial.setTimeout(1);
  ESPSerial.readString();
  ESPSerial.setTimeout(5000);
}

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

bool SendBuffer(const char* buff) {

  char itoaBuffer[3];
  itoa(strlen(buff), itoaBuffer, 10);
  char buffLen[15] = "AT+CIPSEND=";
  strcpy2(buffLen, itoaBuffer, 11, 15);
  
  ESPSerial.println(DebugToESP(buffLen));
  if (ESPSerial.find("OK")) {
    ESPSerial.print(DebugToESP(buff));
    char* findBuf = "Recv";
    
    if (!ESPSerial.find(findBuf)) {
      Serial.println("ESP expected different buffer length.");
      Serial.println("Flooding ESP...");
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

int GetResponseCode() {

  
  ESPSerial.find("HTTP/1.1 ");
  String resp = ESPSerial.readStringUntil(' ');
  return resp.toInt();
}

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

int MakePostRequest() {
  int resp = _makePOST();
  ClearESPBuffer();
  return resp;
}

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

int MakeBasicGetRequest() {
  int resp = _makeGET();
  ClearESPBuffer();
  return resp;
}

void setup() {
  DebugCodes::Init();
  pinMode(4, OUTPUT); // pin 4 goes to ENABLE
  pinMode(9, INPUT);  // bottom sensor
  pinMode(10, INPUT); // top sensor

  // init serial connections
  Serial.begin(115200);
  ESPSerial.begin(9600);
  ESPSerial.setTimeout(5000);

  // reset the wireless module.
  Serial.println("Attempting reset...");
  digitalWrite(4, LOW); 
  delay(200);
  digitalWrite(4, HIGH);
  delay(500);
  ESPSerial.println(DebugToESP("AT+RST"));
  Serial.println("Sent commands...");

  if (ESPSerial.find("ready")) {
    Serial.println("Success.");
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
    ESPSerial.setTimeout(5000);
    Serial.println("Success!");
    Serial.println("Checking connection to server...");

    int respCode = MakeBasicGetRequest();
    if (respCode == 200) {
      Serial.println("WOOT WOOT");
      delay(4000); // ensure the ESP has time to reset itself
      Serial.println("SYSTEM STARTED.");
      delay(750);
    } else {
      Serial.println("Expected response 200, got " + String(respCode));
      DebugCodes::Throw(2);
    }
    
  } else {
    Serial.println("Failure.");
    DebugCodes::Throw(1);
  }
}

void alarm() {
  MakePostRequest();
}


// LOOP VARIABLESW
const unsigned long MAX_OFFSET = 500;
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

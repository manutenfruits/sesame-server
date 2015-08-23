/*

  An Ethernet web server to allow to open multiple doors using a password-base
  authentication that relies on HMAC-SHA 256.

  Uses a trimmed down fork of Cathedrow's Cryptosuite library (https://github.com/manutenfruits/Cryptosuite)
  Uses jcw's Ethercard library (https://github.com/jcw/ethercard)

  Forked from jdotjdot's DoorDuino, adapted for the ENC28J60 chip, with enhanced
  security and more standard HTTP communication

*/


#include <EtherCard.h>
#include <sha256.h>
#include "password.h" //Password file

//HTTP codes
const char HTTP_200[] PROGMEM = "HTTP/1.0 200 OK\r\n";
const char HTTP_400[] PROGMEM = "HTTP/1.0 400 Bad Request\r\n";
const char HTTP_401[] PROGMEM = "HTTP/1.1 401 Unauthorized\r\n";
const char HTTP_404[] PROGMEM = "HTTP/1.1 404 Not Found\r\n";
const char HTTP_405[] PROGMEM = "HTTP/1.1 405 Method Not Athorized\r\n";
const char HTTP_418[] PROGMEM = "HTTP/1.1 418 I'm a teapot\r\n";
const char HTTP_420[] PROGMEM = "HTTP/1.1 420 Enhance Your Calm\r\n";

const char CORS_HEADERS[] PROGMEM = "Content-Type: application/json\r\n"
                                    "Access-Control-Allow-Origin: *\r\n"
                                    "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                                    "Access-Control-Allow-Headers: Authorization, Content-Type\r\n"
                                    "\r\n";

const char GET_METHOD[] PROGMEM = "GET";
const char OPTIONS_METHOD[] PROGMEM = "OPTIONS";
const char POST_METHOD[] PROGMEM = "POST";

//How long will the relay be opened?
#define OPENING_DELAY 250 //ms

// Array of door pins, add more or less if needed
#define NR_OF_DOORS 2
const int actionPins[NR_OF_DOORS] = { 2, 3 };
const int openPins[NR_OF_DOORS] = { 4, 5 };
const int closedPins[NR_OF_DOORS] = { 6, 7 };
boolean doorStatus[NR_OF_DOORS];

// Mac address for the arduino
const byte MAC_ADDRESS[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xF0, 0x0D };

//Buffer for responses
char response[150];

//Ethernet
#define MAX_PACKET_SIZE 600
byte Ethernet::buffer[MAX_PACKET_SIZE];

//Flag to open the door
int asyncOpenDoor = -1;

// PASSWORD defined in a header file
char * password = PASSWORD;
int passwordLen = strlen(password);

//Current nonce
unsigned long currentNonce = 0;

//Time at which the door was opened
unsigned long timerOpenDoor = 0;

struct clientInput {
  char * url = NULL;
  char * method = NULL;
  char * entity = NULL;
  int entity_id = NULL;
  char * action = NULL;
  boolean auth = NULL;
  char * nc_s = NULL;
  unsigned long nc = NULL;
  char nonce[65] = "";
};

//Parse input from client's URL
struct clientInput parseClientInput(char * packetData) {

  struct clientInput holder;

  char * authline = strstr(packetData, "Authorization:");

  holder.method = strtok(packetData, " ");
  holder.url = strtok(NULL, " ");

  if (strcmp_P(holder.method, POST_METHOD) == 0) {
    //If it's a POST, check further
    holder.entity = strtok(holder.url, "/");

    //If
    if (strcmp(holder.entity, "doors") == 0) {
      char * entity_id = strtok(NULL, "/ \n");
      holder.entity_id = strtol(entity_id, NULL, 10);
      holder.action = strtok(NULL, "/ \n");
    }
  }

  //Beginning of auth line
  holder.auth = false;

  if (authline != NULL) {
    char * digestline = strstr(authline, "Digest ");
    char * authvalues = strtok(digestline, "\n");

    const char NC_TOKEN[] PROGMEM = "nc=";
    const char NONCE_TOKEN[] PROGMEM = "nonce=\"";

    //Nonce count
    char * ncptr = strstr(authvalues, NC_TOKEN) + strlen(NC_TOKEN);
    //Actual nonce
    char * nonceptr = strstr(authvalues, NONCE_TOKEN) + strlen(NONCE_TOKEN);

    //Trim
    strtok(ncptr, " ,\n");
    strtok(nonceptr, "\"");

    holder.nc_s = ncptr;
    holder.nc = strtol(ncptr, NULL, 10);
    strncpy(holder.nonce, nonceptr, 65);
    holder.auth = true;
  }

  return holder;
}

void setup() {

  Serial.begin(57600);

  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }

  // Configure door pins as outputs
  int i;
  for (i = 0; i < NR_OF_DOORS; i++) {
    pinMode(openPins[i], INPUT);
    pinMode(closedPins[i], INPUT);

    pinMode(actionPins[i], OUTPUT);
    digitalWrite(actionPins[i], HIGH);
  }

  Serial.println(F("\n[Starting...]"));

  //Initialize Ethernet module
  if (ether.begin(sizeof Ethernet::buffer, MAC_ADDRESS) == 0)
  Serial.println(F("Failed to access Ethernet controller"));

  //Try DHCP request
  if (!ether.dhcpSetup())
  Serial.println(F("DHCP failed"));

  //Print IP information
  ether.printIp(F("IP address: "), ether.myip);
  ether.printIp(F("Netmask: "), ether.netmask);
}

boolean toggleDoor(int pin, boolean open) {

  if (open) {
    if (pin >= 0 && pin < NR_OF_DOORS) {
      Serial.print(F("Opening door "));
      Serial.print(pin);
      Serial.print(F(" on pin "));
      Serial.println(actionPins[pin]);

      digitalWrite(actionPins[pin], LOW);
      return true;
    } else {
      Serial.print(F("No door"));
      Serial.println(pin);
      return false;
    }
  } else {
    digitalWrite(actionPins[pin], HIGH);
    return true;
  }
}

void loop() {

  if (asyncOpenDoor >= 0) {
    //If door should be opened
    if (timerOpenDoor == 0) {
      //If it has not been opened yet
      if (toggleDoor(asyncOpenDoor, true)) {
        //If door exists
        timerOpenDoor = millis();
      } else {
        //If it doesn't exist
        asyncOpenDoor = -1;
      }
    } else if (millis() - timerOpenDoor >= OPENING_DELAY){
      //If it has been opened and the time has passed
      toggleDoor(asyncOpenDoor, false);
      asyncOpenDoor = -1;
      timerOpenDoor = 0;
    }
  }

  // read door status
  int i;
  for (i = 0; i < NR_OF_DOORS; i++) {
    const int open = digitalRead(openPins[i]);
    const int closed = digitalRead(closedPins[i]);

    if (open == HIGH && closed == LOW) {
      //door is open
      doorStatus[i] = 1;
    } else if (open == LOW && closed == HIGH) {
      // door is closed
      doorStatus[i] = 0;
    } else {
      // sensor is broken
      doorStatus[i] = -1;
    }
  }

  // listen for incoming clients
  word pos = ether.packetLoop(ether.packetReceive());
  // check if valid tcp data is received
  if (pos) {

    // an http request ends with a blank line
    char* packetData = (char *) Ethernet::buffer + pos;

    unsigned int packetLength = strlen(packetData);

    //Grab request data
    struct clientInput request = parseClientInput(packetData);

    ether.httpServerReplyAck(); // send ack to the request

    boolean isGet = strcmp_P(request.method, GET_METHOD) == 0,
            isPost = strcmp_P(request.method, POST_METHOD) == 0,
            isOptions = strcmp_P(request.method, OPTIONS_METHOD) == 0;

    // Ignore queries while opening
    if (asyncOpenDoor >= 0) {
      sendCode(HTTP_420, sizeof HTTP_420, true);

    } else if (packetLength >= MAX_PACKET_SIZE) {
      //I'm a teapot
      sendCode(HTTP_418, sizeof HTTP_418, true);

    } else if (isOptions) {
      //Inform about CORS
      sendCode(HTTP_200, sizeof HTTP_200, true);

    } else if (isGet) {
      //Get door status
      sendCode(HTTP_200, sizeof HTTP_200, false);

      int jsonLength = 0;
      jsonLength += sprintf(response + jsonLength, "{\"doors\":{", i, doorStatus[i]);
      int i = 0;
      for (i = 0; i < NR_OF_DOORS; i++) {
        jsonLength += sprintf(response + jsonLength, "\"%d\": %s", i, doorStatus[i] ? "true" : "false");
        if (i < NR_OF_DOORS - 1) {
          jsonLength += sprintf(response + jsonLength, ",");
        }
      }
      jsonLength += sprintf(response + jsonLength, "},\"nonce\":%d}", currentNonce);

      memcpy(ether.tcpOffset(), response, jsonLength);
      ether.httpServerReply_with_flags(jsonLength, TCP_FLAGS_ACK_V|TCP_FLAGS_FIN_V);

    } else if (isPost) {

      if (!request.auth) {
        sendCode(HTTP_401, sizeof HTTP_401, true);
      } else {

        if (request.nc < currentNonce) {
          const int timeLength = sprintf(response, "{\"nonce\":%lu}", currentNonce);

          sendCode(HTTP_400, sizeof HTTP_400, false);
          memcpy(ether.tcpOffset(), response, timeLength);
          ether.httpServerReply_with_flags(timeLength, TCP_FLAGS_ACK_V|TCP_FLAGS_FIN_V);

        } else {

          //update time counter
          currentNonce = request.nc + 1;

          // check hash
          uint8_t *hash;
          Sha256.initHmac((uint8_t *) password, passwordLen);
          Sha256.print(request.nc_s);
          hash = Sha256.resultHmac();

          char hash_c[65];

          // convert from uint8_t to HEX str
          String tmpHash;
          String hash_s = "";

          for (int i=0; i<32; i++) {
            tmpHash = String(hash[i], HEX);
            while (tmpHash.length() < 2) {
              tmpHash = "0" + tmpHash;
            }
            hash_s += tmpHash;
          }
          hash_s.toCharArray(hash_c, 65);

          if (strcmp(request.nonce, hash_c) == 0) {

            if (request.entity_id < 0 || request.entity_id > NR_OF_DOORS) {
              sendCode(HTTP_404, sizeof HTTP_404, true);
            } else {
              asyncOpenDoor = request.entity_id;
              sendCode(HTTP_200, sizeof HTTP_200, true);
            }
          } else {
            // don't open door
            sendCode(HTTP_401, sizeof HTTP_401, true);
          }
        }
      }
    } else {
      //Method not implemented
      sendCode(HTTP_405, sizeof HTTP_405, true);
    }
  }
}

void sendCode (const char* code, long length, boolean finish) {
  memcpy_P(ether.tcpOffset(), code, length);
  memcpy_P(ether.tcpOffset() + length, CORS_HEADERS, sizeof CORS_HEADERS);

  uint8_t flags = TCP_FLAGS_ACK_V;
  if (finish) {
    flags |= TCP_FLAGS_FIN_V;
  }
  ether.httpServerReply_with_flags(length + sizeof CORS_HEADERS - 1, flags);
}

int availableMemory () {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

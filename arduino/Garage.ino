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

const char CORS_HEADERS[] PROGMEM = "Content-Type: text/plain\r\n"
                                    "Access-Control-Allow-Origin: *\r\n"
                                    "Access-Control-Allow-Methods: POST, OPTIONS\r\n"
                                    "\r\n";

//How long will the relay be opened?
#define OPENING_DELAY 1000 //ms

// Array of door pins, add more or less if needed
#define NR_OF_DOORS 2
int doorPins[NR_OF_DOORS] = { 2, 3 };

// Mac address for the arduino
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

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
    char * time_s;
    unsigned long time;
    char * hash_s;
    char hash[65];
    char * door_s;
    int door;
};

//Buffer for responses
char response[100];

//Ethernet
byte Ethernet::buffer[600];


void setup() {

    Serial.begin(57600);

     while (!Serial) {
        ; // wait for serial port to connect. Needed for Leonardo only
    }

    // Configure door pins as outputs
    int i;
    for (i = 0; i < NR_OF_DOORS; i++) {
        pinMode(doorPins[i], OUTPUT);
        digitalWrite(doorPins[i], HIGH);
    }

    Serial.println(F("\n[Starting...]"));

    //Initialize Ethernet module
    if (ether.begin(sizeof Ethernet::buffer, mac) == 0)
        Serial.println(F("Failed to access Ethernet controller"));

    //Try DHCP request
    if (!ether.dhcpSetup())
        Serial.println(F("DHCP failed"));

    //Print IP information
    ether.printIp(F("IP address: "), ether.myip);
    ether.printIp(F("Netmask: "), ether.netmask);
}

//Parse input from client's URL
struct clientInput parseClientInput(char * url) {

    struct clientInput holder;

    holder.time_s = strtok(url, "/ \n");
    holder.door_s = strtok(NULL, "/ \n");
    holder.hash_s = strtok(NULL, "/ \n");

    holder.time = strtoul(holder.time_s, NULL, 10);
    holder.door = strtol(holder.door_s, NULL, 10);
    strncpy(holder.hash, holder.hash_s, 65);

    return holder;
}

boolean toggleDoor(int pin, boolean open) {

    if (open) {
        if (pin >= 0 && pin < NR_OF_DOORS) {
            Serial.print(F("Opening door "));
            Serial.print(pin);
            Serial.print(F(" on pin "));
            Serial.println(doorPins[pin]);

            digitalWrite(doorPins[pin], LOW);
            return true;
        } else {
            Serial.print(F("No door"));
            Serial.println(pin);
            return false;
        }
    } else {
        digitalWrite(doorPins[pin], HIGH);
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

    // listen for incoming clients
    word pos = ether.packetLoop(ether.packetReceive());
    // check if valid tcp data is received
    if (pos) {

        Serial.println("new client");

        //Serial.print(F("Available memory: "));
        //Serial.println(availableMemory());

        // an http request ends with a blank line
        char* packetData = (char *) Ethernet::buffer + pos;
        unsigned int packetLength = strlen(packetData);
        char * method = strtok(packetData, " ");
        char * url = strtok(NULL, " ");

        ether.httpServerReplyAck(); // send ack to the request

        boolean isPost = strcmp(method, "POST") == 0,
                isOptions = strcmp(method, "OPTIONS") == 0;

        // Ignore queries while opening
        if (asyncOpenDoor >= 0) {
            sendCode(HTTP_420, sizeof HTTP_420, true);
        } else if (packetLength >= 600) {
            // flush buffer if goes above 600
            //I'm a teapot
            sendCode(HTTP_418, sizeof HTTP_418, true);

        } else if (isOptions) {
            //Inform about CORS
            sendCode(HTTP_200, sizeof HTTP_200, true);

        } else if (isPost) {

            //Grab request data
            struct clientInput parsedInput = parseClientInput(url);

            if (parsedInput.time < currentNonce) {

                const int n = snprintf(NULL, 0, "%lu", currentNonce);
                int timeLength = snprintf(response, n+1, "%lu", currentNonce);

                sendCode(HTTP_400, sizeof HTTP_400, false);
                memcpy(ether.tcpOffset(), response, timeLength);
                ether.httpServerReply_with_flags(timeLength, TCP_FLAGS_ACK_V|TCP_FLAGS_FIN_V);

                //Serial.print(F("Available memory: "));
                //Serial.println(availableMemory());

            } else {

                //update time counter
                currentNonce = parsedInput.time + 1;

                // check hash
                uint8_t *hash;
                Sha256.initHmac((uint8_t *) password, passwordLen);
                Sha256.print(parsedInput.time_s);
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

                if (strcmp(parsedInput.hash, hash_c) == 0) {

                    if (parsedInput.door < 0 || parsedInput.door > NR_OF_DOORS) {
                        sendCode(HTTP_404, sizeof HTTP_404, true);
                    } else {
                        asyncOpenDoor = parsedInput.door;
                        sendCode(HTTP_200, sizeof HTTP_200, true);
                    }
                } else {
                    // don't open door
                    sendCode(HTTP_401, sizeof HTTP_401, true);
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

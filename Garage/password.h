//Defined password to use
#define PASSWORD "your_defined_password"

//Number of doors supported
#define NR_OF_DOORS 2

//How long will the toggle be triggered?
#define OPENING_DELAY 250 //ms

//Pins to toggle the doors
const int actionPins[NR_OF_DOORS] = { 2, 3 };
//Pins to monitor if the door is open
const int openPins[NR_OF_DOORS] = { 4, 5 };
//Pins to monitor if the door is closed
const int closedPins[NR_OF_DOORS] = { 6, 7 };

//Name of the doors (add to doorNames if any new one is created)
const char door0Name[] PROGMEM = "door1";
const char door1Name[] PROGMEM = "door2";

const char* const doorNames[NR_OF_DOORS] PROGMEM = { door0Name, door1Name };

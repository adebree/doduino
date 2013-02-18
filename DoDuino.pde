// Debugging flags
//
#define WEBDUINO_SERIAL_DEBUGGING    0
#define DIMMER_SERIAL_DEBUGGING      1
#define NETWORK_SERIAL_DEBUGGING     0

static byte mac[]     = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xDD };

// Use the following config when not DHCP
//
static byte ip[]      = { 192, 168, 0, 5 };
static byte netmask[] = { 255, 255, 255, 0 };
static byte gateway[] = { 192, 168, 0, 1 };

// Globally defined variable to store millis() in every loop
//
unsigned long now;

#include "WProgram.h"
#include "Utils.h"
#include "Ethernet.h"
#include "WebServer.h"
#include "Network.h"
#include "Dimmer.h"
#include "Web.h"

void setup()
{
  Serial.begin( 9600 );
  Serial.println( "Starting DoDuino" );
  
  setupNetwork();  

  setupWeb();
  
  setupDimmer();
}

void loop()
{
  now = millis();
      
  loopWeb();
  
  loopDimmer();
}



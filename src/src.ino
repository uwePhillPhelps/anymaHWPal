/*
 * v2b
 * for use with MEGA ADK
 * and MAX3421 USB HOST LIBRARY 2.0 LIBRARY v1.3.1
*/

#define  VERSION_MESSAGE "AnymaPAL v2b 2025apr01"

#include "Anyma.h"

// user interface frontpanel controls and indicators
void    uiSetup();
uint8_t uiToggle();
void    uiStatus();

void setup() 
{
  Serial.begin( 38400 );    // debuging messages

  uiSetup();                // frontpanel pins etc
  
  RxAnymaClear();           // setup rxSyx[]

  if (Usb.Init() == -1){  while (1); } //DO NOT PROCEED
  delay( 200 );

  Serial.println(VERSION_MESSAGE);
  Serial.println("init OK");

  TxAnymaEModeCounter    = 60000; // almost immediate tx
}

void loop() 
{
  Usb.Task();

  if( uiToggle() )
  {
    uiStatus(); // indicate status
    TxAnymaKeepAlive();        // tx at interval
    TxAnymaEMode();            // tx at interval infrequently
  }  

  RxAnyma();                   // modifies rxSyx[]
  TxAnymaDinSyx();             // routes syx from din to anyma
  TxDinPatchSyx();             // routes anyma patchdump to din
  
  TxDinTuning();               // routes syx from anyma to din
  TxDinMainMatrix();
  TxDinAltMatrix();
}

void uiSetup()
{
  pinMode(LED_BUILTIN, OUTPUT);         // status LED
  pinMode(22, INPUT);                   // toggle w/pullup
}

uint8_t uiToggle()
{
  static uint8_t prevToggle = 0;
  uint8_t        currToggle = (digitalRead(22) == false);
  
  if( currToggle && !prevToggle )       // rising edge
  {
    TxAnymaGetPatch();                  // immediate tx
    
    TxAnymaEModeCounter = 60000;        // short tx delay
    digitalWrite(LED_BUILTIN, HIGH);    // immediate led
  }
  return prevToggle = currToggle;       // cache for next
}

void uiStatus()
{
  static uint16_t counter = 0;
  if( counter == 0   )    digitalWrite(LED_BUILTIN, LOW);
  if( ++counter == 1024 ) digitalWrite(LED_BUILTIN, HIGH);
  if( counter   == 2048 ) counter = 0;
}

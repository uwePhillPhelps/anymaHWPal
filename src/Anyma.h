/*
 * v2b
 * use MAX3421 USB HOST LIBRARY v1.3.1
*/
#include <usbh_midi.h> /* use version 1.3.1 */

USB       Usb;
USBH_MIDI UsbMidi(&Usb);

// system exclusive messages to anyma hardware
const uint8_t keepAliveSyx[3] = { 0xF0, 0x71, 0xF7 }; // 'q'
const uint8_t getStatusSyx[5] = { 0xF0, 0x71, 0x62, 0x06, 0xF7 }; // 'qb' 6
const uint8_t eModeSyx[7] = { 0xF0, 0x00, 0x21, 0x33, 0x71, 0x00, 0xF7 }; // 0 '!3q' 0
const uint8_t patchSyx[7] = { 0xF0, 0x00, 0x21, 0x33, 0x71, 0x11, 0xF7 }; // 0 '!3q' 11

void     TxAnymaGetPatch()
{
  UsbMidi.SendData( patchSyx );              // cable 0
}

uint16_t TxAnymaEModeCounter = 0;
void     TxAnymaEMode()
{
  if( ++TxAnymaEModeCounter < 65000 ) return; // abort?
  UsbMidi.SendData( eModeSyx );               // cable 0
  TxAnymaEModeCounter = 0;                    // reset
}

uint16_t TxAnymaKeepAliveCounter = 0;
void     TxAnymaKeepAlive()
{ 
  if( ++TxAnymaKeepAliveCounter < 1000 ) return; // abort?
  UsbMidi.SendData( keepAliveSyx );              // cable 0
  TxAnymaKeepAliveCounter = 0;                   // reset
}

uint16_t TxAnymaGetStatusCounter = 0;
void     TxAnymaGetStatus()
{
  if( ++TxAnymaGetStatusCounter < 1000 ) return; // abort?
  UsbMidi.SendData( getStatusSyx );              // cable 0
  TxAnymaGetStatusCounter = 0;                   // reset
}

uint16_t usbLen;                          // usb transport num bytes received
uint8_t  usbBuf[MIDI_EVENT_PACKET_SIZE];  // usb transport raw bytes
char     dispText[1024];                  // display text

#define RX_SYX_WAIT  0
#define RX_SYX_BEGAN 1
#define RX_SYX_ENDED 2

uint8_t  rxState = RX_SYX_WAIT;                
uint16_t rxLen   = 0;
uint8_t  rxSyx[MIDI_MAX_SYSEX_SIZE];
uint8_t  rxSrc   = 255;                       // 0=anyma, 1=din

inline void RxAnymaClear()
{
  // empty buffer
  for(int i=0; i<MIDI_MAX_SYSEX_SIZE; i++) rxSyx[i] = 0;
}

/*
 * RxAnyma() modifies rxSyx[]
 */
void    RxAnyma()
{
  for(int i=0;i<MIDI_EVENT_PACKET_SIZE;i++)  // clear before next fetch
  { usbBuf[i] = 0; }

  UsbMidi.RecvData( &usbLen, usbBuf);        // fetch data

  uint8_t* dat = usbBuf;                     // usb transport bytes
  uint8_t  len = usbLen;                     // usb transport num bytes
  
  if( len ) 
  {  
    uint8_t    i = 0;
    while( i<MIDI_EVENT_PACKET_SIZE )
    {
      uint8_t cidx = *dat & 0x0f;            // code index
      
      switch( cidx )
      {
        case 4:                              // three byte msg
        case 7:
          rxSrc = *dat >> 4;                 // 0=anyma, 1=din
          
          if( *(dat+1) == 0xF0 ) rxState = RX_SYX_BEGAN;
          if( *(dat+2) == 0xF0 ) rxState = RX_SYX_BEGAN;
          if( *(dat+3) == 0xF0 ) rxState = RX_SYX_BEGAN;

          if( rxState == RX_SYX_BEGAN ) rxLen = 0;
          rxSyx[rxLen++] = *(dat+1); 
          rxSyx[rxLen++] = *(dat+2); 
          rxSyx[rxLen++] = *(dat+3);
          rxState = RX_SYX_WAIT;

          if( *(dat+1) == 0xF7 ) rxState = RX_SYX_ENDED;
          if( *(dat+2) == 0xF7 ) rxState = RX_SYX_ENDED;
          if( *(dat+3) == 0xF7 ) rxState = RX_SYX_ENDED;
          break;
        case 6:                              // two byte msg
          rxSrc = *dat >> 4;                 // 0=anyma, 1=din
          
          if( *(dat+1) == 0xF0 ) rxState = RX_SYX_BEGAN;
          if( *(dat+2) == 0xF0 ) rxState = RX_SYX_BEGAN;

          if( rxState == RX_SYX_BEGAN ) rxLen = 0;
          rxSyx[rxLen++] = *(dat+1);
          rxSyx[rxLen++] = *(dat+2);
          rxState = RX_SYX_WAIT;

          if( *(dat+1) == 0xF7 ) rxState = RX_SYX_ENDED;
          if( *(dat+2) == 0xF7 ) rxState = RX_SYX_ENDED;
          break;
        case 5:                              // one byte msg
          rxSrc = *dat >> 4;                 // 0=anyma, 1=din
          
          rxSyx[rxLen++] = *(dat+1);
          if( *(dat+1) == 0xF0 ) rxState = RX_SYX_BEGAN;

          if( rxState == RX_SYX_BEGAN ) rxLen = 0;
          rxSyx[rxLen++] = *(dat+1);
          rxState = RX_SYX_WAIT;

          if( *(dat+1) == 0xF7 ) rxState = RX_SYX_ENDED;
          break;
        default:              
          break;
      }

      dat += 4;                              // next stride
      i   += 4;

      if( rxLen > (MIDI_MAX_SYSEX_SIZE-4) ) rxLen = 0;
    }
  }
}

void TxAnymaDinSyx()
{
  /*
   * NB: anyma firmware wantonly junks some syx in edit mode
   */
  
  if( rxState != RX_SYX_ENDED ) return;  // abort incomplete
  if( rxSrc != 1 ) return;               // abort not din
  if( rxSyx[0]     != 0xF0 &&            // abort not syx
      rxSyx[rxLen] == 0xF7 ) return;     
   
  Serial.print("Retransmit din syx ");
  Serial.print(rxLen, DEC); Serial.println(" bytes");
  
  UsbMidi.SendData( rxSyx, 0 );         // cable 0=anyma

  RxAnymaClear();                       // clear rxSyx[]
  rxState = RX_SYX_WAIT;                // wait for next
}

void syxDisp()                          // debug and display
{
  if( rxState != RX_SYX_ENDED ) return; // abort?
  
  for(int i=0; i<rxLen; i++)
  {
    sprintf(dispText, " %02X", rxSyx[i] );
    Serial.print( dispText );
  }
  Serial.println();

  rxState = RX_SYX_WAIT;
}

void TxDinPatchSyx()
{
  uint8_t* rx = rxSyx;
  if( rxState != RX_SYX_ENDED ) return; // abort?
  
  if( rx[0] == 0xF0 && // examine rx data
      rx[1] == 0x00 && // and re-transmit via DIN
      rx[2] == 0x21 && 
      rx[3] == 0x33 &&
      rx[4] == 0x71 &&
      rx[5] == 0x10 )  
  {
      Serial.println("Retransmit patch dump to DIN");
      Serial.print(rxLen, DEC); Serial.println(" bytes");
      
      UsbMidi.SendData( rx, 1 );        // AnymaDIN out

      RxAnymaClear();                   // clear rxSyx[]
      rxState = RX_SYX_WAIT;            // wait for next
  }
}

void MidiSend3( uint8_t st, uint8_t d1, uint8_t d2 )
{
  uint8_t txBuf[3] = { st, d1, d2 }; // three-byte message
  UsbMidi.SendData( txBuf, 1 );      // AnymaDIN out
  
  RxAnymaClear();        
}

void TxDinTuning()
{
  uint8_t* rx = rxSyx;
  
  // examine rx data and tx cc 23 main tuning
  if( rx[0] == 0xF0 &&
      rx[1] == 0x71 &&
      rx[2] == 0x00 && // 0x00 = system param
      rx[3] == 0x02 )  
  {
      MidiSend3( 0xB0, 23, rx[4] ); 
  }// is tuning?
}

void TxDinMainMatrix()
{
  uint8_t* rx = rxSyx;
  
  // examine rx data and tx cc16..31 (omit 23)
  if( rx[0] == 0xF0 &&
      rx[1] == 0x71 &&
      rx[2] == 0x06 )  // 0x06 = main matrix
  {
      // cc 16..22 from p 0..6
      if( rx[3] >= 0 && rx[3] <= 6 )
      {
        uint8_t ccNum = rx[3] + 16;
        uint8_t ccVal = rx[4]; 
        MidiSend3( 0xB0, ccNum, ccVal ); // ch1
      }

      // cc 23 'main tuning' handled elsewhere

      if( rx[3] >= 7 && rx[3] <= 14 )
      {
        uint8_t ccNum = rx[3] + 17;
        uint8_t ccVal = rx[4]; 
        MidiSend3( 0xB0, ccNum, ccVal ); // ch1
      }
  }// is main matrix?
}
void TxDinAltMatrix()
{
   uint8_t* rx = rxSyx;
  
  // examine rx data and tx midi cc 102..117 (omit 109, 114)
  if( 0xF0 == rx[0] &&
      0x71 == rx[1] &&
      0x07 == rx[2] ) // 0x07 = alt matrix
  {
      // cc 102..108 from p 0..6
      if( rx[3] >= 0 && rx[3] <= 6 )
      {
        uint8_t ccNum = rx[3] + 102;
        uint8_t ccVal = rx[4];
        MidiSend3( 0xB0, ccNum, ccVal ); // ch1
      }

      // cc 109 - 'alt tuning' handled elsewhere
      
      // cc 110..113 from p 7..10
      if( rx[3] >= 7 && rx[3] <= 10 )
      {
        uint8_t ccNum = rx[3] + 103;
        uint8_t ccVal = rx[4];
        MidiSend3( 0xB0, ccNum, ccVal ); // ch1
      }
      
      // cc 114 - 'alt morph' handled elsewhere
      
      // cc 115..117 from p 11..13
      if( rx[3] >= 11 && rx[3] <= 13 )
      {
        uint8_t ccNum = rx[3] + 104;
        uint8_t ccVal = rx[4];
        MidiSend3( 0xB0, ccNum, ccVal ); // ch1
      }
  }// is alt matrix?
}

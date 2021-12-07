#include <Arduino.h>
#include <TinyGPS++.h>
/*
   This sample code demonstrates how to use an array of TinyGPSCustom objects
   to monitor all the visible satellites.

   Satellite numbers, elevation, azimuth, and signal-to-noise ratio are not
   normally tracked by TinyGPS++, but by using TinyGPSCustom we get around this.

   The simple code also demonstrates how to use arrays of TinyGPSCustom objects,
   each monitoring a different field of the $GPGSV sentence.

   It requires the use of SoftwareSerial, and assumes that you have a
   4800-baud serial GPS device hooked up with RX and TX pins. See below how to set the pins.
*/

// The TinyGPS++ object
TinyGPSPlus gps;

// The serial connection to the GPS device
HardwareSerial ss(1);
/* 
  From http://aprs.gids.nl/nmea/:
   
  $GPGSV
  
  GPS Satellites in view
  
  eg. $GPGSV,3,1,11,03,03,111,00,04,15,270,00,06,01,010,00,13,06,292,00*74
      $GPGSV,3,2,11,14,25,170,00,16,57,208,39,18,67,296,40,19,40,246,00*74
      $GPGSV,3,3,11,22,42,067,42,24,14,311,43,27,05,244,00,,,,*4D

  1    = Total number of messages of this type in this cycle
  2    = Message number
  3    = Total number of SVs in view
  4    = SV PRN number
  5    = Elevation in degrees, 90 maximum
  6    = Azimuth, degrees from true north, 000 to 359
  7    = SNR, 00-99 dB (null when not tracking)
  8-11 = Information about second SV, same as field 4-7
  12-15= Information about third SV, same as field 4-7
  16-19= Information about fourth SV, same as field 4-7

----------------------------------------------------------------------

$GPGSA
GPS DOP and active satellites

eg1. $GPGSA,A,3,,,,,,16,18,,22,24,,,3.6,2.1,2.2*3C
eg2. $GPGSA,A,3,19,28,14,18,27,22,31,39,,,,,1.7,1.0,1.3*35


1    = Mode:
       M=Manual, forced to operate in 2D or 3D
       A=Automatic, 3D/2D
2    = Mode:
       1=Fix not available
       2=2D
       3=3D
3-14 = IDs of SVs used in position fix (null for unused fields)
15   = PDOP
16   = HDOP
17   = VDOP

*/

static const int MAX_SATELLITES = 40;

TinyGPSCustom totalGPGSVMessages(gps, "GPGSV", 1); // $GPGSV sentence, first element
TinyGPSCustom messageNumber(gps, "GPGSV", 2);      // $GPGSV sentence, second element
TinyGPSCustom satsInView(gps, "GPGSV", 3);         // $GPGSV sentence, third element
TinyGPSCustom satNumber[4]; // to be initialized later
TinyGPSCustom elevation[4];
TinyGPSCustom azimuth[4];
TinyGPSCustom snr[4];

TinyGPSCustom satsInUse[14]; // $GPGSA sentence, first element

struct
{
  bool active;
  int elevation;
  int azimuth;
  int snr;
} sats[MAX_SATELLITES];

void setup()
{
  Serial.begin(115200);
  
  static const int RXPin = 34, TXPin = 12;

  ss.begin(9600, SERIAL_8N1, RXPin, TXPin);
  Serial.println(F("SatelliteTracker.ino"));
  Serial.println(F("Monitoring satellite location and signal strength using TinyGPSCustom"));
  Serial.print(F("Testing TinyGPS++ library v. ")); Serial.println(TinyGPSPlus::libraryVersion());
  Serial.println(F("by Mikal Hart"));
  Serial.println();
  
  // Initialize all the uninitialized TinyGPSCustom objects
  for (int i=0; i<4; ++i)
  {
    satNumber[i].begin(gps, "GPGSV", 4 + 4 * i); // offsets 4, 8, 12, 16
    elevation[i].begin(gps, "GPGSV", 5 + 4 * i); // offsets 5, 9, 13, 17
    azimuth[i].begin(  gps, "GPGSV", 6 + 4 * i); // offsets 6, 10, 14, 18
    snr[i].begin(      gps, "GPGSV", 7 + 4 * i); // offsets 7, 11, 15, 19
  }

  for (int i=0; i<12; i++)
  {
    satsInUse[i].begin(gps, "GPGSA", 3 + i); // offsets 3 ... 14
  }


}

bool SatInUse(int satnum){
  for(int i=0;i<14;i++){
    if(satnum == atoi(satsInUse[i].value()) )
      return true;
  }
  return false;
}


void loop()
{
  // Dispatch incoming characters
  if (ss.available() > 0)
  {
    char rr = (char) ss.read();
    //Serial.printf("%c",rr);
    gps.encode(rr);
    if (totalGPGSVMessages.isUpdated())
    {
      for (int i=0; i<4; ++i)
      {
        int no = atoi(satNumber[i].value());
        // Serial.print(F("SatNumber is ")); Serial.println(no);
        if (no >= 1 && no <= MAX_SATELLITES)
        {
          sats[no-1].elevation = atoi(elevation[i].value());
          sats[no-1].azimuth = atoi(azimuth[i].value());
          sats[no-1].snr = atoi(snr[i].value());
          sats[no-1].active = true;
        }
      }
      
      int totalMessages = atoi(totalGPGSVMessages.value());
      int currentMessage = atoi(messageNumber.value());
      if (totalMessages == currentMessage)
      {
        int sats_in_view = 0;
        for (int i=0; i<MAX_SATELLITES; ++i)
          if (sats[i].active)
          {
            sats_in_view ++;
          }
        
        Serial.println("---------------------------------");
        Serial.printf("Sats in view=%d  Sats used=%d    The sats used have a *\r\n", sats_in_view, gps.satellites.value() );
        Serial.print(F("Sat Nums= "));
        for (int i=0; i<MAX_SATELLITES; ++i)
          if (sats[i].active)
          {
            Serial.printf("%3d%c ",i+1, SatInUse(i+1) ? '*' : ' ');
          }

        Serial.printf("\r\nElevation=");
        for (int i=0; i<MAX_SATELLITES; ++i)
          if (sats[i].active)
          {
            Serial.printf("%3d  ", sats[i].elevation);
          }
        Serial.printf("\r\nAzimuth=  ");
        for (int i=0; i<MAX_SATELLITES; ++i)
          if (sats[i].active)
          {
            Serial.printf("%3d  ",sats[i].azimuth);
          }
        
        Serial.printf("\r\nSNR=      ");
        for (int i=0; i<MAX_SATELLITES; ++i)
          if (sats[i].active)
          {
            Serial.printf("%3d  ",sats[i].snr);
          }

        Serial.println();


        for (int i=0; i<MAX_SATELLITES; ++i)
          sats[i].active = false;
      }
    }
  }


}

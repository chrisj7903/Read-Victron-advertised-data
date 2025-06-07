/* ===== Battery Monitor ===== 

Retrieves, decrypts, disassembles and reports the Bluetooth 'advertised data' from 
a Victron Battery Monitor e.g: SmartShunt, BMV-712

The 'advertised data' is encrypted and embedded as the "Extra Manufacturer Data" that 
is repeatedly transmitted over Bluetooth Low Energy (BLE) and can be seen/read without 
the need to establish a BLE connection (= even lower energy consumption).
-------------------------------------------------------------------------------------
Before running this program you must initialise it with information specific to the 
Victron device of interest:
1. <device_name> 
2. <device_address>
3. <encryption_key)  

The VictronConnect (VC) mobile App is used to interrogate the device for this information 
 
Provided the device has been paired to VC, the <device_name> will be shown on 
the VC home page. It can be changed in the Settings at any time.
The <device_address> and <encryption_key> can found in the device Settings via:

Settings (cog) >  3 dots (top right) > Product-Info > scroll down > encryption key  

The target device must be nominated in VBM.h as follows: 
#define VICTRON_ADDRESS <device_address>
#define VICTRON_NAME    <device_name>
If you are using this code for several monitors, you can enable the loadKey() function to 
load the required encryption key by adding the <device_name> for each monitor into loadKey().

The <encryption key> unique to the device must be declared as the 16 byte array key_BM[] in VBM.cpp

NB: <device_address> and <encryption_key> must be lower case. 

If device is reporting some dud readings, where the values reported are clearly wrong or corrupted, you can 
turn FILTERING on to suppress dud readings. To temporarily enable filtering, enter F at the Serial Monitor 
while the sketch is running. For permanent filtering set bool FILTERING = false in ZZ.cpp before compiling.
The filtering relies on the user nominating appropriate preferences/threshholds in VBM.h for:
  EXPECTED_AUX_MODE (auxilliary input selection)
  BATTV_MAX / MIN   (battery Volts)
  BATTA_MAX / MIN   (battery Amps)
  AVAL_ MAX / MIN   (Volts or Kelvin, depending on aux setting)
  AH_MAX            (Amp-Hours of yield)
  PVW_MAX           (Photo-Voltaic Watts - panel power )
  SOC_MAX / MIN     (State of Charge)
The current preferences/threshholds are setup for a 24V system.

---------------------------------------------------------------------------------------------------
Once the progam is running entering "V" at the Serial Monitor toggles VERBOSE mode between more/less detailed status/info
*/

#include "ZZ.h"
#include "VBM.h" // Victron Battery Monitor

int scan_gap_ms   = 500;    // delay between scans 
int scan_max_secs = 2;      // maximum scan timeout

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) ;                                          // wait for serial, up to 2 sec
  Serial << F("\n\n======== Battery Monitor ========\n");
  Serial << F("* wolfssl  : V") << LIBWOLFSSL_VERSION_STRING << '\n';  
  Serial << F("* Target   : ")  << VICTRON_NAME << '\n';
  Serial << F("* VERBOSE  : "); if (VERBOSE)   Serial << F("ON\n"); else Serial << F("OFF\n");
  Serial << F("* FILTERING: "); if (FILTERING) Serial << F("ON\n"); else Serial << F("OFF\n");
  Serial << F("\tEnter V to toggle VERBOSE mode ON/OFF\n");
  Serial << F("\tEnter F to toggle FILTERING of dud readings ON/OFF\n");
  Serial << F("* init BLE ...\n");
  BLEDevice::init("");
  Serial << F("* setup scan ...\n");
  pBLEScan->setAdvertisedDeviceCallbacks(new AdDataCallback());
  pBLEScan->setActiveScan(true);            // uses more power, but get results faster
  Serial << F("* scan for devices every ") << scan_gap_ms << F(" ms (and up to ") << scan_max_secs << F(" secs/scan)\n");
  Serial << CF(dashes) << F("setup done") << CF(dashes) << '\n';
  displayHeadings();
} 

uint32_t loopCount = 0;

void loop(){
  loopCount++; 
  processSerialCommands();
  mfrDataReceived = false; 
  pBLEScan->start(scan_max_secs, false);
  delay(scan_gap_ms);
  if (VERBOSE) Serial << '\n';
  printLoopCount();
  if(mfrDataReceived) {
    if (VERBOSE) {
      Serial << CF(line);
      Serial <<  F("data  : "); printBIGarray(); Serial << '\n';
    }
    decryptAesCtr(VERBOSE);
    if (VERBOSE) {
      Serial << F("output: "); printByteArray(output); Serial << '\n';
    //Serial << F("binary:\n");     printBins(output); Serial << '\n';
      Serial << F("values: "); 
    }
  reportBMvalues();
  }
  else {
    Serial << '\t';
    if (VERBOSE) Serial << F("** No device matching ") << VICTRON_ADDRESS << F(" found during last scan ** "); //(" << t2-t1 << ")";
    else         Serial << F("** Target device not found **");
  }
  Serial << '\n';   
}

void displayHeadings(){
  Serial << F("\t ttg  batt V alms  mid V    aux  amps      Ah     soc\n");  
  Serial << F("\t----- ------ ---- ------- | --- ------ --------- ------\n");
}

void printLoopCount(){
  if (loopCount < 100) Serial << F("0");
  if (loopCount <  10) Serial << F("0");
  Serial << loopCount; 
  if (VERBOSE) Serial << F(" ");  
}

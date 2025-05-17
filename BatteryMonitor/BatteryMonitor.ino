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

The <encryption key> unique to the device must be declared as the 16 byte array key_BM[] in VBM.cpp
---------------------------------------------------------------------------------------------------
Once the progam is running entering "V" at the Serial Monitor toggles VERBOSE mode between more/less detailed status/info
*/

#include "ZZ.h"
#include "VBM.h" // Victron Battery Monitor

int delay_ms  = 1000;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000) ;                                          // wait for serial, up to 2 sec
  Serial << "\n\n======== Battery Monitor ========\n";
  Serial << "* wolfssl : V" << LIBWOLFSSL_VERSION_STRING << '\n';  
  Serial << "* Target  : "  << VICTRON_NAME << '\n';
  Serial << "* Enter V to toggle VERBOSE mode ON/OFF\n";
  Serial << "* init BLE ...\n";
  BLEDevice::init("");
  Serial << "* setup scan ...\n";
  pBLEScan->setAdvertisedDeviceCallbacks(new AdDataCallback());
  pBLEScan->setActiveScan(true);            // uses more power, but get results faster
//pBLEScan->setInterval(100);               // time between consecutive scan starts
//pBLEScan->setWindow(99);                  // duration of each scan, less or equal setInterval value
  Serial << "* scan for devices every " << delay_ms << " ms (and up to " << scan_secs << " secs/scan)\n";
  Serial << CF(dashes) << "setup done" << CF(dashes) << '\n';
  displayHeadings();
} 

uint32_t loopCount = 0;

void loop(){
  loopCount++; 
  processSerialCommands();
  pBLEScan->start(scan_secs, false);
  delay(delay_ms);
  if (VERBOSE) Serial << '\n';
  printLoopCount();
  if (VERBOSE) {
    Serial << CF(line);
    Serial <<  "data  : "; printBIGarray(); Serial << '\n';
  }
  decryptAesCtr(VERBOSE);
  if (VERBOSE) {
    Serial <<   "output: "; printByteArray(output); Serial << '\n';
  //Serial <<   "binary:\n";     printBins(output); Serial << '\n';
    Serial <<   "values: "; 
  }
  reportBMvalues();
} // loop

void displayHeadings(){
  Serial << '\n';
  Serial << "\tttg " << "batt V " << "alms " << " mid V  " << "  aux " << " amps  " << " Ah    " << "soc\n";  
  Serial << "\t--- " << "------ " << "---- " << " ------ " << "| --- " << "------ " << "------ " << "----\n";  
}

void printLoopCount(){
  if (loopCount < 100) Serial << "0";
  if (loopCount <  10) Serial << "0";
  Serial << loopCount << ":\t";  
}

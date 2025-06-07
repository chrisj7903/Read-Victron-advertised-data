#pragma once

// -----------------------------------------------------------------
#include "BLEDevice.h"

// replace with actual Name and Address
#define VICTRON_ADDRESS "ff:ff:ff:ff:ff:ff"     // Smartshunt address (lower case)
#define VICTRON_NAME "My_SmartShunt_1"

// Nominate Aucilliary mode 
//efine EXPECTED_AUX_MODE 0     // on Auxilliary input, monitor aux voltage 
#define EXPECTED_AUX_MODE 1     // on Auxilliary input, monitor mid voltage 
//efine EXPECTED_AUX_MODE 2     // on Auxilliary input, monitor Kelvin temperature
//efine EXPECTED_AUX_MODE 3     // do not monitor Auxilliary input

// Set upper & lower threshholds for detecting dud readings 
#define BATTV_MIN   20    // volts
#define BATTV_MAX   34    // volts
#define  AVAL_MIN   10    // volts
#define  AVAL_MAX   17    // volts
#define BATTA_MIN -200    // amps
#define BATTA_MAX  200    // amps
#define    AH_MAX 1000    // amp-hours
#define   SOC_MIN   10    // %
#define   SOC_MAX  100    // %
extern BLEScan *pBLEScan; // = BLEDevice::getScan();

// Scan for BLE servers for the advertising service we seek. Called for each advertising server
class AdDataCallback : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice);
};

// -----------------------------------------------------------------

#include "wolfssl.h"
#include "wolfssl/wolfcrypt/aes.h" // was #include <wolfssl/wolfcrypt/aes.h>

extern void encryptAesCtr();
extern void decryptAesCtr(bool quiet);

const word32 blkSize = AES_BLOCK_SIZE * 1; 

extern byte inputs[blkSize]; 
extern byte cipher[blkSize]; 
extern byte output[blkSize]; 

extern bool mfrDataReceived;

extern int aux;
extern void  reportBMvalues();
extern float parseBattVolts();
extern float parseMidVolts();

extern void printBIGarray();
extern void printByteArray(byte byteArray[16]);
extern void printBins(byte byteArray[16]);

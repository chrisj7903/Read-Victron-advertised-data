#pragma once

// -----------------------------------------------------------------
#include "BLEDevice.h"

// replace with actual Name and Address
#define VICTRON_ADDRESS "ff:ff:ff:ff:ff:ff"     // Solar Controller address (lower case)
#define VICTRON_NAME    "My_Solar_Controller"

// Set upper & lower threshholds for detecting dud readings 
#define BATTV_MIN   20    // volts
#define BATTV_MAX   34    // volts
#define BATTA_MIN    0    // amps
#define BATTA_MAX  200    // amps
#define   KWH_MAX  500    // kilo watt hours
#define   PVW_MAX 1000    // watts
#define LOADA_MIN    0    // amps
#define LOADA_MAX  200    // amps

extern BLEScan *pBLEScan; // = BLEDevice::getScan();

// Scan for BLE servers for the advertising service we seek. Called for each advertising server
class AdDataCallback : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertiser);  
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

extern void reportSCvalues();
extern float parseBattVolts();

extern void printBIGarray();
extern void printByteArray(byte byteArray[16]);
extern void printBins(byte byteArray[16]);

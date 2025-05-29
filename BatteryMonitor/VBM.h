#pragma once

// -----------------------------------------------------------------
#include "BLEDevice.h"


// replace with actual Name and Address
#define VICTRON_ADDRESS "ff:ff:ff:ff:ff:ff"     // Smartshunt address (lower case)
#define VICTRON_NAME "My_SmartShunt_1"

extern BLEScan *pBLEScan; // = BLEDevice::getScan();
extern int scan_secs;

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

extern int aux;
extern void  reportBMvalues();
extern float parseBattVolts();
extern float parseMidVolts();

extern void printBIGarray();
extern void printByteArray(byte byteArray[16]);
extern void printBins(byte byteArray[16]);

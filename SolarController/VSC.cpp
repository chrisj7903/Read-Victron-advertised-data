/* Routines to 
- access Victron Device using Bluetooth Low Energy (BLE) to retrieve the advertised data, 
- decrypt & disassemble the values in the bit fields of the advertised data */ 

#include "ZZ.h"
#include "VSC.h"

#if defined(NO_AES) or !defined(WOLFSSL_AES_COUNTER) or !defined(WOLFSSL_AES_128)
#error "Missing AES, WOLFSSL_AES_COUNTER or WOLFSSL_AES_128"
#endif

Aes aesEnc;
Aes aesDec;

byte BIGarray[26]    = {0};   // for all manufacturer data including encypted data
byte   encKey[16];      // for nominated encryption key
byte     iv[blkSize];   // initialisation vector 
byte cipher[blkSize];   // encrypted data
byte output[blkSize];   // decrypted result

bool mfrDataReceived = false;

// replace with actual key values (NB: use lower case)
byte key_SC[] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}; // My_Solar_Controller

// fwd decs
//id decryptAesCtr(bool VERBOSE);
void loadKey();
//id reportSCvalues();
void reportDeviceState();
void reportChargerError();
float parseBattVolts();
float parseBattAmps();
float parseKWHtoday();
float parsePVpower();
float parseLoadAmps();
bool checkForbadArgs();
void printBIGarray();
void printByteArray(byte byteArray[16]);
void printBins();

// ----------------------------------------------------------------------
BLEScan *pBLEScan = BLEDevice::getScan();

// --------------------------------------------------------------------------------
// Scan for BLE servers for the advertising service we seek. Called for each advertising server
void AdDataCallback::onResult(BLEAdvertisedDevice advertiser) {
  if (!mfrDataReceived){
    if (advertiser.getAddress().toString() == VICTRON_ADDRESS){         // select a specific advertising device
      unsigned int len = advertiser.getManufacturerData().length();
      if (len >= 11 && advertiser.getManufacturerData()[0] == 0xE1      // second byte of Victron company identifier 0x02E1 (little endian)
                    && advertiser.getManufacturerData()[1] == 0x02      // first byte
                    && advertiser.getManufacturerData()[2] == 0x10) {   // indicates manufacturer data follows next
        BLEDevice::getScan()->stop();                                         // stop this scan
        for (int i = 0; i < min(len,sizeof(BIGarray)); i++) BIGarray[i] = advertiser.getManufacturerData()[i];
        mfrDataReceived = true;
      }
    }
  }
}

// --------------------------------------------------------------------------------
// encryption routine not required here (covered in AES_CTR_enc_dec.ino)
// decrypt cipher -> outputs  
void decryptAesCtr(bool VERBOSE){
  memcpy(encKey,key_SC,sizeof(key_SC));       // key_SC -> encKey[]  
  //loadKey();                                // replaced by line above. enable loadKey() for multiple SC                        
  iv[0] = BIGarray[7];                        // copy LSB into iv
  iv[1] = BIGarray[8];                        // copy MSB into iv
  memcpy(cipher, BIGarray + 10, 16);          // BIGarray[11:26] -> cipher[1:16]
  memset(&aesDec,0,sizeof(Aes));              // Init stack variables
  if (VERBOSE) {
    Serial << "key   : "; printByteArray(encKey); Serial << '\n';
    Serial << "salt  : "; printByteArray(iv);     Serial << '\n';
    Serial << "cipher: "; printByteArray(cipher); Serial << '\n';
  } 
  wc_AesInit      (&aesDec, NULL, INVALID_DEVID);                         // init aesDec 
  wc_AesSetKey    (&aesDec, encKey, blkSize, iv, AES_ENCRYPTION);         // load dec key 
  wc_AesCtrEncrypt(&aesDec, output, cipher, sizeof(cipher)/sizeof(byte)); // do decryption
  if (checkForbadArgs()) Serial << F("**FAIL** bad args detected!");      //
  wc_AesFree(&aesDec);    // free up resources
}

/* Activate for use on multiple controllers
void loadKey(){
  if      (VICTRON_NAME == "My_Solar_Controller") memcpy(encKey,key_SC,sizeof(key_SC));   // key_SC -> encKey[]
  else if (VICTRON_NAME == "My_SmartShunt_1")     memcpy(encKey,key_SS,sizeof(key_SS));   // key_SS -> encKey[]
  else if (VICTRON_NAME == "My_BMV712_P1")        memcpy(encKey,key_P1,sizeof(key_P1));   // key_P1 -> encKey[]
  else if (VICTRON_NAME == "My_BMV712_P2")        memcpy(encKey,key_P2,sizeof(key_P2));   // key_P2 -> encKey[]
  else if (VICTRON_NAME == "My_BMV712_P3")        memcpy(encKey,key_P3,sizeof(key_P3));   // key_P3 -> encKey[]
  else {
    Serial << "\n\n *** Program HALTED: encryption key not set!\n";
    while(1);
  }
}
*/

bool na_batV = false;
bool na_batA = false;
bool na_kWh  = false;
bool na_pvW  = false;
bool na_lodA = false;

int dudvals = 0, maxduds = 0;           // count of dud values in one set of readings

/* ------------------------------------------------------------------------
Extract & decode bytes received and report current values. 
NB: multiple bytes are little-endian (i.e order of double/triple bytes reversed)
signed ints use 2's complement, so the first mask excises the sign bit */
void reportSCvalues(){
  if (FILTERING) maxduds = 0; else maxduds = 10;              // if FILTERING, silences dud reporting  
  //if (TESTMODE) memcpy(output,testArray,16);                  // override output array with test values
  float battV = parseBattVolts();       // -327.68 -> 327.66 V
  float battA = parseBattAmps();        // -3276.8 -> 3276.6 A
  float kWh   = parseKWHtoday();        //       0 -> 655.34 kWh
  float PV_W  = parsePVpower();         //       0 -> 65534  W
  float loadA = parseLoadAmps();        //       0 -> 51.0   A
  //Serial << '\t'; 
  // -- flag (& optionally filter) any dud readings ---------------------------------------
  if (!na_batV && (battV < BATTV_MIN || battV > BATTV_MAX)) dudvals++;
  if (!na_batA && (battA < BATTA_MIN || battA > BATTA_MAX)) dudvals++;
  if (!na_pvW  && (PV_W  >   PVW_MAX ))                     dudvals++;
  if (!na_kWh  && (kWh   >   KWH_MAX ))                     dudvals++;
  if (LOAD_AMPS){ if (!na_lodA  && (loadA< LOADA_MIN || loadA > LOADA_MAX)) dudvals++; }
  // -- in-line reporting -----------------------------------------------------------------
  if (!VERBOSE && dudvals) Serial << " *"; // flag if this reading contains one or more dud values
  if (dudvals <= maxduds){
    if (!VERBOSE) Serial << '\t';  else Serial << " "; 
    reportDeviceState();  Serial << " ";
    reportChargerError(); Serial << " ";
    if (na_batV)  Serial << "n/a-"; else Serial << _WIDTH(_FLOAT(battV,2),5);  Serial << "V ";
    if (na_batA)  Serial << "n/a-"; else Serial << _WIDTH(_FLOAT(battA,1),5);  Serial << "A | ";
    if (na_kWh)   Serial << "n/a-"; else Serial << _WIDTH(_FLOAT(kWh  ,2),6);  Serial << "kWh ";
    if (na_pvW)   Serial << "n/a-"; else Serial << _WIDTH(_FLOAT(PV_W ,0),3);  Serial << "W  ";
    if (LOAD_AMPS) {if (na_lodA) 
                  Serial << " n/a-"; else Serial << _WIDTH(_FLOAT(loadA,1),4); Serial << "A";
    }
  }
  if (dudvals) Serial << "\t[duds: " << dudvals << "]";
  // --------------------------------------------------------------------------------------
  dudvals = 0;  
  na_batV = false;
  na_batA = false;
  na_kWh  = false;
  na_pvW  = false;
  na_lodA = false;
}

void reportDeviceState(){
  if      (output[0] == 0) Serial << "_OFF__";
  else if (output[0] == 1) Serial << "Lo_PWR";
  else if (output[0] == 2) Serial << "FAULT ";
  else if (output[0] == 3) Serial << "_BULK_";
  else if (output[0] == 4) Serial << "ABSORB";
  else if (output[0] == 5) Serial << "FLOAT_";
  else if (output[0] == 6) Serial << "Store ";
  else if (output[0] == 7) Serial << "Eq_Man";
  else  Serial << "*" << _WIDTHZ(_HEX(output[0]),2) << "*";
}

void reportChargerError(){
  if      (output[1] == 0) Serial << "no_err";
  else if (output[1] == 1) Serial << "BATHOT";
  else if (output[1] == 2) Serial << "VOLTHI";
  else if (output[1] == 3) Serial << "REMC_A";
  else if (output[1] == 4) Serial << "REMC_B";
  else if (output[1] == 5) Serial << "REMC_C";
  else if (output[1] == 6) Serial << "REMB_A";
  else if (output[1] == 7) Serial << "REMB_B";
  else if (output[1] == 8) Serial << "REMB_C";
  else  Serial << "*" << _WIDTHZ(_HEX(output[1]),2) << "*";
}

// Some of the routines below use static_cast to convert integers  
// to floats while avoiding dud fractions from integer division.

// Note: SC & BM use same bytes (2,3) for battery volts
float parseBattVolts(){
  bool    neg       =  (output[3] & 0x80) >> 7;                   // extract sign bit
  int16_t batt_mV10 = ((output[3] & 0x7F) << 8) | output[2];      // exclude sign bit from byte 3
  if (batt_mV10 == 0x7FFF) na_batV = true;                       
  if (neg) batt_mV10 = batt_mV10 - 32768;                         // 2's complement = val - 2^(b-1) b = bit# = 16
  return (static_cast<float>(batt_mV10)/100);                     // integer units 10mV converted to V as float
}

// Battery Current (signed) 16 bits = sign bit + 15 bits
float parseBattAmps(){
  bool neg = ((output[5]  & 0x80) >> 7);                          // extract sign bit
  int16_t ma100 = ((output[5] & 0x7F) << 8) | output[4];          // exclude sign bit from byte 5
  if (ma100 == 0x7FFF) na_batA = true;
  if (neg) ma100 = ma100 - 32768;                                 // 2's complement = val - 2^(b-1) b = bit# = 16
  return (static_cast<float>(ma100)/10);                          // convert mA100 to float A
}

// Today's Yield 16bits (unsigned int) units 0.01kWh (10Wh). 
float parseKWHtoday(){
  uint16_t Wh10     = (output[7] << 8) | output[6];               // NB little endian: byte[7] <-> byte[6]  
  if (Wh10 == 0xFFFF) na_kWh = true;
  return (static_cast<float>(Wh10)/100);                          // convert integer in 10Wh units to kWh as float 
}

// PV panel power in Watts
float parsePVpower(){
  uint16_t pvW = (output[9] << 8) | output[8];                    // NB little endian: byte[9] <-> byte[8]
  if (pvW == 0xFFFF) na_pvW = true;
  return (static_cast<float>(pvW));                               // convert integer Watts to float 
}

// Load current? (Possibly irrelevant as VictronConnect doesn't even display this)
float parseLoadAmps(){
  uint16_t PVma100 = ((output[11] & 0x01) << 8) | output[10];     // NB little endian: byte[11] <-> byte[10] 
  if (PVma100 == 0x1FF) na_lodA = true;
  return (static_cast<float>(PVma100)/10);                        //  convert integer in 100mA units to Amps as float 
} 

// --------------------------------- Shared routines ---------------------------------------------
bool checkForbadArgs(){
  int x = wc_AesCtrEncrypt(   NULL, output, cipher, sizeof(cipher)/sizeof(byte)); 
  int y = wc_AesCtrEncrypt(&aesDec, NULL,   cipher, sizeof(cipher)/sizeof(byte)); 
  int z = wc_AesCtrEncrypt(&aesDec, output, NULL,   sizeof(cipher)/sizeof(byte)); 
  if (x == WC_NO_ERR_TRACE(BAD_FUNC_ARG) && 
      y == WC_NO_ERR_TRACE(BAD_FUNC_ARG) && 
      z == WC_NO_ERR_TRACE(BAD_FUNC_ARG)) 
       return false;
  else return true;
}

// print all bytes, before decryption
void printBIGarray(){
  Serial << "[";   
  int sz = sizeof(BIGarray);
  for (int i=0; i<sz; i++) {
    if (BIGarray[i] < 0x10) Serial << "0"; 
    Serial << _HEX(BIGarray[i]);
    if      (i ==  6) Serial << " (";
    else if (i ==  8) Serial << ") ";
    else if (i ==  9) Serial <<  "] ";
    else if (i == 17) Serial << " | ";
    else if (i<sz-1)  Serial << " ";
  } 
//Serial << '\n';
}

// print out HEX bytes for the nominated 16 byte array
void printByteArray(byte byteArray[16]) {
  for (int i = 0; i < 16; i++) { 
    if (byteArray[i] < 0x10) Serial << "0";
    Serial << _HEX(byteArray[i]);
    if      (i == 7) Serial << " | "; // half way marker
    else if (i < 15) Serial << " "; 
  }
}

void printBins(byte byteArray[16]){
  for (int i=0; i<16; i++) {
    Serial << '\t';
    if (i < 10) Serial << " ";
    Serial << "[" << i << "] " << _WIDTHZ(_BIN(byteArray[i]),8) << " | " << _WIDTHZ(_HEX(byteArray[i]),2) << '\n'; 
  }
}

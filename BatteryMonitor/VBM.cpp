/* Routines for a Victron Battery Monitor (e.g Smartshunt or BMV-712) to
- retrieve the data advertised over Bluetooth Low Energy (BLE)   
- decrypt, disassemble, decode & report values from the advertised data */ 

#include "ZZ.h"
#include "VBM.h"
#include <bitset>

#if defined(NO_AES) or !defined(WOLFSSL_AES_COUNTER) or !defined(WOLFSSL_AES_128)
#error "Missing AES, WOLFSSL_AES_COUNTER or WOLFSSL_AES_128"
#endif

Aes aesEnc;
Aes aesDec;

byte BIGarray[26]    = {0};   // for all manufacturer data including encypted data
byte   encKey[16]    = {0};   // for nominated encryption key
byte     iv[blkSize] = {0};   // initialisation vector 
byte cipher[blkSize] = {0};   // encrypted data
byte output[blkSize] = {0};   // decrypted result

// replace with actual key values (in lower case)
byte key_SS[] = {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}; // My_Smartshunt_1
//te key_S2[] = {0x96,0x52,0x4c,0xc1,0x1d,0x95,0x1b,0x63,0x79,0x6d,0x05,0xa9,0xac,0xce,0x73,0x18}; // My_Smartshunt_2
//te key_P1[] = {0x21,0x4e,0x63,0x51,0x1c,0xa9,0xff,0x90,0xdb,0xf9,0xce,0x3d,0xf0,0x53,0x15,0x28}; // My_BMV712_P1

// --- forward declarations ---
void loadKey();
float parseTimeToGo();
float parseBattVolts();
float parseAuxVolts();
float parseMidVolts();
float parseAuxKelvin();
//oat parseAuxCelsius();
float parseBattAmps();
float parseAmpHours();
float parseStateOfCharge();
char * reportAlarms(uint32_t alarmBits);
uint32_t countBitsSet(uint32_t val);
bool checkForbadArgs();
void printBIGarray();
void printByteArray(byte byteArray[16]);
void printBins();

BLEScan *pBLEScan = BLEDevice::getScan();

int scan_secs = 2;

// --------------------------------------------------------------------------------
// Scan for BLE servers for the advertising service we seek. Called for each advertising server
void AdDataCallback::onResult(BLEAdvertisedDevice advertisedDevice) {
  if (advertisedDevice.getAddress().toString() == VICTRON_ADDRESS){ // select target device
    String manufData = advertisedDevice.getManufacturerData();
    unsigned int len = manufData.length();
    if (len >= 11 && manufData[0] == 0xE1 && manufData[1] == 0x02 && manufData[2] == 0x10) {
      manufData.getBytes(BIGarray, std::min(len, sizeof(BIGarray)));
      BLEDevice::getScan()->stop();
    }
  }
}

// --------------------------------------------------------------------------------
// decrypt cipher -> outputs  
void decryptAesCtr(bool VERBOSE){
  memcpy(encKey,key_SS,sizeof(key_SS));       // key_SS -> encKey[]
  //loadKey();                                // replaced by line above. enable loadKey() for multiple BM 
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

/*
void loadKey(){
  if      (VICTRON_NAME == "My_SmartShunt_1") memcpy(encKey,key_SS,sizeof(key_SS));   // key_SS -> encKey[]
  else if (VICTRON_NAME == "My_SmartShunt_2") memcpy(encKey,key_SS,sizeof(key_S2));   // key_S2 -> encKey[]
  else if (VICTRON_NAME == "My_BMV712_P1")    memcpy(encKey,key_P1,sizeof(key_P1));   // key_P1 -> encKey[]
  else {
    Serial << "\n\n *** Program HALTED: encryption key not set!\n";
    while(1);
  }
}
*/

// =====================================================================================

bool inf_TTG = false;
bool na_batV = false;
bool na_aux  = false;
bool na_batA = false;
bool na_Ah   = false;
bool na_soc  = false;

/* ------------------------------------------------------------------------
Report Battery Monitor values
-----------------------------
Extract & decode each raw values contained in the 16 decrypted bytes, calculate each final values and report. 

Notes: 
1) multiple bytes are little-endian (i.e ordering of double/triple bytes is reversed)
2) the signed values use 2's complement, so the sign bit must be handled

LSB: Least Significant bits/Byte
MIB: Middle            bits/byte (triples only)
MSB: Most significant  bits/byte

In order of bytes received:
---------------------------
// Time To Go in minutes (16 bits)
byte  0 Time To Go LSB bits  7-0 
byte  1 Time To Go MSB bits 15-8

// Battery Volts (16 bits, signed, units 10mV)
byte  2 Batt Volts LSB bits  7-0 
byte  3 Batt Volts MSB bits 14-8 
byte  3 Batt Volts sign bit 15    

// Alarms status (2 bytes, 8 bits each)
byte  4 Alarms status LSB bits  7-0  (Battery Monitor status) 
byte  5 Alarms status MSB bits 15-8 (Inverter status)

// when aux = 0 read Aux Volts (16 bits, signed, units 10mV)
// (byte 8 below provides Aux input type selecxtion)
byte  6 Auxillary Volts LSB bits  7-0 
byte  7 Auxillary Volts MSB bits 14-8
byte  7 Auxillary Volts sign bit 15
// when aux = 1 read mid-point Volts (16 bits, units 10mV)
byte  6 Mid-point Volts LSB bits 7-0 
byte  7 Mid-point Volts MSB bits 15-8
// when aux = 2 read temperature in Kelvin (16 bits, units 10mK)
byte  6 Kelvin degrees LSB bits  7-0 
byte  7 Kelvin degrees MSB bits 15-8

byte  8 Aux input type: bits 0,1 =  0:aux 1:mid 2:Kelvin 3:none 

// Battery milli-Amps (22 bits, signed)
byte  8 bits 7-2  LSB Batt Amps: bottom 6 bits of mA value
byte  9 bits 1,0  LSB Batt Amps:  extra 2 bits 
        bits 7-2  MIB Batt Amps: middle 6 bits 
byte 10 bits 1,0  MIB Batt Amps:  extra 2 bits 
        bits 6-2  MSB Batt Amps:    top 5 bits of mA value
        bit  7        Batt A sign bit   1 bit

// Consumed Ah (20 bits, units 100mAh)
byte 11 Consumed Ah LSB bits  7-0
byte 12 Consumed Ah MIB bits 15-8
byte 13 Consumed Ah MSB bits  3-0 (upper 4 bits of Ah)

// State of Charge (10 bits, units 0.1%)
byte 13 SOC LSB bits 7-4 (lower 4 bits of SOC)
byte 14 SOC MSB bits 5-9 (upper 6 bits of SOC)

byte 14 bits 6,7 unused 
byte 15 bits 0-7 unused
------------------------------------------------------------------------ */
void reportBMvalues(){
  inf_TTG = false;
  float ttgDays = parseTimeToGo();            //       0 -> 65535 mins (45.51 days)
  float battV   = parseBattVolts();           // -327.68 -> 327.66 V
  uint32_t alarmBits = (static_cast<uint32_t>(output[5]) << 8) | output[4];
  int   aux   = output[8] & 0x03;           // 0:Aux 1: Mid 2: Kelvin 3: none
  float Aval = 0;          
  if      (aux == 0) Aval = parseAuxVolts();  // -327.68 -> 327.66 V;}
  else if (aux == 1) Aval = parseMidVolts();  //       0 -> 655.34 V
  else if (aux == 2) Aval = parseAuxKelvin(); //       0 -> 655.34 K (-273.15 -> 382.19 C)
  else if (aux == 3) Aval = 999.99;
  float   battA = parseBattAmps();            // -2097.152 -> 2097.150 A 
  float   Ah    = parseAmpHours();            // 0 -> 104,857.4 Ah
  float   SoC   = parseStateOfCharge();       // 0 -> 100%
  // -- in-line reporting -----------------------------------------------------
  //Serial << '\t';
  if (inf_TTG) Serial << "inf_"; else Serial << _FLOAT(ttgDays,1); Serial << "d ";
  if (na_batV) Serial << "n/a-"; else Serial << _FLOAT(battV  ,2); Serial << "V ";
  Serial << reportAlarms(alarmBits) << " ";
  if (na_aux) Serial << "n/a-"; else Serial << _FLOAT(Aval,2);
  if      (aux == 0 || aux == 1) Serial << "V";
  else if (aux == 2)             Serial << "K";
  else                           Serial << "X";
  Serial << "| ";
  Serial << aux << " ";
  if (na_batA) Serial << "n/a-"; else Serial << _FLOAT(battA,3); Serial << "A ";
  if (na_Ah)   Serial << "n/a-"; else Serial << _FLOAT(Ah   ,1); Serial << "Ah ";
  if (na_soc)  Serial << "n/a-"; else Serial << _FLOAT(SoC  ,1); Serial << "%\n";
}

// Some of the routines below use static_cast to convert integers  
// to floats while avoiding dud fractions from integer division.

// Remaining Battery 'Time to Go' in minutes
float parseTimeToGo(){
  uint16_t TTG_mins = (output[1] << 8) + output[0];  // NB little endian: byte[1] <-> byte[0]
  if (TTG_mins == 0xFFFF) inf_TTG = true;
  return (static_cast<float>(TTG_mins)/60/24);   // integer units minutes converted to hours as float
}

// Note: SC & BM use same bytes (2,3) for battery volts
float parseBattVolts(){
  bool    neg       =  (output[3] & 0x80) >> 7;              // extract sign bit for signed int
  int32_t batt_mV10 = ((output[3] & 0x7F) << 8) + output[2];  // exclude sign bit from byte 3
  if (batt_mV10 == 0x7FFF) na_batV = true;                       
  if (neg) batt_mV10 = batt_mV10 - 32768;       // 2's complement = val - 2^(b-1) b = bit# = 16
  return (static_cast<float>(batt_mV10)/100);   // integer units 10mV converted to V as float
}

/* -- AUX ------------------------------------------------------------------------------------
aux = 0 selects auxilliary voltage source
aux = 1 selects mid-point voltage
aux = 2 selects battery temperature in degrees Kelvin 
aux = 3 means result is 'N/A' or 'off' */

// only called when aux = 0
float parseAuxVolts(){
  bool    neg      =  (output[7] & 0x80) >> 7;               // extract sign bit
  int32_t aux_mV10 = ((output[7] & 0x7F) << 8) + output[6]; // exclude sign bit from byte[7]
  if (aux_mV10 == 0x7FFF) na_aux = true;                       
  if (neg) aux_mV10 = aux_mV10 - 32768;         // 2's complement = val - 2^(b-1) b = bit# = 16
  return (static_cast<float>(aux_mV10)/100);   // integer units 10mV converted to V as float
}

// only called when aux = 1
float parseMidVolts(){
  int32_t aux_mV10 = (output[7] << 8) + output[6];
  if (aux_mV10 == 0xFFFF) na_aux = true;                       
  return (static_cast<float>(aux_mV10)/100);   // integer units 10mV converted to V, as float
}

// only called when aux = 2
float parseAuxKelvin(){
  int32_t aux_mK10 = (output[7] << 8) + output[6];
  if  (aux_mK10 == 0xFFFF) na_aux = true;
  return (static_cast<float>(aux_mK10)/100);    // integer units 10 milli-Kelvin, converted to Kelvin, as float
}

/*
float parseAuxCelsius(){
  int32_t aux_mK10 = (output[7] << 8) + output[6];
  int32_t aux_mC10;
  if  (aux_mK10 == 0xFFFF) na_aux = true;
  else aux_mC10 = aux_mK10 - 27315;             // Celius = Kelvin - 273.15        
  return (static_cast<float>(aux_mC10)/100);    // integer units 10mC converted to degrees C, as float
  }*/

// -------------------------------------------------------------------------------------------

// Battery Current (signed) 22 bits = sign bit + 21 bits
float parseBattAmps(){
  bool    neg =  (output[10] & 0x80) >> 7;                                        // bit  21
  int32_t mA  = ((output[8]  & 0xFC) >> 2) + ((output[9]  & 0x03) << 6)       + // bits  0 - 7 
               (((output[9]  & 0xFC) >> 2) + ((output[10] & 0x03) << 6) << 8) + // bits  8 - 15
               (((output[10] & 0x7C) >> 2)                                << 16); // bits 16 - 20
  if (mA == 0x1FFFFF) na_batA = true;
  if (neg) mA = mA - 2097152;                   // 2's complement = val - 2^(b-1) where b = bits = 22
  return (static_cast<float>(mA)/1000);         // convert mA to float A
}

// Amp Hours consumed 20 bits (unsigned) integer units 0.1Ah (100mAh). 
float parseAmpHours(){
  uint32_t mAh100 = output[11]       +            // bits  0 - 7 
                   (output[12] << 8) +            // bits  8 - 15
                  ((output[13] & 0x0F) << 16);    // bits 16 - 19
  if (mAh100 == 0xFFFFF) na_Ah = true;
  return (static_cast<float>(mAh100)/10);           // integer units 100mAh converted to Ah as float 
}

// State of charge 0-100% in units of 0.1%, as 10 bits (unsigned)  
float parseStateOfCharge(){
  uint16_t soc01 = ((output[13] & 0xF0) >> 4) +   // bits 0 - 3
                   ((output[14] & 0x0F) << 4) +   // bits 4 - 7
                   ((output[14] & 0x30) << 4);    // bits 8 - 9
  if (soc01 == 0x3FF) na_soc = true;
  if (soc01  > 1000) soc01 = 9999;                  // flag error if > 100% = 1000/10 
  return (static_cast<float>(soc01)/10);            // integer units 0.1% converted to % as float 
}


// Gemini recommends new/delete to avoid memory leaks
//  char* alarms = new reportAlarms();
//  Serial << alarms << " ";
//  delete[] alarms; // Free the dynamically allocated memory

// NB: according to VE_REG_ALARM_REASONS list (see .png)
// values <= 0x0080 are for Battery Monitors  (i.e. bits from lower byte only)
// values >= 0x0100 are for Victron Inverters (i.e. bits from upper byte only)
char * reportAlarms(uint32_t alarmBits){
  char * alarmState = new char[5];                        // Gemini recommends new/delete to avoid memory leaks
  strcpy(alarmState, "none");
  //uint32_t alarmBits = (output[5])+(output[4]);
  if (countBitsSet(alarmBits) == 1){
    if (alarmBits == 0x0001) strcpy(alarmState, "lo_V");
    if (alarmBits == 0x0002) strcpy(alarmState, "hi_V");
    if (alarmBits == 0x0004) strcpy(alarmState, "socL");
    if (alarmBits == 0x0008) strcpy(alarmState, "lo_S");
    if (alarmBits == 0x0010) strcpy(alarmState, "hi_S");
    if (alarmBits == 0x0020) strcpy(alarmState, "lo_C");
    if (alarmBits == 0x0040) strcpy(alarmState, "hi_C");
    if (alarmBits == 0x0080) strcpy(alarmState, "midV");
    if (alarmBits == 0x0100) strcpy(alarmState, "OVRL");
    if (alarmBits == 0x0200) strcpy(alarmState, "DCrp");
    if (alarmBits == 0x0400) strcpy(alarmState, "loAC");
    if (alarmBits == 0x0800) strcpy(alarmState, "hiAC");
    if (alarmBits == 0x1000) strcpy(alarmState, "Shrt");
    if (alarmBits == 0x2000) strcpy(alarmState, "Lock");} 
  else if (alarmBits > 0) strcpy(alarmState, "Mult");
  return alarmState;
}

// returns count of bits that are set in val
uint32_t countBitsSet(uint32_t val){
  uint32_t count = 0;
  while (val) {count += val & 1;     // Check the least significant bit
               val >>= 1;}           // Right-shift to check the next bit
  return count;
}

// --------------------------------- Shared routines ---------------------------------------------
bool checkForbadArgs(){
  int x = wc_AesCtrEncrypt(   NULL, output, cipher, sizeof(cipher)/sizeof(byte)); //, WC_NO_ERR_TRACE(BAD_FUNC_ARG));
  int y = wc_AesCtrEncrypt(&aesDec, NULL,   cipher, sizeof(cipher)/sizeof(byte)); //, WC_NO_ERR_TRACE(BAD_FUNC_ARG));
  int z = wc_AesCtrEncrypt(&aesDec, output, NULL,   sizeof(cipher)/sizeof(byte)); //, WC_NO_ERR_TRACE(BAD_FUNC_ARG));
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

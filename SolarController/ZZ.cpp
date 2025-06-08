// Global definitions

#include "ZZ.h"

#include <ctype.h>                                            // provides toupper() function

bool VERBOSE  = false;                                        // true = verbose,            false = quiet mode
bool FILTERING = false;                                       // true = filtering on, false = off 
// Some Victon SC do not support load amps (e.g. MPPT100/30)
// However others do (e.g SmartSolar MPPT 75/10,75/15,100/15 & 100/20)   
// To disable load amps reporting, set to false
bool LOAD_AMPS = true;                                        // true = Enabled, false = Disabled 

const char dashes[] PROGMEM = " ------------------- ";
const char line[]   PROGMEM = "..........................................................\n";

// NB: Beware - Serial Monitor must be set with no line ending, else will be CR/LF detected here
void processSerialCommands() {
  if(Serial.available() > 0) {
    char readChar = Serial.read();
    if (readChar >= 42 && readChar <= 122) {                    // ignore if not between '*' and 'z' in ASCII table 
      readChar = toupper(readChar);                             // convert lower case characters to upper case
      switch(readChar){
        case 'V': if (VERBOSE)  {VERBOSE  = false; Serial << F("\nVERBOSE - off\n\n") ;}
                  else          {VERBOSE  = true;  Serial << F("\nVERBOSE - ON\n")    ;} break;
        case 'F': if (FILTERING){FILTERING = false; Serial << F("\nFILTERING - off\n\n");}
                  else          {FILTERING = true;  Serial << F("\nFILTERING - ON\n\n" );} break;        
      } 
    } 
  } 
}

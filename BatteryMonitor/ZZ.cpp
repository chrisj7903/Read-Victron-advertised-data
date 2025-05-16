// Global definitions

#include "ZZ.h"

#include <ctype.h>                    // provides toupper() function

bool VERBOSE  = false;  // true = verbose,         false = quiet mode

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
      } 
    } 
  } 
}
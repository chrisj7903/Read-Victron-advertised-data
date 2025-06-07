#pragma once

#include <Streaming.h> 

extern const char dashes[];
extern const char line[];  
extern void processSerialCommands();
extern bool VERBOSE;
extern bool FILTERING;

// -----------------------------------------------------------------------------------------------
// refer forum thread "Squeezing Code into UNO ..." post #70 (Aug 2016) re: __FlashStringHelper and the F() macro for accessing PROGMEM
#define CF(x) ((const __FlashStringHelper *)x)                                  // to stream a const char[]

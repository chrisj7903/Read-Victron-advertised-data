#pragma once

#include <Streaming.h> 

extern const char dashes[];                                                     // in PROGMEM
extern const char line[];  

extern void processSerialCommands();

extern bool VERBOSE;
extern bool FILTERING;
extern bool LOAD_AMPS;

#define CF(x) ((const __FlashStringHelper *)x)                                  // to stream a const char[]

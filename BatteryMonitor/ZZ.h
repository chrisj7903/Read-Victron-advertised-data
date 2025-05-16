#pragma once

//const char CJ_V[] = ".1";       // version identifier
#define LINUX         0         // compiling in 0:Windows 1:Ubuntu

#include <Streaming.h> 

extern const char dashes[];
extern const char line[];  
extern void processSerialCommands();
extern bool VERBOSE;

// -----------------------------------------------------------------------------------------------
// refer forum thread "Squeezing Code into UNO ..." post #70 (Aug 2016) re: __FlashStringHelper and the F() macro for accessing PROGMEM
#define CF(x) ((const __FlashStringHelper *)x)                                  // to stream a const char[]


// "__FILE__" is predefined by the IDE as the full sketch folder location + file name. 
// "FILENAME" is extracted here as the file name only, ignoring the folder location.
#if (LINUX)
  #define FILENAME (strrchr(__FILE__,'/') ? strrchr(__FILE__,'/')+1 : __FILE__)   // sketch file name - Linux
#else
  #define FILENAME (strrchr(__FILE__,'\\') ? strrchr(__FILE__,'\\')+1 : __FILE__) // sketch file name - Windows
#endif
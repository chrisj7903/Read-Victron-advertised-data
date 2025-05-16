#pragma once

const char CJ_V[] = ".u";
#define LINUX         0               // 0:compiling in Windows 1:Ubuntu

#include <Streaming.h> 

extern const char dashes[];                                                     // in PROGMEM
extern const char line[];  

extern void processSerialCommands();

extern bool VERBOSE;

// -----------------------------------------------------------------------------------------------

#define CF(x) ((const __FlashStringHelper *)x)                                  // to stream a const char[]

// "__FILE__" is predefined by the IDE as the full sketch folder location + file name. 
// "FILENAME" is extracted here as the file name only, ignoring the folder location.
#if (LINUX)
  #define FILENAME (strrchr(__FILE__,'/') ? strrchr(__FILE__,'/')+1 : __FILE__)   // sketch file name - Linux
#else
  #define FILENAME (strrchr(__FILE__,'\\') ? strrchr(__FILE__,'\\')+1 : __FILE__) // sketch file name - Windows
#endif
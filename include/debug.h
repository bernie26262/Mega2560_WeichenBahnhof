#pragma once
#include <Arduino.h>

#ifdef DEBUG_SERIAL
  #define DBG(x)   Serial.print(x)
  #define DBGLN(x) Serial.println(x)
#else
  #define DBG(x)   do{}while(0)
  #define DBGLN(x) do{}while(0)
#endif

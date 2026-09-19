#pragma once
#include <Arduino.h>

// Called by the sketch; HTTP handlers only enqueue work, never access the reader.
void beginWebControl();
void serviceWeb();

// Implemented by the sketch. All callbacks run on the Arduino loop task.
String controlStateJson();
String backupJson();
bool controlBusy();
bool controlHasBackup();
// Returns an HTTP status; 202 means accepted for execution in the main loop.
int enqueueWebAction(const String& action, const String& uid,
                     const String& confirmation, String& error);

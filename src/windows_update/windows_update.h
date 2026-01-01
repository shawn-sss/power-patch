#pragma once

#ifdef _WIN32
bool openWindowsUpdateSettings();
bool startWindowsUpdateScan();
void closeWindowsUpdateWindowAfterDelay(int delayMs);
#endif

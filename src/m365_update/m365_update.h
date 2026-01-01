#pragma once

#ifdef _WIN32
bool startMicrosoft365Update();
void closeWindowByProcessAfterDelay(const wchar_t *exeName, int delayMs);
#endif

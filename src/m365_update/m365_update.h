#pragma once

#ifdef _WIN32
enum class Microsoft365UpdateStatus
{
    Ready,
    Disabled,
    NotInstalled,
};

Microsoft365UpdateStatus queryMicrosoft365UpdateStatus();
bool startMicrosoft365Update();
void closeWindowByProcessAfterDelay(const wchar_t *exeName, int delayMs);
#endif

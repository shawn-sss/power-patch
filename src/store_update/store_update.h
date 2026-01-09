#pragma once

#ifdef _WIN32
enum class MicrosoftStoreStatus
{
    Available,
    Disabled,
    Uninstalled,
};

MicrosoftStoreStatus queryMicrosoftStoreStatus();
bool openMicrosoftStoreLibrary();
bool clickMicrosoftStoreGetUpdates(bool closeAfter);
#endif

#pragma once

#include <QString>

enum class UpdateKind { Windows, Store, Office };
enum class UpdateOutcome { Started, Attention, Unavailable, Failed };

struct UpdateResult {
    UpdateOutcome outcome;
    QString message;
};

// Runs on a worker thread. A successful launch does not confirm installation.
UpdateResult launchUpdate(UpdateKind kind, bool closeAfter);

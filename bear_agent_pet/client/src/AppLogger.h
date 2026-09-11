#pragma once

#include <QString>

namespace AppLogger {
void install();
QString logDirectory();
QString logFilePath();
QString readRecent(int maxBytes = 256 * 1024);
bool clear();
}

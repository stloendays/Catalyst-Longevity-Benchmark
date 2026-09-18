#pragma once

#include <QDateTime>
#include <QString>

namespace TonyQuietMode {

bool isActive();
QDateTime untilUtc();
void clear();
void setForMinutes(int minutes);
void setUntilTomorrowMorning(int hour=8);
QString remainingLabel(bool chinese=false);

}

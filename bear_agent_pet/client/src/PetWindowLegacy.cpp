// Tony 1.0.9 keeps all established interaction/physics code from PetWindowV7.cpp,
// but renames only its old paintEvent implementation. The live paintEvent is
// provided by PetWindowFullCanvas.cpp. Pre-include every dependency before the
// macro so Qt/system headers are never parsed with paintEvent renamed.
#include "PetWindow.h"
#include "AppLogger.h"
#include "TonyResponseRouter.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QCoreApplication>
#include <QCursor>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QScreen>
#include <QSettings>
#include <QSysInfo>
#include <QTime>
#include <QtMath>
#include <limits>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#endif

#define paintEvent legacyPaintEvent
#include "PetWindowV7.cpp"
#undef paintEvent

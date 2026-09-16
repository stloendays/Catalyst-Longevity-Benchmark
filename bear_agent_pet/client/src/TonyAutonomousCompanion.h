#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

class PetWindow;

class TonyAutonomousCompanion final : public QObject {
public:
    explicit TonyAutonomousCompanion(PetWindow *pet, QObject *parent=nullptr);

    QString mode() const;
    void setMode(const QString &mode);
    void announceMode();

private:
    void scheduleNext();
    void tick();

    PetWindow *pet_{nullptr};
    QTimer timer_;
    qint64 startedAtMs_{0};
    qint64 lastSpeechAtMs_{0};
    QString hourBucket_;
    QString morningDate_;
    QString paulaDate_;
    int spokenThisHour_{0};
};

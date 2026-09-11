#pragma once

#include <QObject>
#include <QJsonObject>
#include <QStringList>

class QWidget;
class QSystemTrayIcon;

class LocalBridge : public QObject {
    Q_OBJECT
public:
    explicit LocalBridge(QWidget *promptParent=nullptr, QObject *parent=nullptr);

    QStringList capabilities() const;
    bool enabled() const;
    void setEnabled(bool enabled);
    void setTrayIcon(QSystemTrayIcon *tray);

public slots:
    void execute(const QString &requestId, const QString &tool, const QJsonObject &args);

signals:
    void finished(const QString &requestId,
                  const QString &tool,
                  bool ok,
                  const QJsonObject &result,
                  const QString &error);

private:
    bool confirm(const QString &title, const QString &detail) const;
    void fail(const QString &requestId, const QString &tool, const QString &error);
    void succeed(const QString &requestId, const QString &tool, const QJsonObject &result={});

    QWidget *promptParent_{nullptr};
    QSystemTrayIcon *tray_{nullptr};
};

#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>

class SshTunnel : public QObject {
    Q_OBJECT
public:
    explicit SshTunnel(QObject *parent=nullptr);
    void start();
    void stop();
    bool running() const;

signals:
    void statusChanged(const QString &status);

private slots:
    void launch();

private:
    QProcess process_;
    QTimer retryTimer_;
    bool stopping_{false};
};

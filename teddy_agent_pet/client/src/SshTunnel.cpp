#include "SshTunnel.h"

#include <QHostAddress>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QTcpSocket>

namespace {
bool localForwardReady(int port) {
    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, static_cast<quint16>(port));
    const bool ok=socket.waitForConnected(300);
    if(ok) socket.disconnectFromHost();
    return ok;
}
}

SshTunnel::SshTunnel(QObject *parent): QObject(parent) {
    retryTimer_.setInterval(15000);
    retryTimer_.setSingleShot(true);
    connect(&retryTimer_, &QTimer::timeout, this, &SshTunnel::launch);

    connect(&process_, &QProcess::started, this, [this]{
        emit statusChanged("SSH tunnel running");
    });
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError){
        const auto detail=process_.errorString().trimmed();
        emit statusChanged(detail.isEmpty()
            ? "SSH tunnel unavailable"
            : "SSH tunnel unavailable: "+detail.left(180));
        if(!stopping_ && !retryTimer_.isActive()) retryTimer_.start();
    });
    connect(&process_, qOverload<int,QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int, QProcess::ExitStatus){
        if(stopping_) return;
        const auto err=QString::fromUtf8(process_.readAllStandardError()).trimmed();
        emit statusChanged(err.isEmpty()
            ? "SSH tunnel waiting for local SSH access"
            : "SSH tunnel unavailable: "+err.left(220));
        if(!retryTimer_.isActive()) retryTimer_.start();
    });
}

bool SshTunnel::running() const {
    return process_.state()!=QProcess::NotRunning;
}

int SshTunnel::localPort() const {
    return QSettings().value("ssh/local_port",18790).toInt();
}

bool SshTunnel::ready() const {
    return localForwardReady(localPort());
}

void SshTunnel::start() {
    QSettings s;
    if(!s.value("ssh/enabled",true).toBool()) {
        emit statusChanged("SSH tunnel disabled");
        return;
    }
    stopping_=false;
    launch();
}

void SshTunnel::launch() {
    if(stopping_ || running()) return;

    QSettings s;
    const QString host=s.value("ssh/host","150.158.27.206").toString().trimmed();
    const QString user=s.value("ssh/user","ubuntu").toString().trimmed();
    const int localPort=s.value("ssh/local_port",18790).toInt();
    const int remotePort=s.value("ssh/remote_port",18790).toInt();
    const QString identity=s.value("ssh/identity_file","").toString().trimmed();

    // A user may already have opened the required forwarding tunnel manually.
    // Reuse that listener instead of starting a duplicate ssh.exe that would fail
    // with "address already in use" and produce a misleading warning.
    if(localForwardReady(localPort)) {
        emit statusChanged("SSH tunnel ready (existing local forward)");
        if(!retryTimer_.isActive()) retryTimer_.start();
        return;
    }

    const QString program=QStandardPaths::findExecutable("ssh");
    if(program.isEmpty()) {
        emit statusChanged("SSH tunnel unavailable: Windows OpenSSH client not found");
        if(!retryTimer_.isActive()) retryTimer_.start();
        return;
    }

    QStringList args{
        "-N",
        "-L", QString("%1:127.0.0.1:%2").arg(localPort).arg(remotePort),
        "-o", "BatchMode=yes",
        "-o", "ExitOnForwardFailure=yes",
        "-o", "ServerAliveInterval=30",
        "-o", "ServerAliveCountMax=3",
        "-o", "ConnectTimeout=10",
        "-o", "StrictHostKeyChecking=accept-new"
    };
    if(!identity.isEmpty()) args << "-i" << identity;
    args << QString("%1@%2").arg(user,host);

    process_.setProgram(program);
    process_.setArguments(args);
    process_.setProcessChannelMode(QProcess::SeparateChannels);
    process_.start();
    emit statusChanged("Starting SSH tunnel");
}

void SshTunnel::stop() {
    stopping_=true;
    retryTimer_.stop();
    if(running()) {
        process_.terminate();
        if(!process_.waitForFinished(1500)) process_.kill();
    }
}

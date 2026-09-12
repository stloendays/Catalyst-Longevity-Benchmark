#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QProgressBar;
class QPushButton;
class QTableWidget;
class QTextEdit;
class UpdateManager;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(UpdateManager *updater, QWidget *parent = nullptr);

signals:
    void languageChanged(const QString &language);
    void reconnectRequested();

private:
    QString trUi(const QString &en, const QString &zh) const;
    void refreshLogs();
    void refreshUpdateSummary();
    void refreshOwnerPanel();
    void refreshOwnerDeviceList();
    void approveSelectedPairing();
    void approveTypedPairingCode();
    void revokeSelectedDevice();

    UpdateManager *updater_{nullptr};
    QComboBox *languageBox_{nullptr};
    QCheckBox *automaticUpdates_{nullptr};
    QLabel *currentVersion_{nullptr};
    QLabel *latestVersion_{nullptr};
    QLabel *updateStatus_{nullptr};
    QProgressBar *progress_{nullptr};
    QPushButton *checkNow_{nullptr};
    QPushButton *installNow_{nullptr};
    QTextEdit *logs_{nullptr};

    QNetworkAccessManager *ownerNetwork_{nullptr};
    QLabel *ownerStatus_{nullptr};
    QTableWidget *pendingPairings_{nullptr};
    QTableWidget *ownerDevices_{nullptr};
    QLineEdit *pairingCodeInput_{nullptr};
    QPushButton *approveSelected_{nullptr};
    QPushButton *approveCode_{nullptr};
    QPushButton *revokeDevice_{nullptr};
};

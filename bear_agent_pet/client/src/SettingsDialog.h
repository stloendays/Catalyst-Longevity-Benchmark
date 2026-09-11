#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;
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
};

#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;
class PetWindow;

class PetCreatorDialog final : public QDialog {
    Q_OBJECT
public:
    explicit PetCreatorDialog(PetWindow *pet, QWidget *parent=nullptr);

    void refresh();

private:
    void browseAndActivate();
    void resetToTony();
    void updatePreview(const QString &root);
    bool validateFolder(const QString &root, QString *name, QString *version,
                        QString *idlePath, QString *error) const;

    PetWindow *pet_{nullptr};
    QLabel *activeValue_{nullptr};
    QLabel *preview_{nullptr};
    QLineEdit *pathValue_{nullptr};
    QLabel *status_{nullptr};
    QPushButton *activateButton_{nullptr};
};

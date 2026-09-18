#pragma once

#include <QDialog>
#include <QHash>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class NewsCompanion;

class NewsSettingsDialog final : public QDialog {
    Q_OBJECT
public:
    explicit NewsSettingsDialog(NewsCompanion *news, QWidget *parent=nullptr);
    void refresh();

private:
    bool chinese() const;
    void rebuildText();
    void refreshLatest();

    NewsCompanion *news_{nullptr};
    QCheckBox *enabled_{nullptr};
    QComboBox *frequency_{nullptr};
    QSpinBox *quietStart_{nullptr};
    QSpinBox *quietEnd_{nullptr};
    QLabel *intro_{nullptr};
    QLabel *latest_{nullptr};
    QLabel *quietHint_{nullptr};
    QPushButton *checkNow_{nullptr};
    QPushButton *openLatest_{nullptr};
    QPushButton *close_{nullptr};
    QHash<QString,QCheckBox*> sourceBoxes_;
};

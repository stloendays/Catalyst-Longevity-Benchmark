from pathlib import Path

# mainwindow.h
p = Path('native/qt/src/mainwindow.h')
text = p.read_text(encoding='utf-8')
needle = '    explicit MainWindow(QWidget* parent = nullptr);\n'
insert = needle + '    bool captureSoftcopyrightScreenshots(const QString& outputDir);\n'
if 'captureSoftcopyrightScreenshots' not in text:
    text = text.replace(needle, insert, 1)
p.write_text(text, encoding='utf-8')

# mainwindow.cpp
p = Path('native/qt/src/mainwindow.cpp')
text = p.read_text(encoding='utf-8')
if '#include <QApplication>' not in text:
    text = text.replace('#include <QAbstractItemView>\n', '#include <QAbstractItemView>\n#include <QApplication>\n#include <QDir>\n', 1)

marker = '\nvoid MainWindow::newProject() {'
if 'bool MainWindow::captureSoftcopyrightScreenshots' not in text:
    method = r'''

bool MainWindow::captureSoftcopyrightScreenshots(const QString& outputDir) {
    QDir directory;
    if (!directory.mkpath(outputDir)) return false;

    const auto datasets = BuiltInDatasets::all();
    if (datasets.isEmpty()) return false;

    const auto findDataset = [&datasets](const QString& name) -> const BuiltInDataset* {
        for (const auto& dataset : datasets) {
            if (dataset.name == name) return &dataset;
        }
        return nullptr;
    };

    const auto* stable = findDataset(QStringLiteral("长期稳定性对比"));
    const auto* interval = findDataset(QStringLiteral("T90 区间定位"));
    const auto* longRun = findDataset(QStringLiteral("长周期寿命下限"));
    const auto* quality = findDataset(QStringLiteral("数据质量检查"));
    if (!stable || !interval || !longRun || !quality) return false;

    resize(1440, 900);
    show();
    QApplication::processEvents();

    const QStringList navLabels = {
        QStringLiteral("项目"), QStringLiteral("首页"), QStringLiteral("数据"),
        QStringLiteral("资料"), QStringLiteral("分析"), QStringLiteral("AI 助手"),
        QStringLiteral("设置")
    };

    const auto capture = [this, &outputDir, &navLabels](int pageIndex, int tabIndex, const QString& fileName) {
        pages_->setCurrentIndex(pageIndex);
        const auto navButtons = findChildren<QPushButton*>();
        for (auto* button : navButtons) {
            if (button->objectName() != QStringLiteral("navButton")) continue;
            button->setChecked(pageIndex >= 0 && pageIndex < navLabels.size()
                               && button->text() == navLabels[pageIndex]);
        }
        if (tabIndex >= 0) {
            if (auto* tabs = pages_->currentWidget()->findChild<QTabWidget*>()) {
                tabs->setCurrentIndex(tabIndex);
            }
        }
        QApplication::processEvents();
        repaint();
        QApplication::processEvents();
        return grab().save(QDir(outputDir).filePath(fileName), "PNG");
    };

    bool ok = true;

    setRecords(stable->records, QStringLiteral("内置数据集 · %1").arg(stable->name), false);
    ok = capture(1, -1, QStringLiteral("01_首页_长期稳定性总览.png")) && ok;

    setRecords(quality->records, QStringLiteral("内置数据集 · %1").arg(quality->name), false);
    ok = capture(2, -1, QStringLiteral("02_数据_质量检查.png")) && ok;

    setRecords(interval->records, QStringLiteral("内置数据集 · %1").arg(interval->name), false);
    ok = capture(4, 0, QStringLiteral("03_分析_T90寿命区间.png")) && ok;

    setRecords(stable->records, QStringLiteral("内置数据集 · %1").arg(stable->name), false);
    ok = capture(4, 1, QStringLiteral("04_分析_同时间对比.png")) && ok;

    setRecords(longRun->records, QStringLiteral("内置数据集 · %1").arg(longRun->name), false);
    ok = capture(4, 2, QStringLiteral("05_分析_实验建议.png")) && ok;

    setRecords(stable->records, QStringLiteral("内置数据集 · %1").arg(stable->name), false);
    ok = capture(4, 3, QStringLiteral("06_分析_公开参考库.png")) && ok;
    ok = capture(3, -1, QStringLiteral("07_资料_资料整理.png")) && ok;
    ok = capture(5, -1, QStringLiteral("08_AI助手_资料边界.png")) && ok;
    ok = capture(0, -1, QStringLiteral("09_项目_项目管理.png")) && ok;
    ok = capture(6, -1, QStringLiteral("10_设置_软件信息.png")) && ok;

    hide();
    return ok;
}
'''
    if marker not in text:
        raise SystemExit('mainwindow.cpp insertion marker not found')
    text = text.replace(marker, method + marker, 1)
p.write_text(text, encoding='utf-8')

# main.cpp
p = Path('native/qt/src/main.cpp')
text = p.read_text(encoding='utf-8')
needle = '    app.setWindowIcon(makeApplicationIcon());\n\n'
if '--copyright-capture=' not in text:
    block = r'''    QString copyrightCaptureDir;
    for (const auto& argument : app.arguments()) {
        const QString prefix = QStringLiteral("--copyright-capture=");
        if (argument.startsWith(prefix)) {
            copyrightCaptureDir = argument.mid(prefix.size());
            break;
        }
    }
    if (!copyrightCaptureDir.isEmpty()) {
        catalyst::MainWindow captureWindow;
        applyWindowPolish(captureWindow);
        const bool captured = captureWindow.captureSoftcopyrightScreenshots(copyrightCaptureDir);
        return captured ? 0 : 41;
    }

'''
    if needle not in text:
        raise SystemExit('main.cpp insertion marker not found')
    text = text.replace(needle, needle + block, 1)
p.write_text(text, encoding='utf-8')

print('Temporary software-copyright screenshot capture support applied.')

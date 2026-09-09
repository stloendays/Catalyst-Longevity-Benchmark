from pathlib import Path


def replace(path: Path, old: str, new: str, label: str) -> None:
    text = path.read_text(encoding='utf-8')
    if old not in text:
        raise SystemExit(f'Missing {label} in {path}')
    path.write_text(text.replace(old, new), encoding='utf-8')

main = Path('native/qt/src/main.cpp')
window = Path('native/qt/src/mainwindow.cpp')
cmake = Path('native/qt/CMakeLists.txt')
installer = Path('native/qt/installer/CatalystLongevity.iss')

# Use an Office-like Chinese UI font when available and let widgets inherit it.
replace(main, '#include <QFont>\n#include <QFrame>', '#include <QFont>\n#include <QFontDatabase>\n#include <QFrame>', 'QFontDatabase include')
helper = '''QString preferredUiFontFamily() {
    const QStringList installed = QFontDatabase::families();
    const QStringList preferred = {
        QStringLiteral("DengXian"),
        QStringLiteral("Microsoft YaHei UI"),
        QStringLiteral("Microsoft YaHei")
    };
    for (const auto& family : preferred) {
        if (installed.contains(family, Qt::CaseInsensitive)) return family;
    }
    return QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
}

'''
replace(main, 'void polishChineseCopy(catalyst::MainWindow& window) {', helper + 'void polishChineseCopy(catalyst::MainWindow& window) {', 'preferred font helper')
replace(main, '    app.setFont(QFont(QStringLiteral("Microsoft YaHei"), 10));', '    app.setFont(QFont(preferredUiFontFamily(), 10));', 'application font')

replace(window, '            font-family:"Microsoft YaHei";\n', '', 'hard-coded QSS font')
replace(window, '''QLabel* heading(const QString& text, int pointSize = 20) {
    auto* label = new QLabel(text);
    label->setFont(QFont(QStringLiteral("Microsoft YaHei"), pointSize, QFont::DemiBold));
    label->setObjectName(QStringLiteral("pageHeading"));
    return label;
}''', '''QLabel* heading(const QString& text, int pointSize = 20) {
    auto* label = new QLabel(text);
    QFont font = label->font();
    font.setPointSize(pointSize);
    font.setWeight(QFont::DemiBold);
    label->setFont(font);
    label->setObjectName(QStringLiteral("pageHeading"));
    return label;
}''', 'heading font inheritance')

# Rename the real executable and installer-facing product, not only the window caption.
replace(cmake, '    OUTPUT_NAME "Catalyst Longevity Research"', '    OUTPUT_NAME "智策"', 'CMake executable name')

replace(installer, '#define MyAppName "Catalyst Longevity Research"', '#define MyAppName "智策"', 'installer app name')
replace(installer, '#define MyAppExeName "Catalyst Longevity Research.exe"', '#define MyAppExeName "智策.exe"', 'installer executable name')
replace(installer, '#define MyAppPublisher "Catalyst Longevity Research"', '#define MyAppPublisher "智策"', 'installer publisher')
replace(installer, '#define MyAppVersion "Native Desktop Preview"\n', '', 'remove preview version label')
replace(installer, 'AppVersion={#MyAppVersion}\n', 'AppVerName={#MyAppName}\n', 'installer display name')
replace(installer, 'OutputBaseFilename=Catalyst-Longevity-Research-Setup', 'OutputBaseFilename=Zhice-Setup', 'installer output name')

print('Zhice native package and typography aligned.')

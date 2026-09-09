from pathlib import Path
import re

root = Path('.')


def replace_exact(path: Path, old: str, new: str, label: str) -> None:
    text = path.read_text(encoding='utf-8')
    if old not in text:
        raise SystemExit(f'Missing expected block for {label} in {path}')
    path.write_text(text.replace(old, new), encoding='utf-8')


def replace_regex(path: Path, pattern: str, replacement: str, label: str) -> None:
    text = path.read_text(encoding='utf-8')
    updated, count = re.subn(pattern, replacement, text, flags=re.S)
    if count != 1:
        raise SystemExit(f'Expected one {label} replacement in {path}, got {count}')
    path.write_text(updated, encoding='utf-8')


mainwindow = root / 'native/qt/src/mainwindow.cpp'
main = root / 'native/qt/src/main.cpp'
report = root / 'native/qt/src/reportexporter.cpp'

new_style = r'''QString gptMonochromeStyle() {
    return QStringLiteral(R"(
        QMainWindow, QWidget { background:#F5F6F8; color:#111827; font-size:13px; }
        QMainWindow { background:#F5F6F8; }
        QStatusBar { background:#FFFFFF; color:#6B7280; border-top:1px solid #E5E7EB; min-height:28px; }
        QStatusBar QLabel { color:#6B7280; padding:0 8px; }

        #sidebar { background:#F8F9FB; border-right:1px solid #E5E7EB; }
        #brand { color:#111827; background:transparent; border:none; padding:3px 10px 0; font-size:27px; font-weight:700; letter-spacing:.8px; }
        #brandSubtitle { color:#8A919E; background:transparent; border:none; padding:0 10px 12px; font-size:11px; }
        #navButton { color:#4B5563; background:transparent; border:none; border-left:3px solid transparent; border-radius:7px; padding:10px 12px; text-align:left; font-weight:500; min-height:25px; }
        #navButton:hover { background:#EEF0F3; color:#111827; }
        #navButton:checked { background:transparent; border-left:3px solid #111827; color:#111827; font-weight:650; }
        #navButton:pressed { background:#E7E9ED; }

        #pageHeading { color:#0F172A; font-size:25px; font-weight:700; }
        #mutedText { color:#6B7280; line-height:1.55; }
        #sectionTitle { color:#111827; font-size:15px; font-weight:650; }
        #metricTitle { color:#7A8290; font-size:12px; font-weight:500; }
        #metricValue { color:#111827; font-size:21px; font-weight:650; }
        #sourcePath { color:#1F2937; font-weight:600; }

        #metricCard { background:#FFFFFF; border:1px solid #E3E6EA; border-radius:12px; min-height:70px; }
        #metricCard:hover { border-color:#C9CDD4; }
        #decisionCard { background:#FFFFFF; border:1px solid #E3E6EA; border-radius:12px; min-height:105px; }
        #decisionCard:hover { border-color:#C9CDD4; }
        #decisionTitle { color:#4B5563; font-size:12px; font-weight:600; }
        #decisionDetail { color:#7A8290; font-size:12px; }

        #panel, #gptSurface, #evidenceSurface, #aiSurface { background:#FFFFFF; border:1px solid #E3E6EA; border-radius:12px; }
        #panel:hover, #gptSurface:hover, #evidenceSurface:hover, #aiSurface:hover { border-color:#CDD1D7; }
        #gptSurface { border-color:#DEE1E5; }
        #evidenceSurface { border-left:2px solid #4B5563; }
        #aiSurface { background:#FFFFFF; border-color:#E3E6EA; }
        #infoPanel { background:#F8F9FB; border:1px solid #E3E6EA; border-radius:10px; }

        #statusNeutral, #statusGood, #statusWarn, #statusBad {
            background:transparent;
            border:none;
            border-radius:0;
            padding:0;
            font-size:13px;
            font-weight:600;
        }
        #statusNeutral { color:#6B7280; }
        #statusGood { color:#168A52; }
        #statusWarn { color:#B7791F; }
        #statusBad { color:#B42318; }
        #guardStatus { color:#111827; font-size:14px; font-weight:650; padding:2px 0; }

        #primaryButton { background:#111827; color:#FFFFFF; border:1px solid #111827; border-radius:8px; padding:9px 15px; font-weight:600; min-height:22px; }
        #primaryButton:hover { background:#253044; border-color:#253044; }
        #primaryButton:pressed { background:#374151; border-color:#374151; }
        #primaryButton:disabled { background:#B7BCC5; border-color:#B7BCC5; color:#F9FAFB; }
        #secondaryButton { background:#FFFFFF; color:#374151; border:1px solid #D4D7DD; border-radius:8px; padding:9px 15px; font-weight:600; min-height:22px; }
        #secondaryButton:hover { background:#F4F5F7; border-color:#AEB4BE; color:#111827; }
        #secondaryButton:pressed { background:#ECEEF1; }

        QLineEdit, QComboBox, QDoubleSpinBox, QTextEdit { background:#FFFFFF; color:#111827; border:1px solid #D4D7DD; border-radius:8px; padding:7px 9px; selection-background-color:#374151; selection-color:#FFFFFF; }
        QLineEdit:hover, QComboBox:hover, QDoubleSpinBox:hover, QTextEdit:hover { border-color:#AEB4BE; }
        QLineEdit:focus, QComboBox:focus, QDoubleSpinBox:focus, QTextEdit:focus { border:1px solid #6B7280; background:#FFFFFF; }
        QLineEdit:disabled, QComboBox:disabled, QDoubleSpinBox:disabled, QTextEdit:disabled { background:#F4F5F7; color:#9CA3AF; }
        QComboBox::drop-down, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { border:none; background:transparent; }

        QTableWidget { background:#FFFFFF; alternate-background-color:#FBFBFC; border:1px solid #E3E6EA; border-radius:9px; gridline-color:#ECEEF1; selection-background-color:#EEF0F3; selection-color:#111827; }
        QHeaderView::section { background:#F7F8FA; color:#5F6774; border:none; border-right:1px solid #ECEEF1; border-bottom:1px solid #E3E6EA; padding:9px 8px; font-weight:600; }
        QTableWidget::item { padding:7px; border:none; }
        QTableWidget::item:hover { background:#F3F4F6; }
        QTableWidget::item:selected { background:#EAECF0; color:#111827; }

        QTabWidget::pane { background:#FFFFFF; border:1px solid #E3E6EA; border-radius:10px; top:-1px; }
        QTabBar::tab { background:transparent; color:#727986; border:none; border-bottom:2px solid transparent; padding:9px 18px 8px; margin-right:8px; }
        QTabBar::tab:hover { color:#111827; }
        QTabBar::tab:selected { background:transparent; color:#111827; border-bottom:2px solid #111827; font-weight:650; }

        QScrollBar:vertical { background:transparent; width:9px; margin:3px 2px; }
        QScrollBar::handle:vertical { background:#D4D7DD; border-radius:4px; min-height:28px; }
        QScrollBar::handle:vertical:hover { background:#AEB4BE; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
        QScrollBar:horizontal { background:transparent; height:9px; margin:2px 3px; }
        QScrollBar::handle:horizontal { background:#D4D7DD; border-radius:4px; min-width:28px; }
        QScrollBar::handle:horizontal:hover { background:#AEB4BE; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; }
        QToolTip { background:#111827; color:#FFFFFF; border:1px solid #374151; border-radius:6px; padding:6px 8px; }
    )");
}'''
replace_regex(
    mainwindow,
    r'QString gptMonochromeStyle\(\) \{.*?\n\}',
    new_style,
    'main stylesheet',
)

old_heading = '''QLabel* heading(const QString& text, int pointSize = 20) {
    auto* label = new QLabel(text);
    label->setFont(QFont(QStringLiteral("Microsoft YaHei UI"), pointSize, QFont::DemiBold));
    label->setObjectName(QStringLiteral("pageHeading"));
    return label;
}'''
new_heading = '''QLabel* heading(const QString& text, int pointSize = 20) {
    auto* label = new QLabel(text);
    QFont font = label->font();
    font.setPointSize(pointSize);
    font.setWeight(QFont::DemiBold);
    label->setFont(font);
    label->setObjectName(QStringLiteral("pageHeading"));
    return label;
}'''
replace_exact(mainwindow, old_heading, new_heading, 'heading font inheritance')

old_chip = '''void setStatusChip(QLabel* label, const QString& text, const QString& objectName) {
    if (!label) return;
    label->setText(text);
    if (label->objectName() != objectName) {'''
new_chip = '''void setStatusChip(QLabel* label, const QString& text, const QString& objectName) {
    if (!label) return;
    const bool semantic = objectName == QStringLiteral("statusGood")
        || objectName == QStringLiteral("statusWarn")
        || objectName == QStringLiteral("statusBad");
    label->setText(semantic ? QStringLiteral("●  %1").arg(text) : text);
    if (label->objectName() != objectName) {'''
replace_exact(mainwindow, old_chip, new_chip, 'plain semantic status labels')

new_sidebar = r'''QWidget* MainWindow::buildSidebar() {
    auto* sidebar = new QFrame;
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(220);
    auto* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(16, 22, 16, 18);
    layout->setSpacing(7);

    auto* brand = new QLabel(QStringLiteral("智策"));
    brand->setObjectName(QStringLiteral("brand"));
    layout->addWidget(brand);
    auto* brandSubtitle = new QLabel(QStringLiteral("催化剂研究与实验决策"));
    brandSubtitle->setObjectName(QStringLiteral("brandSubtitle"));
    layout->addWidget(brandSubtitle);
    layout->addSpacing(12);

    auto* group = new QButtonGroup(sidebar);
    group->setExclusive(true);
    const QStringList labels = {
        QStringLiteral("首页"),
        QStringLiteral("数据"),
        QStringLiteral("资料"),
        QStringLiteral("分析"),
        QStringLiteral("AI 助手"),
        QStringLiteral("项目"),
        QStringLiteral("设置")
    };
    const int pageIndices[] = {1, 2, 3, 4, 5, 0, 6};

    for (int i = 0; i < labels.size(); ++i) {
        auto* button = new QPushButton(labels[i]);
        button->setCheckable(true);
        button->setObjectName(QStringLiteral("navButton"));
        button->setCursor(Qt::PointingHandCursor);
        const int pageIndex = pageIndices[i];
        group->addButton(button, pageIndex);
        layout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, pageIndex]() {
            pages_->setCurrentIndex(pageIndex);
        });
        if (i == 0) button->setChecked(true);
    }

    layout->addStretch();
    return sidebar;
}'''
replace_regex(
    mainwindow,
    r'QWidget\* MainWindow::buildSidebar\(\) \{.*?\n\}\n\nQWidget\* MainWindow::buildProjectPage',
    new_sidebar + '\n\nQWidget* MainWindow::buildProjectPage',
    'sidebar redesign',
)

replace_exact(
    mainwindow,
    '    root->addWidget(pages_, 1);\n\n    connect(evidencePage_, &EvidencePage::evidenceChanged, this, [this]() {',
    '    root->addWidget(pages_, 1);\n    pages_->setCurrentIndex(1);\n\n    connect(evidencePage_, &EvidencePage::evidenceChanged, this, [this]() {',
    'default homepage',
)

replace_exact(
    mainwindow,
    '    titleBox->addWidget(heading(QStringLiteral("首页")));\n    titleBox->addWidget(muted(QStringLiteral(\n        "查看催化剂长期表现、寿命指标和实验条件检查结果。")));',
    '    titleBox->addWidget(heading(QStringLiteral("欢迎使用智策")));\n    titleBox->addWidget(muted(QStringLiteral(\n        "整合实验数据、科研资料与分析能力，辅助催化剂长期稳定性研究和实验决策。")));',
    'overview welcome copy',
)
replace_exact(
    mainwindow,
    '    decisionTop->addWidget(muted(QStringLiteral("先检查数据和实验条件，再查看比较结果。状态颜色只表示当前资料是否完整。")));',
    '    decisionTop->addWidget(muted(QStringLiteral("先检查数据和实验条件，再查看比较结果。状态标记仅表示当前信息是否完整。")));',
    'decision hint',
)

old_settings = '''    cardLayout->addWidget(new QLabel(QStringLiteral("催化剂长期稳定性评估与实验决策系统")));
    cardLayout->addWidget(muted(QStringLiteral("Windows 原生桌面应用 · C++20 + Qt 6 + SQLite")));
    cardLayout->addSpacing(10);
    cardLayout->addWidget(new QLabel(QStringLiteral("运行方式：本地桌面窗口，不启动浏览器，不依赖 Streamlit。")));
    cardLayout->addWidget(new QLabel(QStringLiteral("数据输入：CSV / Excel .xlsx。")));
    cardLayout->addWidget(new QLabel(QStringLiteral("项目存储：本地 .clrproj 文件（实验数据、资料关联和确认状态）。")));
    cardLayout->addWidget(new QLabel(QStringLiteral("核心功能：数据检查、寿命分析、同时间对比、实验建议、资料整理和 PDF 报告。")));'''
new_settings = '''    auto* productName = new QLabel(QStringLiteral("智策"));
    productName->setObjectName(QStringLiteral("pageHeading"));
    cardLayout->addWidget(productName);
    cardLayout->addWidget(muted(QStringLiteral("催化剂长期稳定性评估与实验决策")));
    cardLayout->addSpacing(10);
    cardLayout->addWidget(new QLabel(QStringLiteral("技术架构：C++20 + Qt 6 + SQLite。")));
    cardLayout->addWidget(new QLabel(QStringLiteral("数据输入：CSV / Excel .xlsx。")));
    cardLayout->addWidget(new QLabel(QStringLiteral("项目存储：本地 .clrproj 文件（实验数据、资料关联和确认状态）。")));
    cardLayout->addWidget(new QLabel(QStringLiteral("核心功能：数据检查、寿命分析、同时间对比、实验建议、资料整理和 PDF 报告。")));'''
replace_exact(mainwindow, old_settings, new_settings, 'settings product card')

# Main application font and product identity.
replace_exact(
    main,
    '#include <QFont>\n#include <QFrame>',
    '#include <QFont>\n#include <QFontDatabase>\n#include <QFrame>',
    'font database include',
)

font_helper = '''QString preferredUiFontFamily() {
    const QStringList families = QFontDatabase::families();
    const QStringList preferred = {
        QStringLiteral("DengXian"),
        QStringLiteral("Microsoft YaHei UI"),
        QStringLiteral("Microsoft YaHei")
    };
    for (const auto& family : preferred) {
        if (families.contains(family, Qt::CaseInsensitive)) return family;
    }
    return QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
}

'''
replace_exact(
    main,
    'void polishChineseCopy(catalyst::MainWindow& window) {',
    font_helper + 'void polishChineseCopy(catalyst::MainWindow& window) {',
    'preferred UI font helper',
)

replace_exact(
    main,
    '    window.setWindowTitle(QStringLiteral("催化剂长期稳定性评估与实验决策系统"));',
    '    window.setWindowTitle(QStringLiteral("智策"));',
    'window title',
)
replace_exact(
    main,
    '            color = QColor(QStringLiteral("#E5E5E5"));',
    '            color = QColor(QStringLiteral("#4B5563"));',
    'light sidebar icon color',
)

replace_regex(
    main,
    r'\n    const auto frames = window\.findChildren<QFrame\*>\(\);.*?\n    \}\n\}',
    '\n}',
    'remove legacy card shadow pass',
)

replace_exact(
    main,
    '    app.setApplicationName(QStringLiteral("Catalyst Longevity Research"));\n    app.setApplicationDisplayName(QStringLiteral("催化剂长期稳定性评估与实验决策系统"));\n    app.setStyle(QStringLiteral("Fusion"));\n    app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));',
    '    app.setApplicationName(QStringLiteral("智策"));\n    app.setApplicationDisplayName(QStringLiteral("智策"));\n    app.setStyle(QStringLiteral("Fusion"));\n    app.setFont(QFont(preferredUiFontFamily(), 10));',
    'application identity and font',
)

# PDF report identity follows the product name while preserving a descriptive subtitle.
replace_exact(
    report,
    '    html += QStringLiteral("<h1>催化剂长期稳定性评估与实验决策系统</h1>");\n    html += QStringLiteral("<p class=\'meta\'>催化剂寿命数据分析与实验辅助报告</p>");',
    '    html += QStringLiteral("<h1>智策</h1>");\n    html += QStringLiteral("<p class=\'meta\'>催化剂长期稳定性评估与实验决策报告</p>");',
    'PDF report identity',
)

print('Zhice UI polish applied successfully.')

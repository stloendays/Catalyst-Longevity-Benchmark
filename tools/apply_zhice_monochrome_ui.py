from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
main_window = ROOT / "native/qt/src/mainwindow.cpp"
main_cpp = ROOT / "native/qt/src/main.cpp"

mw = main_window.read_text(encoding="utf-8")
mc = main_cpp.read_text(encoding="utf-8")

new_style = r'''QString gptMonochromeStyle() {
    return QStringLiteral(R"(
        QMainWindow, QWidget {
            background:#F5F5F5;
            color:#171717;
            font-family:"Microsoft YaHei";
            font-size:13px;
        }
        QMainWindow { background:#F5F5F5; }
        QStatusBar {
            background:#FFFFFF;
            color:#737373;
            border-top:1px solid #E5E5E5;
            min-height:29px;
        }
        QStatusBar QLabel { color:#737373; padding:0 8px; }

        #sidebar {
            background:#0D0D0D;
            border-right:1px solid #202020;
        }
        #brand {
            color:#FFFFFF;
            background:transparent;
            border:none;
            padding:4px 8px 2px;
            font-size:23px;
            font-weight:700;
            letter-spacing:.6px;
        }
        #brandSub {
            color:#8A8A8A;
            background:transparent;
            border:none;
            padding:0 8px 10px;
            font-size:11px;
            font-weight:500;
        }
        #navButton {
            color:#BDBDBD;
            background:transparent;
            border:none;
            border-left:3px solid transparent;
            border-radius:0;
            padding:11px 12px 11px 14px;
            text-align:left;
            font-weight:500;
            min-height:25px;
        }
        #navButton:hover {
            background:#171717;
            color:#FFFFFF;
        }
        #navButton:checked {
            background:transparent;
            border-left:3px solid #FFFFFF;
            color:#FFFFFF;
            font-weight:700;
        }
        #navButton:pressed { background:#1F1F1F; }

        #pageHeading {
            color:#111111;
            font-size:25px;
            font-weight:700;
        }
        #mutedText { color:#737373; line-height:1.55; }
        #sectionTitle { color:#171717; font-size:15px; font-weight:700; }
        #metricTitle { color:#737373; font-size:12px; font-weight:500; }
        #metricValue { color:#111111; font-size:21px; font-weight:700; }
        #sourcePath { color:#262626; font-weight:650; }

        #metricCard,
        #decisionCard,
        #panel,
        #gptSurface,
        #evidenceSurface,
        #aiSurface {
            background:#FFFFFF;
            border:1px solid #E2E2E2;
            border-radius:12px;
        }
        #metricCard { min-height:70px; }
        #decisionCard { min-height:105px; }
        #metricCard:hover,
        #decisionCard:hover,
        #panel:hover,
        #gptSurface:hover,
        #evidenceSurface:hover,
        #aiSurface:hover {
            background:#FFFFFF;
            border-color:#BFBFBF;
        }
        #evidenceSurface { border-left:3px solid #262626; }
        #aiSurface { background:#FFFFFF; }
        #infoPanel {
            background:#FAFAFA;
            border:1px solid #E5E5E5;
            border-radius:10px;
        }
        #decisionTitle { color:#525252; font-size:12px; font-weight:650; }
        #decisionDetail { color:#737373; font-size:12px; }

        #statusNeutral,
        #statusGood,
        #statusWarn,
        #statusBad {
            background:transparent;
            border:none;
            border-radius:0;
            padding:2px 0;
            font-size:13px;
            font-weight:700;
        }
        #statusNeutral { color:#525252; }
        #statusGood { color:#166534; }
        #statusWarn { color:#8A5A00; }
        #statusBad { color:#A61B1B; }
        #guardStatus { color:#171717; font-size:14px; font-weight:700; padding:4px 0; }

        #primaryButton {
            background:#111111;
            color:#FFFFFF;
            border:1px solid #111111;
            border-radius:8px;
            padding:9px 16px;
            font-weight:600;
            min-height:22px;
        }
        #primaryButton:hover { background:#2B2B2B; border-color:#2B2B2B; }
        #primaryButton:pressed { background:#3D3D3D; border-color:#3D3D3D; }
        #primaryButton:disabled { background:#B5B5B5; border-color:#B5B5B5; color:#F5F5F5; }
        #secondaryButton {
            background:#FFFFFF;
            color:#262626;
            border:1px solid #D4D4D4;
            border-radius:8px;
            padding:9px 16px;
            font-weight:600;
            min-height:22px;
        }
        #secondaryButton:hover { background:#FAFAFA; border-color:#9E9E9E; color:#111111; }
        #secondaryButton:pressed { background:#F0F0F0; }

        QLineEdit, QComboBox, QDoubleSpinBox, QTextEdit {
            background:#FFFFFF;
            color:#171717;
            border:1px solid #D4D4D4;
            border-radius:8px;
            padding:7px 9px;
            selection-background-color:#E5E5E5;
            selection-color:#111111;
        }
        QLineEdit:hover, QComboBox:hover, QDoubleSpinBox:hover, QTextEdit:hover { border-color:#A3A3A3; }
        QLineEdit:focus, QComboBox:focus, QDoubleSpinBox:focus, QTextEdit:focus { border:1px solid #525252; background:#FFFFFF; }
        QLineEdit:disabled, QComboBox:disabled, QDoubleSpinBox:disabled, QTextEdit:disabled { background:#F5F5F5; color:#A3A3A3; }
        QComboBox::drop-down, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { border:none; background:transparent; }

        QTableWidget {
            background:#FFFFFF;
            alternate-background-color:#FCFCFC;
            border:1px solid #E5E5E5;
            border-radius:9px;
            gridline-color:#EFEFEF;
            selection-background-color:#EEEEEE;
            selection-color:#111111;
        }
        QHeaderView::section {
            background:#FAFAFA;
            color:#525252;
            border:none;
            border-right:1px solid #EEEEEE;
            border-bottom:1px solid #E5E5E5;
            padding:9px 8px;
            font-weight:650;
        }
        QTableWidget::item { padding:7px; border:none; }
        QTableWidget::item:hover { background:#F5F5F5; }
        QTableWidget::item:selected { background:#ECECEC; color:#111111; }

        QTabWidget::pane {
            background:#FFFFFF;
            border:1px solid #E5E5E5;
            border-radius:10px;
            top:-1px;
        }
        QTabBar::tab {
            background:transparent;
            color:#737373;
            border:none;
            border-bottom:2px solid transparent;
            padding:9px 18px;
            margin-right:6px;
            font-weight:500;
        }
        QTabBar::tab:hover { background:transparent; color:#262626; }
        QTabBar::tab:selected {
            background:transparent;
            color:#111111;
            border-bottom:2px solid #111111;
            font-weight:700;
        }

        QScrollBar:vertical { background:transparent; width:10px; margin:3px 2px; }
        QScrollBar::handle:vertical { background:#D4D4D4; border-radius:4px; min-height:28px; }
        QScrollBar::handle:vertical:hover { background:#A3A3A3; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
        QScrollBar:horizontal { background:transparent; height:10px; margin:2px 3px; }
        QScrollBar::handle:horizontal { background:#D4D4D4; border-radius:4px; min-width:28px; }
        QScrollBar::handle:horizontal:hover { background:#A3A3A3; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width:0; }
        QToolTip { background:#171717; color:#FFFFFF; border:1px solid #333333; border-radius:6px; padding:6px 8px; }
    )");
}'''

mw, n = re.subn(
    r'QString gptMonochromeStyle\(\) \{.*?\n\}',
    new_style,
    mw,
    count=1,
    flags=re.S,
)
assert n == 1, "Could not replace gptMonochromeStyle"

mw = mw.replace('QFont(QStringLiteral("Microsoft YaHei UI"), pointSize, QFont::DemiBold)',
                'QFont(QStringLiteral("Microsoft YaHei"), pointSize, QFont::DemiBold)')

old_brand = '''    auto* brand = new QLabel(QStringLiteral("CATALYST\\nLONGEVITY"));
    brand->setObjectName(QStringLiteral("brand"));
    layout->addWidget(brand);
    layout->addSpacing(22);'''
new_brand = '''    auto* brand = new QLabel(QStringLiteral("智策"));
    brand->setObjectName(QStringLiteral("brand"));
    layout->addWidget(brand);
    auto* brandSub = new QLabel(QStringLiteral("催化剂研究与实验决策"));
    brandSub->setObjectName(QStringLiteral("brandSub"));
    layout->addWidget(brandSub);
    layout->addSpacing(16);'''
assert old_brand in mw, "Sidebar brand block not found"
mw = mw.replace(old_brand, new_brand, 1)

old_nav = '''    const QStringList labels = {
        QStringLiteral("项目"),
        QStringLiteral("首页"),
        QStringLiteral("数据"),
        QStringLiteral("资料"),
        QStringLiteral("分析"),
        QStringLiteral("AI 助手"),
        QStringLiteral("设置")
    };

    for (int i = 0; i < labels.size(); ++i) {
        auto* button = new QPushButton(labels[i]);
        button->setCheckable(true);
        button->setObjectName(QStringLiteral("navButton"));
        button->setCursor(Qt::PointingHandCursor);
        group->addButton(button, i);
        layout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, i]() {
            pages_->setCurrentIndex(i);
        });
        if (i == 0) button->setChecked(true);
    }

    layout->addStretch();
    auto* buildLabel = new QLabel(QStringLiteral("Native Desktop\\nC++ / Qt 6"));
    buildLabel->setObjectName(QStringLiteral("sidebarFoot"));
    layout->addWidget(buildLabel);'''
new_nav = '''    const QStringList labels = {
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
        const int pageIndex = pageIndices[i];
        auto* button = new QPushButton(labels[i]);
        button->setCheckable(true);
        button->setObjectName(QStringLiteral("navButton"));
        button->setCursor(Qt::PointingHandCursor);
        group->addButton(button, pageIndex);
        layout->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, pageIndex]() {
            pages_->setCurrentIndex(pageIndex);
        });
        if (pageIndex == 1) button->setChecked(true);
    }

    layout->addStretch();'''
assert old_nav in mw, "Sidebar nav/footer block not found"
mw = mw.replace(old_nav, new_nav, 1)

mw = mw.replace('titleBox->addWidget(heading(QStringLiteral("首页")));',
                'titleBox->addWidget(heading(QStringLiteral("欢迎使用智策")));', 1)
mw = mw.replace('"查看催化剂长期表现、寿命指标和实验条件检查结果。"',
                '"集中查看长期稳定性、寿命指标、实验条件和下一步实验建议。"', 1)
mw = mw.replace('"先检查数据和实验条件，再查看比较结果。状态颜色只表示当前资料是否完整。"',
                '"先检查数据与实验条件，再查看比较结果。状态标记仅用于提示当前信息完整度。"', 1)

formal = 'cardLayout->addWidget(new QLabel(QStringLiteral("催化剂长期稳定性评估与实验决策系统")));'
replacement = '''    auto* productName = new QLabel(QStringLiteral("智策"));
    productName->setObjectName(QStringLiteral("sectionTitle"));
    cardLayout->addWidget(productName);
    cardLayout->addWidget(muted(QStringLiteral("催化剂长期稳定性评估与实验决策系统")));'''
assert formal in mw, "Settings product-name line not found"
mw = mw.replace(formal, replacement, 1)

main_window.write_text(mw, encoding="utf-8")

# Remove the unused legacy teal stylesheet from main.cpp so the source has one visual system.
mc, n = re.subn(
    r'\nQString polishedStyleSheet\(\) \{.*?\n\}\n\nvoid polishChineseCopy',
    '\nvoid polishChineseCopy',
    mc,
    count=1,
    flags=re.S,
)
assert n == 1, "Legacy polishedStyleSheet block not found"

mc = mc.replace('''        } else if (text == QStringLiteral("CATALYST\\nLONGEVITY")) {
            label->setText(QStringLiteral("催化剂寿命研究\\nCATALYST LONGEVITY"));
        } else if (text == QStringLiteral("Native Desktop\\nC++ / Qt 6")) {
            label->setText(QStringLiteral("原生桌面版\\nC++20 · Qt 6"));''', '')
mc = mc.replace('window.setWindowTitle(QStringLiteral("催化剂长期稳定性评估与实验决策系统"));',
                'window.setWindowTitle(QStringLiteral("智策"));', 1)
mc = mc.replace('sidebar->setFixedWidth(258);', 'sidebar->setFixedWidth(232);', 1)
mc = mc.replace('app.setOrganizationName(QStringLiteral("Catalyst Longevity Research"));',
                'app.setOrganizationName(QStringLiteral("ZhiCe"));', 1)
mc = mc.replace('app.setApplicationName(QStringLiteral("Catalyst Longevity Research"));',
                'app.setApplicationName(QStringLiteral("ZhiCe"));', 1)
mc = mc.replace('app.setApplicationDisplayName(QStringLiteral("催化剂长期稳定性评估与实验决策系统"));',
                'app.setApplicationDisplayName(QStringLiteral("智策"));', 1)
mc = mc.replace('app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));',
                'app.setFont(QFont(QStringLiteral("Microsoft YaHei"), 10));', 1)

main_cpp.write_text(mc, encoding="utf-8")

print("Updated:")
print(main_window.relative_to(ROOT))
print(main_cpp.relative_to(ROOT))

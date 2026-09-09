from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f"missing marker for {label}")
    return text.replace(old, new, 1)

# mainwindow.h
path = ROOT / "native/qt/src/mainwindow.h"
text = path.read_text(encoding="utf-8")
text = replace_once(
    text,
    "public:\n    explicit MainWindow(QWidget* parent = nullptr);\n",
    "public:\n    explicit MainWindow(QWidget* parent = nullptr);\n    bool captureDocumentationScreenshots(const QString& outputDir, QString* errorMessage = nullptr);\n",
    "mainwindow public capture API",
)
path.write_text(text, encoding="utf-8")

# mainwindow.cpp
path = ROOT / "native/qt/src/mainwindow.cpp"
text = path.read_text(encoding="utf-8")
text = replace_once(text, "#include <QCloseEvent>\n", "#include <QCloseEvent>\n#include <QCoreApplication>\n#include <QDir>\n", "mainwindow includes 1")
text = replace_once(text, "#include <QPushButton>\n", "#include <QPushButton>\n#include <QPixmap>\n", "mainwindow includes 2")

constructor_marker = '''MainWindow::MainWindow(QWidget* parent)\n    : QMainWindow(parent) {\n    resize(1380, 860);\n    setMinimumSize(1120, 720);\n    buildUi();\n    applyTheme();\n    newProject();\n    QTimer::singleShot(0, this, [this]() {\n        setStyleSheet(gptMonochromeStyle());\n        for (auto* frame : findChildren<QFrame*>()) {\n            frame->setAttribute(Qt::WA_Hover, true);\n            frame->setMouseTracking(true);\n            if (frame->objectName() != QStringLiteral("sidebar")) frame->setGraphicsEffect(nullptr);\n        }\n        for (auto* table : findChildren<QTableWidget*>()) table->setMouseTracking(true);\n    });\n}\n\n'''

capture_impl = r'''MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    resize(1380, 860);
    setMinimumSize(1120, 720);
    buildUi();
    applyTheme();
    newProject();
    QTimer::singleShot(0, this, [this]() {
        setStyleSheet(gptMonochromeStyle());
        for (auto* frame : findChildren<QFrame*>()) {
            frame->setAttribute(Qt::WA_Hover, true);
            frame->setMouseTracking(true);
            if (frame->objectName() != QStringLiteral("sidebar")) frame->setGraphicsEffect(nullptr);
        }
        for (auto* table : findChildren<QTableWidget*>()) table->setMouseTracking(true);
    });
}

bool MainWindow::captureDocumentationScreenshots(const QString& outputDir, QString* errorMessage) {
    const auto datasets = BuiltInDatasets::all();
    if (datasets.size() < 5) {
        if (errorMessage) *errorMessage = QStringLiteral("内置数据集不足，无法生成说明书截图。");
        return false;
    }

    QDir output(outputDir);
    if (!output.exists() && !output.mkpath(QStringLiteral("."))) {
        if (errorMessage) *errorMessage = QStringLiteral("无法创建截图目录：%1").arg(outputDir);
        return false;
    }

    resize(1440, 900);
    show();

    const auto settle = [this]() {
        for (int i = 0; i < 4; ++i) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 80);
            repaint();
        }
    };

    const auto selectPage = [this, &settle](int pageIndex, const QString& navText) {
        pages_->setCurrentIndex(pageIndex);
        for (auto* button : findChildren<QPushButton*>()) {
            if (button->objectName() == QStringLiteral("navButton")) {
                button->setChecked(button->text() == navText);
            }
        }
        settle();
    };

    const auto loadDataset = [this, &datasets, &settle](int index) {
        const auto& dataset = datasets[index];
        setRecords(dataset.records, QStringLiteral("内置数据集 · %1").arg(dataset.name), false);
        projectDirty_ = false;
        updateProjectUi();
        settle();
    };

    const auto selectAnalysisTab = [this, &settle](int tabIndex) {
        for (auto* tabs : findChildren<QTabWidget*>()) {
            if (tabs->count() >= 4) {
                tabs->setCurrentIndex(tabIndex);
                break;
            }
        }
        settle();
    };

    const auto saveShot = [this, &output, &settle, errorMessage](const QString& fileName) {
        settle();
        const QPixmap image = grab();
        const QString path = output.filePath(fileName);
        if (image.isNull() || !image.save(path, "PNG")) {
            if (errorMessage) *errorMessage = QStringLiteral("截图保存失败：%1").arg(path);
            return false;
        }
        return true;
    };

    loadDataset(0);
    selectPage(1, QStringLiteral("首页"));
    if (!saveShot(QStringLiteral("01_首页_长期稳定性总览.png"))) return false;

    loadDataset(4);
    selectPage(2, QStringLiteral("数据"));
    if (!saveShot(QStringLiteral("02_数据_质量检查.png"))) return false;

    loadDataset(1);
    selectPage(4, QStringLiteral("分析"));
    selectAnalysisTab(0);
    if (!saveShot(QStringLiteral("03_分析_T90寿命指标.png"))) return false;

    loadDataset(0);
    selectPage(4, QStringLiteral("分析"));
    selectAnalysisTab(1);
    if (!saveShot(QStringLiteral("04_分析_同时间对比.png"))) return false;

    loadDataset(2);
    if (maxAdditionalHoursSpin_) maxAdditionalHoursSpin_->setValue(120.0);
    if (minSamplingIntervalSpin_) minSamplingIntervalSpin_->setValue(24.0);
    refreshResearchSupportViews();
    selectPage(4, QStringLiteral("分析"));
    selectAnalysisTab(2);
    if (!saveShot(QStringLiteral("05_分析_实验建议.png"))) return false;

    loadDataset(0);
    selectPage(4, QStringLiteral("分析"));
    selectAnalysisTab(3);
    if (!saveShot(QStringLiteral("06_分析_公开参考库.png"))) return false;

    QVector<EvidenceItem> evidence;
    EvidenceItem item1;
    item1.sourcePath = QStringLiteral("公开资料_长期稳定性研究.pdf");
    item1.sourceSha256 = QStringLiteral("7e8d2d3c9b3a8a6a1dd7d60e128fe20c");
    item1.sourcePage = 3;
    item1.category = QStringLiteral("doi");
    item1.term = QStringLiteral("DOI");
    item1.valueText = QStringLiteral("10.1002/cctc.201500379");
    item1.snippet = QStringLiteral("Ni 基催化剂在高温条件下开展长期稳定性测试，并报告了随时间变化的转化性能。");
    item1.boundCatalyst = QStringLiteral("Ni-CeO2");
    item1.boundTimeHours = 96.0;
    item1.status = QStringLiteral("condition_reviewed_context_only");
    item1.note = QStringLiteral("已核对催化剂、温度、时间点和性能指标，仅作为分析上下文。 ");
    evidence.append(item1);

    EvidenceItem item2;
    item2.sourcePath = QStringLiteral("公开资料_长期稳定性研究.pdf");
    item2.sourceSha256 = item1.sourceSha256;
    item2.sourcePage = 4;
    item2.category = QStringLiteral("temperature_c");
    item2.term = QStringLiteral("temperature");
    item2.valueText = QStringLiteral("700 °C");
    item2.snippet = QStringLiteral("长期稳定性评价在约 700 °C 条件下进行，用于对照当前实验温度量级。");
    item2.boundCatalyst = QStringLiteral("Ni-CeO2");
    item2.boundTimeHours = 144.0;
    item2.status = QStringLiteral("condition_reviewed_context_only");
    item2.note = QStringLiteral("温度与进料条件已人工核对。 ");
    evidence.append(item2);

    EvidenceItem item3;
    item3.sourcePath = QStringLiteral("待核对资料_补充实验.pdf");
    item3.sourceSha256 = QStringLiteral("a7b8c9d0e1f2a3b4c5d6e7f809101112");
    item3.sourcePage = 7;
    item3.category = QStringLiteral("duration_h");
    item3.term = QStringLiteral("duration");
    item3.valueText = QStringLiteral("200 h");
    item3.snippet = QStringLiteral("文献给出约 200 h 的稳定性测试窗口，尚需核对催化剂体系与空速条件。");
    item3.boundCatalyst = QStringLiteral("Ni-Al2O3");
    item3.boundTimeHours = 200.0;
    item3.status = QStringLiteral("bound_to_catalyst_time_requires_condition_review");
    item3.note = QStringLiteral("待确认空速和压力条件。 ");
    evidence.append(item3);

    if (evidencePage_) evidencePage_->setEvidenceItems(evidence);
    refreshDecisionOverview();
    selectPage(3, QStringLiteral("资料"));
    if (!saveShot(QStringLiteral("07_资料_关联与确认.png"))) return false;

    selectPage(5, QStringLiteral("AI 助手"));
    if (!saveShot(QStringLiteral("08_AI助手_资料准备.png"))) return false;

    selectPage(0, QStringLiteral("项目"));
    if (!saveShot(QStringLiteral("09_项目_本地管理.png"))) return false;

    selectPage(6, QStringLiteral("设置"));
    if (!saveShot(QStringLiteral("10_设置_软件信息.png"))) return false;

    loadDataset(3);
    selectPage(4, QStringLiteral("分析"));
    selectAnalysisTab(0);
    if (!saveShot(QStringLiteral("11_分析_条件差异阻止比较.png"))) return false;

    if (errorMessage) {
        *errorMessage = QStringLiteral("已生成 11 张智策实际 Qt 界面截图：%1").arg(output.absolutePath());
    }
    return true;
}

'''
text = replace_once(text, constructor_marker, capture_impl, "capture implementation")
path.write_text(text, encoding="utf-8")

# evidencepage.cpp: remove filled cell backgrounds and simplify remaining visible copy.
path = ROOT / "native/qt/src/evidencepage.cpp"
text = path.read_text(encoding="utf-8")
text = text.replace(
    '        "Packet 只包含已完成人工条件复核且绑定到明确催化剂与时间的证据；其他候选自动排除。这里展示的 Markdown 将作为后续 AI Analyst / Evidence Critic 的可审计上下文基础。"',
    '        "只有已关联到明确催化剂和时间点，并经人工确认的资料会进入 AI 可用内容；其余条目保留在资料列表中，不参与自动分析。"'
)
text = text.replace(
    '        "原生 PDF 读取只使用 PDF 自带文本层，不自动 OCR 扫描页；这避免 OCR 错误直接进入实验事实链。“条件已复核”仍只是人工上下文核对状态，不是实验真值认证，也不会自动进入性能轨迹、T90 或直接排名。"',
    '        "PDF 读取优先使用文件自带文本层，不自动对扫描页执行 OCR。人工确认只表示关键上下文已经核对，不会修改实验观测，也不会自动进入性能轨迹、T90 或直接排名。"'
)
old_status = '''        if (item.status == QStringLiteral("condition_reviewed_context_only")) {\n            statusItem->setForeground(QColor(QStringLiteral("#166534")));\n            statusItem->setBackground(QColor(QStringLiteral("#F0FDF4")));\n        } else if (item.status == QStringLiteral("candidate_requires_condition_binding")) {\n            statusItem->setForeground(QColor(QStringLiteral("#71717A")));\n            statusItem->setBackground(QColor(QStringLiteral("#F4F4F5")));\n        } else {\n            statusItem->setForeground(QColor(QStringLiteral("#92400E")));\n            statusItem->setBackground(QColor(QStringLiteral("#FFFBEB")));\n        }\n'''
new_status = '''        if (item.status == QStringLiteral("condition_reviewed_context_only")) {\n            statusItem->setText(QStringLiteral("●  %1").arg(statusItem->text()));\n            statusItem->setForeground(QColor(QStringLiteral("#166534")));\n        } else if (item.status == QStringLiteral("candidate_requires_condition_binding")) {\n            statusItem->setForeground(QColor(QStringLiteral("#71717A")));\n        } else {\n            statusItem->setText(QStringLiteral("●  %1").arg(statusItem->text()));\n            statusItem->setForeground(QColor(QStringLiteral("#8A5A00")));\n        }\n'''
text = replace_once(text, old_status, new_status, "evidence status cells")
path.write_text(text, encoding="utf-8")

# main.cpp: add command line screenshot mode before starting network integration.
path = ROOT / "native/qt/src/main.cpp"
text = path.read_text(encoding="utf-8")
marker = '''    catalyst::IntegrationGateway integrationGateway;\n'''
insert = '''    const QStringList arguments = app.arguments();\n    const int screenshotArgument = arguments.indexOf(QStringLiteral("--capture-screenshots"));\n    if (screenshotArgument >= 0) {\n        if (screenshotArgument + 1 >= arguments.size()) return 40;\n        catalyst::MainWindow window;\n        applyWindowPolish(window);\n        QString screenshotMessage;\n        const bool captured = window.captureDocumentationScreenshots(arguments[screenshotArgument + 1], &screenshotMessage);\n        return captured ? 0 : 41;\n    }\n\n    catalyst::IntegrationGateway integrationGateway;\n'''
text = replace_once(text, marker, insert, "main screenshot command")
path.write_text(text, encoding="utf-8")

# Permanent manual workflow for future real GUI screenshot regeneration.
workflow = ROOT / ".github/workflows/ui-screenshots.yml"
workflow.write_text('''name: Zhice Real UI Screenshots\n\non:\n  workflow_dispatch:\n\npermissions:\n  contents: read\n\njobs:\n  capture:\n    runs-on: windows-2022\n    timeout-minutes: 35\n    steps:\n      - name: Checkout repository\n        uses: actions/checkout@v4\n\n      - name: Install Qt 6\n        uses: jurplel/install-qt-action@v4\n        with:\n          version: "6.8.3"\n          arch: "win64_msvc2022_64"\n          cache: true\n          modules: "qtpdf"\n\n      - name: Configure and build\n        shell: pwsh\n        run: |\n          cmake -S native/qt -B build-qt -G "Visual Studio 17 2022" -A x64\n          cmake --build build-qt --config Release --parallel\n          if (-not (Test-Path "build-qt\\Release\\智策.exe")) { throw "智策.exe was not produced" }\n\n      - name: Run self-test\n        shell: pwsh\n        run: |\n          $exe = (Resolve-Path "build-qt\\Release\\智策.exe").Path\n          $test = Start-Process -FilePath $exe -ArgumentList "--self-test" -WorkingDirectory (Split-Path $exe) -Wait -PassThru\n          if ($test.ExitCode -ne 0) { throw "Native self-test failed with exit code $($test.ExitCode)" }\n\n      - name: Capture real Qt screens\n        shell: pwsh\n        run: |\n          New-Item -ItemType Directory -Force -Path "ui-screenshots" | Out-Null\n          $output = (Resolve-Path "ui-screenshots").Path\n          $exe = (Resolve-Path "build-qt\\Release\\智策.exe").Path\n          $capture = Start-Process -FilePath $exe -ArgumentList @("--capture-screenshots", $output) -WorkingDirectory (Split-Path $exe) -Wait -PassThru\n          if ($capture.ExitCode -ne 0) { throw "Screenshot capture failed with exit code $($capture.ExitCode)" }\n          $shots = Get-ChildItem "ui-screenshots" -Filter "*.png"\n          if ($shots.Count -lt 11) { throw "Expected at least 11 screenshots, got $($shots.Count)" }\n          $shots | Select-Object Name, Length\n\n      - name: Upload screenshots\n        uses: actions/upload-artifact@v4\n        with:\n          name: Zhice-Real-UI-Screenshots\n          path: ui-screenshots/*.png\n          if-no-files-found: error\n''', encoding="utf-8")

print("Applied Zhice screenshot capture and evidence styling updates.")

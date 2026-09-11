#include "LocalBridge.h"

#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMessageBox>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QUrl>
#include <QDateTime>

LocalBridge::LocalBridge(QWidget *promptParent, QObject *parent)
    : QObject(parent), promptParent_(promptParent) {}

QStringList LocalBridge::capabilities() const {
    return {
        "read_clipboard",
        "write_clipboard",
        "capture_screen",
        "open_url",
        "open_file",
        "show_notification"
    };
}

bool LocalBridge::enabled() const {
    return QSettings().value("local_bridge/enabled", true).toBool();
}

void LocalBridge::setEnabled(bool enabled) {
    QSettings().setValue("local_bridge/enabled", enabled);
}

void LocalBridge::setTrayIcon(QSystemTrayIcon *tray) {
    tray_ = tray;
}

bool LocalBridge::confirm(const QString &title, const QString &detail) const {
    return QMessageBox::question(
        promptParent_,
        title,
        detail + "\n\n只有这一次会执行。",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No) == QMessageBox::Yes;
}

void LocalBridge::fail(const QString &requestId, const QString &tool, const QString &error) {
    emit finished(requestId, tool, false, {}, error);
}

void LocalBridge::succeed(const QString &requestId, const QString &tool, const QJsonObject &result) {
    emit finished(requestId, tool, true, result, {});
}

void LocalBridge::execute(const QString &requestId, const QString &toolRaw, const QJsonObject &args) {
    const QString tool = toolRaw.trimmed().toLower();
    if(!enabled()) {
        fail(requestId, tool, "Tony 的本地工具目前已关闭。");
        return;
    }

    if(tool == "show_notification") {
        const QString title = args.value("title").toString("Tony").left(80);
        const QString text = args.value("text").toString().left(500);
        if(text.trimmed().isEmpty()) {
            fail(requestId, tool, "通知内容为空。");
            return;
        }
        if(tray_ && tray_->isVisible()) {
            tray_->showMessage(title, text, QSystemTrayIcon::Information, 5500);
            succeed(requestId, tool, {{"shown", true}});
        } else {
            fail(requestId, tool, "系统托盘不可用，无法显示通知。");
        }
        return;
    }

    if(tool == "read_clipboard") {
        if(!confirm("Tony 想读取剪贴板", "服务器 Agent 请求读取你当前剪贴板中的文本。")) {
            fail(requestId, tool, "用户拒绝读取剪贴板。");
            return;
        }
        const QString full = QApplication::clipboard()->text();
        constexpr int kMaxChars = 12000;
        const bool truncated = full.size() > kMaxChars;
        const QString text = full.left(kMaxChars);
        succeed(requestId, tool, {
            {"text", text},
            {"characters", full.size()},
            {"truncated", truncated}
        });
        return;
    }

    if(tool == "write_clipboard") {
        const QString text = args.value("text").toString();
        if(text.isEmpty()) {
            fail(requestId, tool, "没有要写入剪贴板的文本。");
            return;
        }
        const QString preview = text.left(240);
        if(!confirm("Tony 想修改剪贴板", QString("服务器 Agent 请求把下面内容写入剪贴板：\n\n%1%2")
                        .arg(preview, text.size() > preview.size() ? "…" : ""))) {
            fail(requestId, tool, "用户拒绝修改剪贴板。");
            return;
        }
        QApplication::clipboard()->setText(text);
        succeed(requestId, tool, {{"characters", text.size()}});
        return;
    }

    if(tool == "capture_screen") {
        if(!confirm("Tony 想截取屏幕", "服务器 Agent 请求保存当前屏幕截图。当前版本只会把截图保存到本机，不会自动上传。")) {
            fail(requestId, tool, "用户拒绝截屏。");
            return;
        }
        QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if(!screen) screen = QGuiApplication::primaryScreen();
        if(!screen) {
            fail(requestId, tool, "找不到可用屏幕。");
            return;
        }
        QPixmap shot = screen->grabWindow(0);
        if(shot.isNull()) {
            fail(requestId, tool, "截屏失败。");
            return;
        }
        QString base = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        if(base.isEmpty()) base = QDir::homePath();
        QDir dir(base + "/Tony");
        if(!dir.exists() && !dir.mkpath(".")) {
            fail(requestId, tool, "无法创建截图目录。");
            return;
        }
        const QString fileName = QString("Tony_%1.png")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz"));
        const QString path = dir.filePath(fileName);
        if(!shot.save(path, "PNG")) {
            fail(requestId, tool, "无法保存截图。");
            return;
        }
        succeed(requestId, tool, {
            {"path", QDir::toNativeSeparators(path)},
            {"width", shot.width()},
            {"height", shot.height()},
            {"uploaded", false}
        });
        return;
    }

    if(tool == "open_url") {
        const QUrl url(args.value("url").toString());
        const QString scheme = url.scheme().toLower();
        if(!url.isValid() || url.host().isEmpty() || (scheme != "http" && scheme != "https")) {
            fail(requestId, tool, "只允许打开合法的 http/https 地址。");
            return;
        }
        if(!confirm("Tony 想打开网页", QString("服务器 Agent 请求打开：\n\n%1").arg(url.toString()))) {
            fail(requestId, tool, "用户拒绝打开网页。");
            return;
        }
        if(!QDesktopServices::openUrl(url)) {
            fail(requestId, tool, "系统无法打开该网页。");
            return;
        }
        succeed(requestId, tool, {{"url", url.toString()}});
        return;
    }

    if(tool == "open_file") {
        const QString rawPath = args.value("path").toString();
        const QString path = QDir::cleanPath(QDir::fromNativeSeparators(rawPath));
        QFileInfo info(path);
        if(path.isEmpty() || !info.exists() || !info.isFile()) {
            fail(requestId, tool, "本地文件不存在或不是普通文件。");
            return;
        }
        if(!confirm("Tony 想打开文件", QString("服务器 Agent 请求打开本地文件：\n\n%1").arg(QDir::toNativeSeparators(info.absoluteFilePath())))) {
            fail(requestId, tool, "用户拒绝打开文件。");
            return;
        }
        if(!QDesktopServices::openUrl(QUrl::fromLocalFile(info.absoluteFilePath()))) {
            fail(requestId, tool, "系统无法打开该文件。");
            return;
        }
        succeed(requestId, tool, {
            {"path", QDir::toNativeSeparators(info.absoluteFilePath())},
            {"size_bytes", static_cast<double>(info.size())}
        });
        return;
    }

    // Deliberately no shell, PowerShell, delete-file or arbitrary write-file tool here.
    fail(requestId, tool, "不支持的本地工具。Tony 不允许服务器执行任意系统命令。");
}

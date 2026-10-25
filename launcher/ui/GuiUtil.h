#pragma once

#include <QFileInfo>
#include <QWidget>
#include <optional>

class QDialog;

namespace GuiUtil {
std::optional<QString> uploadPaste(const QString& name, const QFileInfo& filePath, QWidget* parentWidget);
std::optional<QString> uploadPaste(const QString& name, const QString& data, QWidget* parentWidget);
void setClipboardText(QString text);
QStringList BrowseForFiles(QString context, QString caption, QString filter, QString defaultPath, QWidget* parentWidget);
QString BrowseForFile(QString context, QString caption, QString filter, QString defaultPath, QWidget* parentWidget);

/// Restores the window geometry saved under settingsKey (if any) and saves the current
/// geometry again every time the dialog finishes. The setting must be registered by the
/// Application at startup. Call this at the end of the dialog constructor, after its
/// content (and any default sizing) is in place.
void rememberGeometry(class QDialog* dialog, const QString& settingsKey);
}  // namespace GuiUtil

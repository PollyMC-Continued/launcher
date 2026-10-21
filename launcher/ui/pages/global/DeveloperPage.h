// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QWidget>
#include "ui/pages/BasePage.h"

class DeveloperPage : public QWidget, public BasePage {
    Q_OBJECT
public:
    explicit DeveloperPage(QWidget* parent = nullptr);
    QString displayName() const override { return tr("Developer"); }
    QIcon icon() const override { return QIcon::fromTheme("applications-development"); }
    QString id() const override { return "developer-settings"; }
    bool shouldDisplay() const override;
private:
    void refreshStatus();
};

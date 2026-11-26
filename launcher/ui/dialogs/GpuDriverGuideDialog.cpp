// SPDX-License-Identifier: GPL-3.0-only
/*
 *  PollyMC-Continued - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "GpuDriverGuideDialog.h"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

#include "SoftwareRenderer.h"

namespace {
struct Vendor {
    const char* name;
    const char* url;
    QStringList keywords;
};

const QList<Vendor>& vendors()
{
    static const QList<Vendor> list = {
        { "Intel", "https://www.intel.com/content/www/us/en/support/detect.html", { "intel" } },
        { "AMD", "https://www.amd.com/en/support/download/drivers.html", { "amd", "radeon", "ati " } },
        { "NVIDIA", "https://www.nvidia.com/Download/index.aspx", { "nvidia", "geforce", "quadro", "rtx", "gtx" } },
    };
    return list;
}
}  // namespace

GpuDriverGuideDialog::GpuDriverGuideDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Graphics Driver Guide"));
    resize(520, 560);

    auto* layout = new QVBoxLayout(this);

    const QStringList gpus = SoftwareRenderer::detectedGpus();
    QString detected = gpus.isEmpty() ? tr("Your graphics card could not be detected automatically.")
                                      : tr("<b>Detected graphics:</b> %1").arg(gpus.join(", ").toHtmlEscaped());
    bool basicOnly = !gpus.isEmpty();
    for (const QString& gpu : gpus) {
        if (!gpu.toLower().contains("basic"))
            basicOnly = false;
    }
    if (basicOnly)
        detected += "<br>" + tr("Windows is using its basic built-in display driver, so no real graphics driver is installed.");
    auto* detectedLabel = new QLabel(detected, this);
    detectedLabel->setWordWrap(true);
    detectedLabel->setTextFormat(Qt::RichText);
    layout->addWidget(detectedLabel);

    auto* buttons = new QHBoxLayout();
    for (const Vendor& vendor : vendors()) {
        auto* button = new QPushButton(tr("%1 drivers").arg(vendor.name), this);
        const QUrl url(vendor.url);
        connect(button, &QPushButton::clicked, this, [url] { QDesktopServices::openUrl(url); });
        for (const QString& gpu : gpus) {
            for (const QString& keyword : vendor.keywords) {
                if (gpu.toLower().contains(keyword)) {
                    QFont font = button->font();
                    font.setBold(true);
                    button->setFont(font);
                }
            }
        }
        buttons->addWidget(button);
    }
    layout->addLayout(buttons);

    auto* steps = new QLabel(
        tr("<h3>How to update your graphics driver</h3>"
           "<ol>"
           "<li><b>Find your graphics card.</b> Press <i>Win + R</i>, type <i>dxdiag</i> and press Enter, then open the <i>Display</i> "
           "tab. Or open <i>Device Manager</i> and expand <i>Display adapters</i>.</li>"
           "<li><b>Laptop or ready-made PC?</b> Check the maker's support page for your exact model first (Dell, HP, Lenovo, ASUS, "
           "Acer...). Their drivers are tested for your machine. Otherwise use the buttons above:"
           "<ul>"
           "<li><b>Intel:</b> run the <i>Intel Driver &amp; Support Assistant</i>, it finds the right driver for you.</li>"
           "<li><b>AMD:</b> pick your product on the AMD page, or run the AMD auto-detect tool.</li>"
           "<li><b>NVIDIA:</b> pick your product on the NVIDIA page, or use the NVIDIA app.</li>"
           "</ul></li>"
           "<li><b>Install it and restart your PC.</b> Choose a clean or standard install, both are fine.</li>"
           "<li><b>Check it worked.</b> In <i>dxdiag</i> the Display tab should show your real card with a driver version, not "
           "\"Microsoft Basic Display Adapter\".</li>"
           "</ol>"
           "<h3>Still not working?</h3>"
           "<p>Very old graphics (for example Intel HD Graphics 2000/3000 and older, or cards from before about 2012) cannot run recent "
           "Minecraft versions even with the latest driver. In that case:</p>"
           "<ul>"
           "<li>Play an older Minecraft version (1.16.5 or older work on much older hardware), or</li>"
           "<li>turn on <i>Use software rendering</i> in the instance's settings under Performance. It works on almost any PC but is "
           "slow, or</li>"
           "<li>upgrade the graphics card.</li>"
           "</ul>"),
        this);
    steps->setWordWrap(true);
    steps->setTextFormat(Qt::RichText);
    steps->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(steps);
    layout->addWidget(scroll, 1);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(box);
}

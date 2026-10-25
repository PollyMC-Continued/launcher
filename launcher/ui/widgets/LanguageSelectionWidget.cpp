#include "LanguageSelectionWidget.h"

#include <QCheckBox>
#include <QHeaderView>
#include <QLabel>
#include <QTreeView>
#include <QVBoxLayout>
#include "Application.h"
#include "settings/SettingsObject.h"
#include "BuildConfig.h"
#include "settings/Setting.h"
#include "translations/TranslationsModel.h"

LanguageSelectionWidget::LanguageSelectionWidget(QWidget* parent) : QWidget(parent)
{
    verticalLayout = new QVBoxLayout(this);
    verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
    languageView = new QTreeView(this);
    languageView->setObjectName(QStringLiteral("languageView"));
    languageView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    languageView->setAlternatingRowColors(true);
    languageView->setRootIsDecorated(false);
    languageView->setItemsExpandable(false);
    languageView->setWordWrap(true);
    languageView->header()->setCascadingSectionResizes(true);
    languageView->header()->setStretchLastSection(false);
    verticalLayout->addWidget(languageView);
    loadingLabel = new QLabel(this);
    loadingLabel->setObjectName(QStringLiteral("loadingLabel"));
    loadingLabel->setWordWrap(true);
    loadingLabel->setStyleSheet(QStringLiteral("color: gray;"));
    verticalLayout->addWidget(loadingLabel);
    helpUsLabel = new QLabel(this);
    helpUsLabel->setObjectName(QStringLiteral("helpUsLabel"));
    helpUsLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    helpUsLabel->setOpenExternalLinks(true);
    helpUsLabel->setWordWrap(true);
    verticalLayout->addWidget(helpUsLabel);

    formatCheckbox = new QCheckBox(this);
    formatCheckbox->setObjectName(QStringLiteral("formatCheckbox"));
    formatCheckbox->setCheckState(APPLICATION->settings()->get("UseSystemLocale").toBool() ? Qt::Checked : Qt::Unchecked);
    connect(formatCheckbox, &QCheckBox::stateChanged,
            [this]() { APPLICATION->translations()->setUseSystemLocale(formatCheckbox->isChecked()); });
    verticalLayout->addWidget(formatCheckbox);

    auto translations = APPLICATION->translations();
    auto index = translations->selectedIndex();
    languageView->setModel(translations);
    languageView->setCurrentIndex(index);
    languageView->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    languageView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    connect(languageView->selectionModel(), &QItemSelectionModel::currentRowChanged, this, &LanguageSelectionWidget::languageRowChanged);
    // First run: the language list fills in as soon as the index download lands. Until then,
    // the widget is usable in English - say so instead of looking silently broken.
    connect(translations, &QAbstractItemModel::rowsInserted, this, &LanguageSelectionWidget::updateLoadingHint);
    connect(translations, &QAbstractItemModel::rowsRemoved, this, &LanguageSelectionWidget::updateLoadingHint);
    updateLoadingHint();
    verticalLayout->setContentsMargins(0, 0, 0, 0);

    auto language_setting = APPLICATION->settings()->getSetting("Language");
    connect(language_setting.get(), &Setting::SettingChanged, this, &LanguageSelectionWidget::languageSettingChanged);
}

QString LanguageSelectionWidget::getSelectedLanguageKey() const
{
    auto translations = APPLICATION->translations();
    return translations->data(languageView->currentIndex(), Qt::UserRole).toString();
}

void LanguageSelectionWidget::retranslate()
{
    QString text = tr("Don't see your language or the quality is poor?<br/><a href=\"%1\">Help us with translations!</a>")
                       .arg(BuildConfig.TRANSLATIONS_URL);
    helpUsLabel->setText(text);
    formatCheckbox->setText(tr("Use system locales"));
    updateLoadingHint();
}

void LanguageSelectionWidget::updateLoadingHint()
{
    const bool downloading = APPLICATION->translations()->rowCount() <= 1;
    loadingLabel->setVisible(downloading);
    if (downloading) {
        loadingLabel->setText(tr("Downloading the list of languages… You can continue in English now and "
                                 "switch languages at any time - the list fills in by itself."));
    } else {
        loadingLabel->clear();
    }
}

void LanguageSelectionWidget::languageRowChanged(const QModelIndex& current, const QModelIndex& previous)
{
    if (current == previous) {
        return;
    }
    auto translations = APPLICATION->translations();
    QString key = translations->data(current, Qt::UserRole).toString();
    translations->selectLanguage(key);
    translations->updateLanguage(key);
}

void LanguageSelectionWidget::languageSettingChanged(const Setting&, const QVariant&)
{
    auto translations = APPLICATION->translations();
    auto index = translations->selectedIndex();
    languageView->setCurrentIndex(index);
}

#include "ScrollMessageBox.h"
#include <QPushButton>
#include "ui/GuiUtil.h"
#include "ui_ScrollMessageBox.h"

ScrollMessageBox::ScrollMessageBox(QWidget* parent, const QString& title, const QString& text, const QString& body, const QString& option)
    : QDialog(parent), ui(new Ui::ScrollMessageBox)
{
    ui->setupUi(this);
    this->setWindowTitle(title);
    ui->label->setText(text);
    ui->textBrowser->setText(body);

    if (!option.isEmpty()) {
        ui->optionCheckBox->setVisible(true);
        ui->optionCheckBox->setText(option);
    }

    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("OK"));

    // remember the window size between runs, like the settings window does
    GuiUtil::rememberGeometry(this, "ScrollMessageBoxGeometry");
}

ScrollMessageBox::~ScrollMessageBox()
{
    delete ui;
}

bool ScrollMessageBox::isOptionChecked() const
{
    return ui->optionCheckBox->isChecked();
}

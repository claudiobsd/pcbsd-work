#include <QLineEdit>
#include "dialogname.h"
#include "ui_dialogname.h"

DialogName::DialogName(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogName)
{
    ui->setupUi(this);
    isValid=false;
}

DialogName::~DialogName()
{
    delete ui;
}



void DialogName::setForbiddenList(QStringList& a)
{
    forbidden=a;
    on_nameEdit_textChanged(ui->nameEdit->text());
}

void DialogName::setTitle(QString t)
{
    this->setWindowTitle(t);
}

void DialogName::setName(QString t)
{
    ui->nameEdit->setText(t);
}

QString DialogName::getName()
{
    return ui->nameEdit->text();
}


void DialogName::done(int result)
{
    if(result==QDialog::Accepted && !isValid) return;
        QDialog::done(result);
}

void DialogName::on_nameEdit_textChanged(const QString &arg1)
{
    if(arg1.isEmpty() || forbidden.contains(arg1)) {
        ui->nameEdit->setStyleSheet("background-color: rgb(255, 188, 183);");
        isValid=false;
    }
    else {
        ui->nameEdit->setStyleSheet("");
        isValid=true;
    }
}

#ifndef DIALOGPROP_H
#define DIALOGPROP_H

#include <QDialog>
#include "zmanagerwindow.h"
namespace Ui {
class DialogProp;
}

#define PROP_READONLY 1
#define PROP_ISNUMBER 2
#define PROP_ISOPTION  4
#define PROP_ISPATH   8
#define PROP_ISSTRING 16

struct zproperty {
    QString Name;
    QString Description;
    QStringList ValidOptions;
    int Flags;
};


class DialogProp : public QDialog
{
    Q_OBJECT
    QList<zproperty> AllProperties;

public:

    QString getPoolProperty(zpool_t *pool,QString Property);
    void InitAllProperties();
    void refreshList(zpool_t *pool);

    explicit DialogProp(QWidget *parent = 0);
    ~DialogProp();
    
private:
    Ui::DialogProp *ui;
};

#endif // DIALOGPROP_H

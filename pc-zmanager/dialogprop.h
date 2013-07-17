#ifndef DIALOGPROP_H
#define DIALOGPROP_H

#include <QDialog>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include "zmanagerwindow.h"
namespace Ui {
class DialogProp;
}

#define PROP_READONLY 1
#define PROP_ISNUMBER 2
#define PROP_ISOPTION  4
#define PROP_ISPATH   8
#define PROP_ISSTRING 16
#define PROP_CHANGED 32

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
    QStringList getAllChangedProps();   // RETURN A LIST OF ALL PROPERTIES THAT WERE CHANGED BY THE USER
    QStringList getAllChangedValues();  // RETURN A LIST WITH THE VALUES OF ALL PROPERTIES CHANGED BY THE USER

    explicit DialogProp(QWidget *parent = 0);
    ~DialogProp();
    
private slots:
    void on_propList_itemDoubleClicked(QTreeWidgetItem *item, int column);
    void on_propList_comboChanged(int index);
    void on_propList_itemChanged(QTreeWidgetItem *item, int column);

private:
    Ui::DialogProp *ui;
};

#endif // DIALOGPROP_H

#include "dialogprop.h"
#include "ui_dialogprop.h"
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QColor>

DialogProp::DialogProp(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogProp)
{
    ui->setupUi(this);
    ui->propList->setIconSize(QSize(32,32));
    InitAllProperties();

}

DialogProp::~DialogProp()
{
    delete ui;
}


void DialogProp::InitAllProperties()
{
    // INITIALIZATION CANNOT BE STATIC DUE TO CALLS TO tr() FOR PROPER TRANSLATION
    // IT MUST BE DONE AT RUNTIME AND AS PART OF A CLASS BASED IN QOBJECT
    zproperty tmp;

    // ADD ONE BY ONE ALL VALID PROPERTIES. THE FOLLOWING CORRESPOND TO ZFS VERSION 28

    tmp.Name="allocated";
    tmp.Description=tr("Amount of storage space within the pool that has been physically allocated.");
    tmp.Flags=PROP_READONLY;
    tmp.ValidOptions.clear();
    AllProperties.append(tmp);

    tmp.Name="capacity";
    tmp.Description=tr("Percentage of pool space used.");
    tmp.Flags=PROP_READONLY;
    tmp.ValidOptions.clear();
    AllProperties.append(tmp);

    tmp.Name="dedupratio";
    tmp.Description=tr("The deduplication ratio specified for a pool, expressed as a multiplier. For example, a value of 1.76 indicates that 1.76"
                       " units of data were stored but only 1 unit of disk space was actually consumed. See zfs(8) for a description of the "
                       "deduplication feature.");
    tmp.Flags=PROP_READONLY;
    tmp.ValidOptions.clear();
    AllProperties.append(tmp);



}

QString DialogProp::getPoolProperty(zpool_t *pool,QString Property)
{
    zprop_t tmp;

    foreach(tmp,pool->Properties) {
        if(tmp.Name==Property) return tmp.Value;
    }

    return "";

}

void DialogProp::refreshList(zpool_t *pool)
{

    zproperty pr;
    int index=0;

    foreach(pr,AllProperties)
    {
        QTreeWidgetItem *item=new QTreeWidgetItem(ui->propList);
        if(!item) return;
        item->setText(0,pr.Name);
        item->setToolTip(0,pr.Description);
        if(pr.Flags&PROP_ISOPTION) {
            // TODO: ADD A DROPDOWN BOX HERE

        }
        else{
        item->setText(1,getPoolProperty(pool,pr.Name));
        }
        if(pr.Flags&PROP_READONLY) item->setIcon(0,QIcon(":/icons/user-busy.png"));
        else item->setIcon(0,QIcon(":/icons/user-online.png"));


        ++index;

    }



}

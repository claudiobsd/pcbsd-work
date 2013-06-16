#include "dialogpartition.h"
#include "ui_dialogpartition.h"
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QGraphicsWidget>
#include <QGraphicsRectItem>
#include <QTransform>
#include <QPen>
#include <QBrush>


DialogPartition::DialogPartition(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogPartition)
{
    ui->setupUi(this);
    device=NULL;

    ui->devLayout->header()->setStretchLastSection(false);
    ui->devLayout->header()->setResizeMode(0,QHeaderView::Stretch);
    ui->devLayout->header()->setResizeMode(1,QHeaderView::ResizeToContents);
    ui->devLayout->header()->setResizeMode(2,QHeaderView::ResizeToContents);
    ui->devLayout->header()->setResizeMode(3,QHeaderView::ResizeToContents);
    ui->devLayout->header()->setResizeMode(4,QHeaderView::ResizeToContents);
    ui->devLayout->setIconSize(QSize(22,22));
    ui->devLayout->setSelectionMode(QAbstractItemView::SingleSelection);

    ui->GraphicPart->setScene(new QGraphicsScene());

}

DialogPartition::~DialogPartition()
{
    delete ui;
}


void DialogPartition::setDevice(vdev_t * _device)
{
    QString tmp;
    device=_device;

    ui->devName->setText(device->Name);
    ui->devTotalSize->setText(printBytes(device->Size));


    /* This is to overcome an implementation limitation in Qt */
    /* The slider widget can only count up to 2^32 in a 32 bit system */
    /* So we will use coarser granularity to (larger sectors) to make it work */
    granularity=0;
    unsigned long long temp=device->SectorsCount;
    while(temp > 1<<31) {
        granularity++;
        temp>>=1;
    }

    // SHOW ONLY PARTITIONS THAT ARE AVAILABLE DEPENDING ON TYPE OF TABLE

    ui->PartType->clear();
    if(device->PartType=="MBR") {
        // TODO: ADD ALL PARTITION TYPES FROM GPART SOURCE CODE
        ui->PartType->addItem("freebsd",QVariant(1));
        ui->PartType->setCurrentIndex(0);
    }
    if(device->PartType=="GPT") {
        // TODO: ADD ALL PARTITION TYPES FROM GPART SOURCE CODE
        ui->PartType->addItem("freebsd-ufs",QVariant(1));
        ui->PartType->addItem("freebsd-zfs",QVariant(1));
        ui->PartType->addItem("freebsd-boot",QVariant(1));
        ui->PartType->addItem("freebsd-swap",QVariant(1));
        ui->PartType->setCurrentIndex(0);
    }
    if(device->PartType=="BSD") {
        // BSD PARTITION ONLY SUPPORTS FreeBSD types
        ui->PartType->addItem("freebsd-ufs",QVariant(1));
        ui->PartType->addItem("freebsd-zfs",QVariant(1));
        ui->PartType->addItem("freebsd-swap",QVariant(1));
        ui->PartType->setCurrentIndex(0);
    }



    QList<vdev_t>::const_iterator idx=device->Partitions.constBegin();
    unsigned long long sector=((device->Level==0)? device->SectorStart:0),maxsectorcount=0;
    int maxspaceidx=-1,numidx=0;
    QTreeWidgetItem *maxspaceitem=NULL;

    int coloridx=0;
    double scalex=(double)1000.0/(double)device->SectorsCount;



    ui->GraphicPart->scene()->clear();

    while(idx!=device->Partitions.constEnd())
    {
        if((*idx).SectorStart>sector) {
            QTreeWidgetItem *item=new QTreeWidgetItem(ui->devLayout);
            item->setIcon(0,QIcon(":/icons/task-complete.png"));
            item->setText(0,tr("** FREE **"));
            item->setText(1,tmp.sprintf("%llu",sector));
            item->setText(2,tmp.sprintf("%llu",(*idx).SectorStart-sector));
            item->setText(3,printBytes(((*idx).SectorStart-sector)*device->SectorSize));
            //item->setDisabled(false);
            item->setBackgroundColor(4,QColor::fromHsv(coloridx,0,192));

            ui->GraphicPart->scene()->addRect(sector*scalex,0,((*idx).SectorStart-sector)*scalex,100,QPen(QColor::fromHsv(coloridx,0,192)),QBrush(QColor::fromHsv(coloridx,0,192)));
            coloridx+=30;

            if((*idx).SectorStart-sector>maxsectorcount) { maxspaceitem=item; maxsectorcount=(*idx).SectorStart-sector; maxspaceidx=numidx; }
            sector=(*idx).SectorStart;
        }

        QTreeWidgetItem *item=new QTreeWidgetItem(ui->devLayout);
        item->setIcon(0,QIcon(":/icons/kdf.png"));
        item->setText(0,(*idx).Name + " [" + (*idx).PartType + "]");
        item->setText(1,tmp.sprintf("%llu",(*idx).SectorStart));
        item->setText(2,tmp.sprintf("%llu",(*idx).SectorsCount));
        item->setText(3,printBytes(((*idx).SectorsCount)*device->SectorSize));
        //item->setDisabled(true);
        item->setBackgroundColor(4,QColor::fromHsv(coloridx,128,255));
        sector=(*idx).SectorStart+(*idx).SectorsCount;

        ui->GraphicPart->scene()->addRect((*idx).SectorStart*scalex,0,(*idx).SectorsCount*scalex,100,QPen(QColor::fromHsv(coloridx,128,255)),QBrush(QColor::fromHsv(coloridx,128,255)));
        coloridx+=30;

        ++idx;
        ++numidx;



    }




    if( ((device->Level==0)? device->SectorStart:0)+device->SectorsCount>sector) {
        QTreeWidgetItem *item=new QTreeWidgetItem(ui->devLayout);
        item->setIcon(0,QIcon(":/icons/task-complete.png"));
        item->setText(0,tr("** FREE **"));
        item->setText(1,tmp.sprintf("%llu",sector));
        item->setText(2,tmp.sprintf("%llu",((device->Level==0)? device->SectorStart:0)+device->SectorsCount-sector));
        item->setText(3,printBytes((((device->Level==0)? device->SectorStart:0)+device->SectorsCount-sector)*device->SectorSize));
        //item->setDisabled(false);
        item->setBackgroundColor(4,QColor::fromHsv(coloridx,0,192));
        if(((device->Level==0)? device->SectorStart:0)+device->SectorsCount-sector>maxsectorcount) { maxspaceitem=item; maxsectorcount=((device->Level==0)? device->SectorStart:0)+device->SectorsCount-sector; maxspaceidx=numidx; }

        ui->GraphicPart->scene()->addRect(sector*scalex,0,(((device->Level==0)? device->SectorStart:0)+device->SectorsCount-sector)*scalex,100,QPen(QColor::fromHsv(coloridx,0,192)),QBrush(QColor::fromHsv(coloridx,0,192)));
        coloridx+=30;


    }


    if(!maxspaceitem) {
        // THIS DISK DOESN'T HAVE ANY FREE SPACE!
        ui->devLargestFree->setText(tr("No free space!"));
        ui->SizeSelect->setDisabled(true);
        ui->sizeSlider->setRange(0,0);
        ui->sizeSlider->setDisabled(true);
        ui->SizeText->setDisabled(true);
        ui->SizeText->clear();
        ui->PartType->clear();
        ui->PartType->setEnabled(false);
    }
    else {

    ui->devLayout->setCurrentItem(maxspaceitem);

    ui->devLargestFree->setText(printBytes(maxsectorcount*device->SectorSize));
    }


}


// PRINT SIZE IN BYTES, KBYTES , GBYTES OR TBYTES
QString DialogPartition::printBytes(unsigned long long bytes,int unit)
{
    QString a;

    if(unit>4) unit=-1;

    // UNIT == -1 FOR AUTOMATIC SELECTION OF UNITS
    if(unit==-1) {
    if(bytes<2*1024) return a.sprintf("%llu bytes",bytes);
    if(bytes<2*1024*1024) return a.sprintf("%.2lf kB",((double)bytes)/1024);
    if(bytes<1*1024*1024*1024) return a.sprintf("%.2lf MB",((double)bytes)/(1024*1024));
    if(bytes<(1LL*1024LL*1024LL*1024LL*1024LL)) return a.sprintf("%.2lf GB",((double)bytes)/(1024*1024*1024));
    return a.sprintf("%.2lf TB",((double)bytes)/((double)1024.0*1024.0*1024.0*1024.0));
    }

    switch(unit)
    {
    case 1:
        return a.sprintf("%.2lf",((double)bytes)/1024);
    case 2:
        return a.sprintf("%.2lf",((double)bytes)/(1024*1024));
    case 3:
        return a.sprintf("%.2lf",((double)bytes)/(1024*1024*1024));
    case 4:
        return a.sprintf("%.2lf",((double)bytes)/((double)1024.0*1024.0*1024.0*1024.0));
    default:
        return a.sprintf("%llu",bytes);

    }

}


// GET PREFERRED UNITS FOR PRINTING
int DialogPartition::printUnits(unsigned long long bytes)
{
    if(bytes<2*1024) return 0;
    if(bytes<2*1024*1024) return 1;
    if(bytes<1*1024*1024*1024) return 2;
    if(bytes<(1LL*1024LL*1024LL*1024LL*1024LL)) return 3;
    return 4;
}


void DialogPartition::on_SizeSelect_currentIndexChanged(int index)
{
    ui->SizeText->setText(printBytes(((unsigned long long)ui->sizeSlider->sliderPosition())*device->SectorSize,ui->SizeSelect->currentIndex()));
}

void DialogPartition::on_sizeSlider_valueChanged(int value)
{
    ui->SizeText->setText(printBytes(( ((unsigned long long)ui->sizeSlider->sliderPosition())<<granularity)*device->SectorSize,ui->SizeSelect->currentIndex()));

}


void DialogPartition::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);

    QSize sz=ui->GraphicPart->size();
    QTransform matrix;

    matrix.scale((qreal)(sz.width()-10)/1000.0,(qreal)(sz.height()-10)/100.0);
    ui->GraphicPart->setTransform(matrix);
}


void DialogPartition::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);

    QSize sz=ui->GraphicPart->size();
    QTransform matrix;

    matrix.scale((qreal)(sz.width()-10)/1000.0,(qreal)(sz.height()-10)/100.0);
    ui->GraphicPart->setTransform(matrix);

}

long long DialogPartition::getStartSector()
{
    if(ui->devLayout->currentItem()) {
        if(ui->devLayout->currentItem()->text(0)==tr("** FREE **")) {
        long long sector=ui->devLayout->currentItem()->text(1).toLongLong();
        return sector;
        }

    }
    return -1;
}

unsigned long long DialogPartition::getSectorCount()
{
    return ((unsigned long long)ui->sizeSlider->sliderPosition())<<granularity;
}

QString DialogPartition::getPartType()
{
    return ui->PartType->currentText();
}


void DialogPartition::on_devLayout_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    if(current->text(0)!=tr("** FREE **")) {
        ui->PartType->setEnabled(false);
        ui->SizeSelect->setEnabled(false);
        ui->SizeText->clear();
        ui->SizeText->setEnabled(false);
        ui->sizeSlider->setRange(0,0);
        ui->sizeSlider->setEnabled(false);
        return;
    }

    ui->PartType->setEnabled(true);
    ui->SizeSelect->setEnabled(true);
    ui->SizeSelect->setEnabled(true);
    ui->SizeSelect->setCurrentIndex(printUnits(current->text(2).toULongLong()*device->SectorSize));
    ui->sizeSlider->setRange(0,current->text(2).toULongLong()>>granularity);
    ui->sizeSlider->setSliderPosition(current->text(2).toULongLong()>>granularity);
    ui->SizeText->setText(printBytes(current->text(2).toULongLong()*device->SectorSize,ui->SizeSelect->currentIndex()));
    ui->SizeText->setEnabled(true);
    return;

}

void DialogPartition::on_PartType_currentIndexChanged(int index)
{
    if((ui->PartType->currentText()=="freebsd-ufs")
      || (ui->PartType->currentText()=="freebsd-boot") ) ui->newfsCheck->setEnabled(true);
    else { ui->newfsCheck->setEnabled(false); ui->newfsCheck->setChecked(false); }

}


bool DialogPartition::isnewfsChecked()
{
 return ui->newfsCheck->isChecked();
}

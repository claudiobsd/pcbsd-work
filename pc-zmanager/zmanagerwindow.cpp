#include <QTimer>
#include "zmanagerwindow.h"
#include "ui_zmanagerwindow.h"
#include "dialogpartition.h"
#include "dialogmount.h"
#include <pcbsd-utils.h>
#include <QDebug>
#include <QIcon>
#include <QString>
#include <QStringList>
#include <QMenu>
#include <QAction>
#include <QMessageBox>

ZManagerWindow::ZManagerWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ZManagerWindow)
{
    ui->setupUi(this);

    ui->zpoolList->setIconSize(QSize(48,48));
    ui->zpoolList->header()->setStretchLastSection(false);
    ui->zpoolList->header()->setResizeMode(0,QHeaderView::Stretch);
    ui->zpoolList->header()->setResizeMode(1,QHeaderView::ResizeToContents);

    ui->deviceList->setIconSize(QSize(48,48));
    ui->deviceList->header()->setStretchLastSection(false);
    ui->deviceList->header()->setResizeMode(0,QHeaderView::Stretch);
    ui->deviceList->header()->setResizeMode(1,QHeaderView::ResizeToContents);

    connect(ui->zpoolList,SIGNAL(customContextMenuRequested(QPoint)),SLOT(zpoolContextMenu(QPoint)));
    connect(ui->deviceList,SIGNAL(customContextMenuRequested(QPoint)),SLOT(deviceContextMenu(QPoint)));

}

ZManagerWindow::~ZManagerWindow()
{
    delete ui;
}


void ZManagerWindow::ProgramInit()
{
    // Perform initialization
    QTimer::singleShot(300,this,SLOT(refreshState()));
}

void ZManagerWindow::slotSingleInstance()
{
    activateWindow();
}


void ZManagerWindow::GetCurrentTopology()
{

    // RUN ALL REQUIRED PROCESSES AND GET THE RESULTS

    QStringList a=pcbsd::Utils::runShellCommand("zpool status");
    QStringList g=pcbsd::Utils::runShellCommand("geom disk list");
    QStringList h=pcbsd::Utils::runShellCommand("gpart list");
    QStringList h2=pcbsd::Utils::runShellCommand("gpart show -p");
    QStringList lbl=pcbsd::Utils::runShellCommand("glabel status");
    QStringList m=pcbsd::Utils::runShellCommand("mount");


    // CLEAR ALL EXISTING TOPOLOGY
    this->Pools.clear();
    this->Disks.clear();

    QStringList::const_iterator idx;
    int state;



    // BEGIN PROCESSING DISKS FROM GEOM

    idx=g.constBegin();
    vdev_t dsk,part;
    state=0;

    while(idx!=g.constEnd()) {

        QString str=*idx;


        if(str.startsWith("Geom name: ")) {

            if(state) {
                this->Disks.append(dsk);
                state=0;
            }
            dsk.Name=str.remove(0,11);
            dsk.SectorSize=0;
            dsk.SectorStart=0;
            dsk.SectorsCount=0;
            dsk.Description.clear();
            dsk.Level=0;
            dsk.Status=0;
            dsk.InPool.clear();
            dsk.Index=0;
            dsk.Size=0LL;
            dsk.PartType.clear();
            dsk.Partitions.clear();
            ++state;
        }

        if(str.startsWith("   Mediasize: ") && (state==1)) {

            QStringList l=str.remove(0,14).split(" ");
            QStringList::const_iterator lit=l.constBegin();
            while( (*lit).isEmpty() && lit!=l.constEnd()) ++lit;
            if(lit!=l.constEnd()) dsk.Size=(*lit).toULongLong();
            ++state;
        }

        if(str.startsWith("   Sectorsize: ") && (state==2)) {

            dsk.SectorSize=str.remove(0,15).toInt();
            ++state;
        }


        if(str.startsWith("   descr: ") && (state==3)) {

            dsk.Description=str.remove(0,10);
            ++state;
        }


        ++idx;
    }

    if(state==4) {
        this->Disks.append(dsk);
    }




    // BEGIN PROCESSING PARTITIONS FROM GPART

    idx=h.constBegin();
    state=0;
    QString ParentGeom;

    while(idx!=h.constEnd()) {

        QString str=*idx;


        if(str.startsWith("Geom name: ")) {

            if(state) {
                // APPEND SLICES AND PARTITIONS
                QList<vdev_t>::iterator dskidx=this->Disks.begin();

                while(dskidx!=this->Disks.end()) {
                    if((*dskidx).Name==dsk.Name) {
                        // ADD SLICES
                        (*dskidx).PartType=dsk.PartType;
                        (*dskidx).Partitions=dsk.Partitions;

                        break;
                    }
                    if(dsk.Name.startsWith((*dskidx).Name)) {
                        // THE GEOM IS ACTUALLY A SLICE
                        QList<vdev_t>::iterator partidx=(*dskidx).Partitions.begin();

                        while(partidx!=(*dskidx).Partitions.end()) {
                            if((*partidx).Name==dsk.Name) {
                                // ADD PARTITIONS
                                (*partidx).PartType=dsk.PartType;
                                (*partidx).Partitions=dsk.Partitions;

                                break;
                            }

                            ++partidx;

                        }
                    }

                    ++dskidx;

                }
                if(dskidx==this->Disks.constEnd()) {
                    // THIS SHOULDN'T HAPPEN, DISK HAD TO BE IN THE GEOM LIST!!!
                    // SIMPLY DISCARD


                }

                state=0;
            }


            // DETERMINE IF THE GEOM IS A DISK OR A SLICE
            QList<vdev_t>::iterator dskidx=this->Disks.begin();



            dsk.Name=str.remove(0,11);
            dsk.Level=0;
            while(dskidx!=this->Disks.end()) {
                if((*dskidx).Name==dsk.Name) {
                    // GEOM IS A DISK
                    break;
                }
                if(dsk.Name.startsWith((*dskidx).Name)) {
                    // GEOM IS A SLICE
                    dsk.Level=1;
                }
                ++dskidx;
            }

            dsk.Description.clear();
            dsk.PartType.clear();
            dsk.Partitions.clear();
            dsk.SectorSize=0;
            dsk.SectorStart=0;
            dsk.SectorsCount=0;
            dsk.Index=0;
            dsk.Status=0;
            dsk.Size=0LL;
            ++state;
        }

        if(str.startsWith("scheme: ") && (state==1)) {

            dsk.PartType=str.remove(0,8);
            ++state;
        }


        if(str.startsWith("Providers:") &&(state==2)) {
            ++state;
        }

        if(str.contains(". Name: ") && (state>=3)) {
            if(state>3) {
                // ADD PREVIOUS PARTITION TO MAIN GEOM
                // WE MUST INSERT IN THE LIST, SORTED BY SECTOR START
               QList<vdev_t>::iterator partit=dsk.Partitions.begin();

                while(partit!=dsk.Partitions.end())
                {
                    if( (*partit).SectorStart>part.SectorStart) break;
                    ++partit;
                }

                if(partit==dsk.Partitions.end()) dsk.Partitions.append(part);
                else dsk.Partitions.insert(partit,part);
                state=3;
            }
            part.Name=str.remove(0,9);
            part.Description.clear();
            part.Partitions.clear();
            part.PartType.clear();
            part.Level=dsk.Level+1;
            part.SectorSize=0;
            part.SectorStart=0;
            part.SectorsCount=0;
            part.Status=0;
            part.InPool.clear();
            part.Index=0;
            part.Size=0LL;
            ++state;
        }

        if(str.startsWith("   Mediasize: ") &&(state==4)) {
            QStringList l=str.remove(0,14).split(" ");
            QStringList::const_iterator lit=l.constBegin();
            while( (*lit).isEmpty() && lit!=l.constEnd()) ++lit;
            if(lit!=l.constEnd()) part.Size=(*lit).toULongLong();
            ++state;
        }

        if(str.startsWith("   Sectorsize: ") && (state==5)) {

            part.SectorSize=str.remove(0,15).toInt();
            ++state;
        }


        if(str.startsWith("   type: ") &&(state==6)) {
            part.PartType=str.remove(0,9);
            ++state;
        }

        if(str.startsWith("   index: ") && (state==7)) {

            part.Index=str.remove(0,10).toInt();
            ++state;
        }

        if(str.startsWith("   end: ") &&(state==8)) {
            part.SectorsCount=str.remove(0,8).toULongLong();
            ++state;
        }

        if(str.startsWith("   start: ") &&(state==9)) {
            part.SectorStart=str.remove(0,10).toULongLong();
            part.SectorsCount-=(part.SectorStart-1);
            ++state;
        }

        if(str.startsWith("Consumers:") &&(state==10)) {
            // WE MUST INSERT IN THE LIST, SORTED BY SECTOR START
           QList<vdev_t>::iterator partit=dsk.Partitions.begin();

            while(partit!=dsk.Partitions.end())
            {
                if( (*partit).SectorStart>part.SectorStart) break;
                ++partit;
            }

            if(partit==dsk.Partitions.end()) dsk.Partitions.append(part);
            else dsk.Partitions.insert(partit,part);
            state=-1;
        }


        ++idx;
    }

    if(state==3 || state==-1) {
            // APPEND SLICES AND PARTITIONS
            QList<vdev_t>::iterator dskidx=this->Disks.begin();

            while(dskidx!=this->Disks.end()) {
                if((*dskidx).Name==dsk.Name) {
                    // ADD SLICES
                    (*dskidx).PartType=dsk.PartType;
                    (*dskidx).Partitions=dsk.Partitions;

                    break;
                }
                if(dsk.Name.startsWith((*dskidx).Name)) {
                    // THE GEOM IS ACTUALLY A SLICE
                    QList<vdev_t>::iterator partidx=(*dskidx).Partitions.begin();
                    while(partidx!=(*dskidx).Partitions.end()) {
                        if((*partidx).Name==dsk.Name) {
                            // ADD PARTITIONS
                            (*partidx).PartType=dsk.PartType;
                            (*partidx).Partitions=dsk.Partitions;

                            break;
                        }

                        ++partidx;

                    }
                }

                ++dskidx;

            }
            if(dskidx==this->Disks.constEnd()) {
                // THIS SHOULDN'T HAPPEN, DISK HAD TO BE IN THE GEOM LIST!!!
                // SIMPLY DISCARD


            }

            state=0;
        }

    // GPART SHOW IS ONLY NEEDED BECAUSE GPART LIST
    // DOESN'T SHOW IF A DISK HAS A PARTITION TABLE
    // BUT NO PARTITIONS IN IT
    // IN ANY CASE, WE READ THE SECTORS AGAIN FOR EACH PARTITION

    vdev_t *exist_disk;

    bool mainitem;
    idx=h2.constBegin();

    while(idx!=h2.constEnd()) {
        QStringList tokens=(*idx).split(" ", QString::SkipEmptyParts);

        if(tokens.count()>=4)
        {

            if(tokens.at(0)=="=>") { mainitem=true; tokens.removeAt(0); }
            else mainitem=false;

            exist_disk=getDiskbyName(tokens.at(2));
                if(exist_disk!=NULL) {
                    if(!mainitem || !exist_disk->SectorStart) exist_disk->SectorStart=tokens.at(0).toULongLong();
                    exist_disk->SectorsCount=tokens.at(1).toULongLong();
                    exist_disk->PartType=tokens.at(3);
                }

        }


        ++idx;
    }








    // GET LABELS FROM GLABEL

    idx=lbl.constBegin();

    while(idx!=lbl.constEnd()) {
        QStringList tokens=(*idx).split(" ", QString::SkipEmptyParts);

        QList<vdev_t>::iterator dskit=this->Disks.begin();

        while(dskit!=this->Disks.end())
        {
            QList<vdev_t>::iterator sliceit=(*dskit).Partitions.begin();
            while(sliceit!=(*dskit).Partitions.end()) {
                QList<vdev_t>::Iterator partit=(*sliceit).Partitions.begin();
                while(partit!=(*sliceit).Partitions.end()) {
                    if(tokens.at(2)==(*partit).Name) { (*partit).Alias=tokens.at(0); break; }
                    ++partit;
                }
                    if(partit!=(*sliceit).Partitions.end()) break;
                    if(tokens.at(2)==(*sliceit).Name) { (*sliceit).Alias=tokens.at(0); break; }
                    ++sliceit;
            }

            if(sliceit!=(*dskit).Partitions.end()) break;

            if(tokens.at(2)==(*dskit).Name) { (*dskit).Alias=tokens.at(0); break; }

            ++dskit;
        }

        ++idx;
    }

    // GET MOUNT LOCATIONS FROM MOUNT

    idx=m.constBegin();

    while(idx!=m.constEnd()) {
        QStringList tokens=(*idx).split(" ",QString::SkipEmptyParts);
        if(tokens.count()<3) { ++ idx; continue; }
        // FIND DISK OR PARTITION WITH GIVEN NAME


        QList<vdev_t>::iterator dskit=this->Disks.begin();

        while(dskit!=this->Disks.end())
        {
            QList<vdev_t>::iterator sliceit=(*dskit).Partitions.begin();
            while(sliceit!=(*dskit).Partitions.end()) {
                QList<vdev_t>::Iterator partit=(*sliceit).Partitions.begin();
                while(partit!=(*sliceit).Partitions.end()) {
                    if(tokens.at(0)=="/dev/"+(*partit).Name) { (*partit).MountPoint=tokens.at(2); break; }
                    if(!(*partit).Alias.isEmpty()) {
                        if(tokens.at(0)=="/dev/"+(*partit).Alias) { (*partit).MountPoint=tokens.at(2); break; }
                    }
                    ++partit;
                }
                    if(partit!=(*sliceit).Partitions.end()) break;
                    if(tokens.at(0)=="/dev/"+(*sliceit).Name) { (*sliceit).MountPoint=tokens.at(2); break; }
                    if(!(*sliceit).Alias.isEmpty()) {
                        if(tokens.at(0)=="/dev/"+(*sliceit).Alias) { (*sliceit).MountPoint=tokens.at(2); break; }
                    }
                    ++sliceit;
            }

            if(sliceit!=(*dskit).Partitions.end()) break;

            if(tokens.at(0)=="/dev/"+(*dskit).Name) { (*dskit).MountPoint=tokens.at(2); break; }
            if(!(*dskit).Alias.isEmpty()) {
                if(tokens.at(0)=="/dev/"+(*dskit).Alias) { (*dskit).MountPoint=tokens.at(2); break; }
            }

            ++dskit;
        }




        ++idx;
    }


    // FINISHED PROCESSING DEVICES

    // PROCESS THE POOL LIST

    idx=a.constBegin();
    zpool_t pool;
    state=0;

    while(idx!=a.constEnd()) {

        QString str=*idx;

        if(str.startsWith("  pool: ")) {

            if(state) {
                // PREVIOUS POOL FINISHED, STORE
                this->Pools.append(pool);
                state=0;
            }
            pool.Name=str.remove(0,8);
            pool.Size=0;
            pool.Free=0;
            pool.Status=0;
            pool.VDevs.clear();
            ++state;
        }

        if(str.startsWith(" state: ")&& (state==1)) {
            QString tmp=*idx;
            tmp=tmp.remove(0,8);
            pool.Status=STATE_UNKNOWN;
            if(tmp=="ONLINE") pool.Status=STATE_ONLINE;
            if(tmp=="OFFLINE") pool.Status=STATE_OFFLINE;
            if(tmp=="DEGRADED") pool.Status=STATE_DEGRADED;
            if(tmp=="FAULTED") pool.Status=STATE_FAULTED;
            if(tmp=="REMOVED") pool.Status=STATE_REMOVED;
            if(tmp=="UNAVAIL") pool.Status=STATE_UNAVAIL;
            ++state;
        }

        if(str.startsWith(QString("\t")+pool.Name) && (state==2)) {
        // START DETAILED LIST OF THE POOL
            ++state;
        }
        if(str.startsWith("\t  ")&& (state==3)) {
            QString tmp=*idx;
            int level=0;
            tmp=tmp.remove(0,3);
            if(tmp.startsWith("  ")) {
                // THIS IS A DISK INSIDE A MIRROR/RAID, ETC.
                ++level;
            }
            QStringList tokens=tmp.split(" ",QString::SkipEmptyParts);

            QStringList::const_iterator it=tokens.constBegin();
            vdev_t vdev;

            if(tokens.count()>0)
            {
                QString tok=*it;

                vdev.Name=tok;
                vdev.Level=level;
                ++it;

                QString tmp=*it;

                vdev.Status=STATE_UNKNOWN;

                if(tmp=="ONLINE") vdev.Status=STATE_ONLINE;
                if(tmp=="OFFLINE") vdev.Status=STATE_OFFLINE;
                if(tmp=="DEGRADED") vdev.Status=STATE_DEGRADED;
                if(tmp=="FAULTED") vdev.Status=STATE_FAULTED;
                if(tmp=="REMOVED") vdev.Status=STATE_REMOVED;
                if(tmp=="UNAVAIL") vdev.Status=STATE_UNAVAIL;

                if(vdev.Status==STATE_UNKNOWN) vdev.Name.clear();

                // CHECK IF VDEV IS A MIRROR OR RAID ARRAY

                if(vdev.Name.startsWith("mirror")) vdev.PartType="mirror";
                if(vdev.Name.startsWith("raidz3")) vdev.PartType="raidz3";
                if(vdev.Name.startsWith("raidz2")) vdev.PartType="raidz2";
                if(vdev.Name.startsWith("raidz-")) vdev.PartType="raidz";

                vdev.Partitions.clear();


            }
            if(!vdev.Name.isEmpty()) {
                vdev.InPool=pool.Name;
                if(vdev.Level) pool.VDevs.last().Partitions.append(vdev);
                else pool.VDevs.append(vdev);

                vdev_t *dsk=getDiskbyName(vdev.Name);
                if(dsk!=NULL) dsk->InPool = pool.Name;
                if(dsk!=NULL && level==0) {
                 // THE POOL IS STRIPED ONLY
                    pool.Type="striped";
                }
                if(pool.Type.isEmpty() && !vdev.PartType.isEmpty()) pool.Type=vdev.PartType;

            }
        }

        if(str.startsWith("errors: ")&& (state==3)) ++state;


        idx++;
    }
    if(state) {
        // PREVIOUS POOL FINISHED, STORE
        this->Pools.append(pool);
        state=0;
    }











}

void ZManagerWindow::refreshState()
{
    GetCurrentTopology();


    ui->zpoolList->clear();
    ui->deviceList->clear();

    // ADD ALL POOLS

    if(Pools.count()!=0) {

    QList<zpool_t>::iterator it=Pools.begin();

    while(it!=Pools.end()) {
    QTreeWidgetItem *item=new QTreeWidgetItem(ui->zpoolList);
    item->setText(0,(*it).Name);
    item->setIcon(0,QIcon(":/icons/server-database.png"));
    item->setText(1,getStatusString((*it).Status));
    if((*it).Status==STATE_ONLINE) item->setIcon(1,QIcon(":/icons/task-complete.png"));
    else if( ((*it).Status==STATE_FAULTED)|| ((*it).Status==STATE_UNAVAIL)|| ((*it).Status==STATE_REMOVED)) item->setIcon(1,QIcon(":/icons/task-reject.png"));
        else item->setIcon(1,QIcon(":/icons/task-attention.png"));


    QList<vdev_t>::const_iterator devit=(*it).VDevs.constBegin();

    while(devit!=(*it).VDevs.constEnd())
    {
        QTreeWidgetItem *subitem=new QTreeWidgetItem(item);

        // TODO: ADD MORE DESCRIPTION HERE FROM THE GEOM INFO OF THE DEVICE
        subitem->setText(0,(*devit).Name);
        // TODO: USE APPROPRIATE ICONS FOR DISKS, SLICES, FILES, ETC.
        subitem->setIcon(0,QIcon(":/icons/drive-harddisk.png"));
        subitem->setText(1,getStatusString((*devit).Status));
        if((*devit).Status==STATE_ONLINE) subitem->setIcon(1,QIcon(":/icons/task-complete.png"));
        else if( ((*devit).Status==STATE_FAULTED)|| ((*devit).Status==STATE_UNAVAIL)|| ((*devit).Status==STATE_REMOVED)) subitem->setIcon(1,QIcon(":/icons/task-reject.png"));
            else subitem->setIcon(1,QIcon(":/icons/task-attention.png"));
        ++devit;
    }

    ++it;
    }

    }

    else {
        QTreeWidgetItem *item=new QTreeWidgetItem(ui->zpoolList);
        item->setText(0,tr("No pools available, right click to create a new one..."));
   }



    // ADD ALL DISK DEVICES


    QList<vdev_t>::const_iterator idx=Disks.constBegin();


    while(idx!=Disks.constEnd()) {
        QTreeWidgetItem *item=new QTreeWidgetItem(ui->deviceList);

        QString sz;
        sz.sprintf(" (%.1lf GB)",((double)(*idx).Size)/((double)(1024.0*1024.0*1024.0)));
        if(!(*idx).PartType.isEmpty()) { sz+=" [" + (*idx).PartType + "]"; }
        sz+="\n";
        item->setText(0,(*idx).Name + sz + (*idx).Description);
        item->setIcon(0,QIcon(":/icons/drive-harddisk.png"));
        if((*idx).MountPoint.isEmpty()) {
            // IS NOT MOUNTED, CHECK IF IT HAS ANY PARTITIONS
            if((*idx).Partitions.count()==0) {
                // NO PARTITIONS, IT'S EITHER UNUSED OR PART OF A POOL (DEDICATED)
                if((*idx).InPool.isEmpty())  item->setText(1,tr("Avaliable"));
                else item->setText(1,tr("ZPool: ")+(*idx).InPool);
            } else item->setText(1,tr("Sliced")); }
        else item->setText(1,tr("Mounted: ")+(*idx).MountPoint);

        // ADD SLICES HERE AS SUBITEMS
        if((*idx).Partitions.count()>0) {
            QList<vdev_t>::const_iterator partidx=(*idx).Partitions.constBegin();


            while(partidx!=(*idx).Partitions.constEnd()) {

            QTreeWidgetItem *subitem=new QTreeWidgetItem(item);

            QString sz;

            sz.sprintf(" (%.1lf GB)\n",((double)(*partidx).Size)/((double)(1024.0*1024.0*1024.0)));
            subitem->setText(0,(*partidx).Name + sz + (*partidx).PartType);
            subitem->setIcon(0,QIcon(":/icons/partitionmanager.png"));
            if((*partidx).MountPoint.isEmpty()) { if((*partidx).Partitions.count()==0) { if((*partidx).PartType.isEmpty() || ((*partidx).PartType=="BSD")) subitem->setText(1,tr("Available")); else subitem->setText(1,tr("Unmounted"));} else subitem->setText(1,tr("Partitioned")); }
            else subitem->setText(1,tr("Mounted: ")+(*partidx).MountPoint);

            if((*partidx).Partitions.count()>0) {
                QList<vdev_t>::const_iterator part2idx=(*partidx).Partitions.constBegin();


                while(part2idx!=(*partidx).Partitions.constEnd()) {

                QTreeWidgetItem *subsubitem=new QTreeWidgetItem(subitem);

                QString sz;

                sz.sprintf(" (%.1lf GB)\n",((double)(*part2idx).Size)/((double)(1024.0*1024.0*1024.0)));
                subsubitem->setText(0,(*part2idx).Name + sz + (*part2idx).PartType);
                subsubitem->setIcon(0,QIcon(":/icons/kdf.png"));
                if((*part2idx).MountPoint.isEmpty()) { if( (*part2idx).PartType.isEmpty()) subsubitem->setText(1,tr("Available")); else subsubitem->setText(1,tr("Unmounted")); }
                else subsubitem->setText(1,tr("Mounted: ")+(*part2idx).MountPoint);
                ++part2idx;
                }


            }




            ++partidx;
            }


        }

    ++idx;
    }

ui->zpoolList->expandAll();
ui->deviceList->expandAll();

}


bool ZManagerWindow::close()
{
    return QMainWindow::close();
}


const QString ZManagerWindow::getStatusString(int status)
{
    switch(status)
    {
    case STATE_OFFLINE:
        return tr("Offline");
    case STATE_ONLINE:
        return tr("Online");
    case STATE_DEGRADED:
        return tr("Degraded");
    case STATE_FAULTED:
        return tr("Faulted");
    case STATE_REMOVED:
        return tr("Removed");
    case STATE_UNAVAIL:
        return tr("Unavailable");
    default:
    case STATE_UNKNOWN:
        return tr("Unknown");

    }
}





void ZManagerWindow::zpoolContextMenu(QPoint p)
{
    zpool_t *pool;
    QMenu m(tr("zpool Menu"),this);

    QTreeWidgetItem *item=ui->zpoolList->itemAt(p);

    if(Pools.count()>0) {
        // THIS IS A POOL OR A DEVICE
        QString name=item->text(0);

        qDebug() << name;

        // CHECK IF NAME IS A POOL OR A DEVICE
        pool=getZpoolbyName(name);

        if(!pool) {
            // THIS IS A DEVICE
            // TODO: ADD CONTEXT MENU FOR DEVICES THAT ARE PART OF A POOL

            m.addAction(tr("Useless test action"))->setData(QVariant(0));

        }
        else {
        // SHOW CONTEXT MENU FOR A POOL

            m.addAction(tr("Create new pool"))->setData(QVariant(1));
            m.addAction(tr("Destroy pool"))->setData(QVariant(2));

        // TODO: ADD MORE MENU ACTIONS HERE



        }

    }
    else {

        m.addAction(tr("Create new pool"))->setData(QVariant(1));

    }


    QAction *res=m.exec(ui->zpoolList->viewport()->mapToGlobal(p));

    if(res!=NULL) {
        int selected=res->data().toInt();
        bool result;
        switch(selected)
        {
        case 1:
            result=zpoolCreate();
            break;
        case 2:
            result=zpoolDestroy(pool);
            break;
        default:
            result=false;
            break;
        }


        if(result) this->refreshState();


    }




}

void ZManagerWindow::deviceContextMenu(QPoint p)
{
    QMenu m(tr("Device Menu"),this);
    QTreeWidgetItem *item=ui->deviceList->itemAt(p);

    vdev_t *dev=getDiskbyName(item->text(0).split(" ",QString::SkipEmptyParts).at(0));

    if(dev==NULL) return;

    if(!dev->InPool.isEmpty()) {
        // DISK IS IN A POOL, NOTHING TO DO UNTIL IT'S REMOVED
        return;
    }

    if(!dev->MountPoint.isEmpty()) m.addAction(tr("Unmount"))->setData(QVariant(1));  // OFFER TO UNMOUNT IF PARTITION IS MOUNTED
    else {
        if( (!dev->PartType.isEmpty()) && (dev->Partitions.count()==0) && (dev->PartType!="MBR") && (dev->PartType!="GPT") && (dev->PartType!="BSD") && (dev->PartType!="freebsd") )
        {
            // THIS IS A PARTITION WITH A FILE SYSTEM BUT NOT MOUNTED
            m.addAction(tr("Mount"))->setData(QVariant(2));
        }

        if(dev->PartType.isEmpty() && dev->Level<2) {
            // THIS IS NOT PARTITIONED, OFFER TO CREATE A PARTITION TABLE
            m.addAction(tr("Create MBR partition table"))->setData(QVariant(3));
            m.addAction(tr("Create GPT partition table"))->setData(QVariant(4));
        }
        if(dev->PartType=="freebsd" && dev->Level<2) {
            // THIS IS NOT PARTITIONED, OFFER TO CREATE A PARTITION TABLE
            m.addAction(tr("Create BSD partition table"))->setData(QVariant(5));
        }

        else if( (dev->Level<2) && ((dev->PartType=="MBR") || (dev->PartType=="GPT") || (dev->PartType=="BSD"))) {
            // THIS DISK HAS PARTITIONS, OFFER TO ADD A NEW ONE
            if(dev->Partitions.count()==0) m.addAction(tr("Delete Partition Table"))->setData(QVariant(6));
            if(dev->Level==0) m.addAction(tr("Add new slice"))->setData(QVariant(7));
            else m.addAction(tr("Add new partition"))->setData(QVariant(7));
        }

        if( (dev->Partitions.count()==0) && (dev->Level>0)) {
            // OFFER TO DELETE THE PARTITION
            if(dev->Level==1) m.addAction(tr("Destroy this slice"))->setData(QVariant(8));
            else m.addAction(tr("Destroy this partition"))->setData(QVariant(8));

        }


    }



    QAction *result=m.exec(ui->deviceList->viewport()->mapToGlobal(p));

    if(result!=NULL) {
        int selected=result->data().toInt();
        bool result;
        switch(selected)
        {
        case 1:
            result=deviceUnmount(dev);
            break;
        case 2:
            result=deviceMount(dev);
            break;
        case 3:
        case 4:
        case 5:
            result=deviceCreatePartitionTable(dev,selected-3);
            break;
        case 6:
            result=deviceDestroyPartitionTable(dev);
            break;
        case 7:
            result=deviceAddPartition(dev);
            break;
        case 8:
            result=deviceDestroyPartition(dev);
            break;
        default:
            result=false;
            break;
        }


        if(result) this->refreshState();


    }


}

void ZManagerWindow::filesystemContextMenu(QPoint p)
{
}


vdev_t *ZManagerWindow::getDiskbyName(QString name)
{
    QList<vdev_t>::const_iterator it=this->Disks.constBegin();


    while(it!=this->Disks.constEnd())
    {
        if((*it).Name==name) return (vdev_t *)&(*it);
        if((*it).Partitions.count()!=0) {
            // PROCESS ALL SLICES
            QList<vdev_t>::const_iterator sliceit=(*it).Partitions.constBegin();

            while(sliceit!=(*it).Partitions.constEnd())
            {
                if((*sliceit).Name==name) return (vdev_t *)&(*sliceit);
                if((*it).Partitions.count()!=0) {
                    // PROCESS ALL BSD LABEL PARTITIONS
                    QList<vdev_t>::const_iterator partit=(*sliceit).Partitions.constBegin();

                    while(partit!=(*sliceit).Partitions.constEnd())
                    {
                        if((*partit).Name==name) return (vdev_t *)&(*partit);

                    ++partit;
                    }

                }

            ++sliceit;
            }

        }

        ++it;
    }

    return NULL;

}


vdev_t *ZManagerWindow::getContainerDisk(vdev_t* device)
{

    QList<vdev_t>::const_iterator it=this->Disks.constBegin();


    while(it!=this->Disks.constEnd())
    {
        if((*it).Partitions.count()!=0) {
            // PROCESS ALL SLICES



            if((*it).Partitions.contains(*device)) return (vdev_t *)&(*it);

            QList<vdev_t>::const_iterator sliceit=(*it).Partitions.constBegin();

            while(sliceit!=(*it).Partitions.constEnd())
            {

                if((*sliceit).Partitions.contains(*device)) return (vdev_t *)&(*sliceit);

            ++sliceit;
            }

        }

        ++it;
    }

    return NULL;



}







zpool_t *ZManagerWindow::getZpoolbyName(QString name)
{
QList<zpool_t>::const_iterator it=this->Pools.constBegin();

while(it!=this->Pools.constEnd())
{
    if((*it).Name==name) return (zpool_t *)&(*it);
    ++it;
}

return NULL;

}




bool ZManagerWindow::deviceCreatePartitionTable(vdev_t *device,int type)
{
    QString cmd;
    if(type==0) cmd="gpart create -s mbr ";
    if(type==1) cmd="gpart create -s gpt ";
    if(type==2) cmd="gpart create -s bsd ";
    cmd+=device->Name;

    QStringList a=pcbsd::Utils::runShellCommand(cmd);

    if(processErrors(a,"gpart")) return false;

    return true;
}

bool ZManagerWindow::deviceDestroyPartitionTable(vdev_t *device)
{
    QString cmd;
    cmd="gpart destroy ";
    cmd+=device->Name;

    QStringList a=pcbsd::Utils::runShellCommand(cmd);

    if(processErrors(a,"gpart")) return false;

    return true;
}
bool ZManagerWindow::deviceUnmount(vdev_t * device)
{
    QString cmdline="umount ";

    if(device->Alias.isEmpty())  cmdline += "/dev/"+device->Name;
    else cmdline+="/dev/"+device->Alias;

    QStringList a=pcbsd::Utils::runShellCommand(cmdline);


    if(processErrors(a,"umount")) return false;


    return true;
}
bool ZManagerWindow::deviceMount(vdev_t * device)
{

DialogMount mnt;

mnt.setDevice(device);

int result=mnt.exec();

if(result) {

    QString cmdline="mount ";

    if(device->Alias.isEmpty())  cmdline += "/dev/"+device->Name;
    else cmdline+="/dev/"+device->Alias;

    cmdline += " " + mnt.getMountLocation();

    QStringList a=pcbsd::Utils::runShellCommand(cmdline);


    if(processErrors(a,"mount")) return false;


}

    return true;
}
bool ZManagerWindow::deviceAddPartition(vdev_t *device)
{
    DialogPartition part;

    part.setDevice(device);

    int result=part.exec();

    if(result) {

        long long startsector=part.getStartSector();
        unsigned long long sectorcount=part.getSectorCount();

        if(!sectorcount) return false;

        QString type=part.getPartType();

        QString cmdline,tmp;
        cmdline="gpart add";
        cmdline += " -t " + type;
        if(startsector>=0) { tmp.sprintf(" -b %lld",startsector); cmdline += tmp; }
        tmp.sprintf(" -s %llu",sectorcount);
        cmdline += tmp;

        cmdline += " " + device->Name;

        QStringList a=pcbsd::Utils::runShellCommand(cmdline);

        if(processErrors(a,"gpart")) return false;


        if(part.isnewfsChecked()) {
            // WE NEED TO INITIALIZE A FILE SYSTEM IN THE DEVICE

            // FOR NOW ONLY UFS FILE SYSTEM IS SUPPORTED, SO...

            // SINCE GPART SUCCEEDED, IT MUST'VE OUTPUT THE NAME OF THE NEW DEVICE

            QStringList tmp=a.at(0).split(" ",QString::SkipEmptyParts);

            cmdline="newfs /dev/"+ tmp.at(0);

            QStringList b=pcbsd::Utils::runShellCommand(cmdline);

            if(processErrors(b,"newfs")) return false;

        }


        return true;

    }

    return false;
}
bool ZManagerWindow::deviceDestroyPartition(vdev_t* device)
{
   QString cmdline;
   QString tmp;
   cmdline="gpart delete ";
   tmp.sprintf(" -i %d",device->Index);
   cmdline+=tmp;
   cmdline += " " + getContainerDisk(device)->Name;

   QStringList a=pcbsd::Utils::runShellCommand(cmdline);

   if(processErrors(a,"gpart")) return false;

   return true;
}


bool ZManagerWindow::processErrors(QStringList& output,QString command)
{
    QStringList::const_iterator it=output.constBegin();
    QString start;
    QString errormsg;
    bool errorfound=false;

    start=command+":";

    while(it!=output.constEnd())
    {

        if((*it).startsWith(command)) {
            QString tmp=(*it);
            tmp.remove(0,command.length()+2);
            errormsg+= tmp + "\n";
            errorfound=true;
        }
        ++it;
    }

    if(errorfound) {
        QString msg(tr("An error was detected while executing '%1':\n\n"));
        msg=msg.arg(command);
        msg+=errormsg;
        QMessageBox err(QMessageBox::Warning,tr("Error report"),msg,QMessageBox::Ok,this);

        err.exec();

        return true;

    }

    return false;
}




bool ZManagerWindow::zpoolCreate()
{

    return false;
}

bool ZManagerWindow::zpoolDestroy(zpool_t *pool)
{
    return false;
}

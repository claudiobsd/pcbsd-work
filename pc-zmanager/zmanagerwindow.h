#ifndef ZMANAGERWINDOW_H
#define ZMANAGERWINDOW_H

#include <QMainWindow>

namespace Ui {
class ZManagerWindow;
}

enum vdevStatus {
    STATE_UNKNOWN=0,
    STATE_ONLINE,
    STATE_OFFLINE,
    STATE_DEGRADED,
    STATE_FAULTED,
    STATE_REMOVED,
    STATE_UNAVAIL
};

struct __vdev_t;
typedef struct __vdev_t vdev_t;

struct __vdev_t {
    QString Name;               // DEVICE NAME
    QString Alias;              // GLABEL ALIAS
    QString MountPoint;         // POINT WHERE THE DEVICE IS MOUNTED (NON-ZFS)
    QString Description;        // DEVICE DESCRIPTION
    int Level;
    int Index;
    int Status;
    QString PartType;           // TYPE OF PARTITIONING SYSTEM OR PARTITION TYPE
    QString InPool;
    int SectorSize;
    unsigned long long SectorStart;
    unsigned long long SectorsCount;
    unsigned long long Size;    // SIZE OF THE DEVICE

    QList<vdev_t> Partitions;   // LIST OF ALL SUB-PARTITIONS
    bool operator==(vdev_t b) { return (Name==b.Name); };   // REQUIRED BY LIST FUNCTIONS

};





typedef struct {
    QString Name;
    QString Type;
    int Status;
    QList<vdev_t> VDevs;
    unsigned long long Size;
    unsigned long long Free;
} zpool_t;


class ZManagerWindow : public QMainWindow
{
    Q_OBJECT

private:
    Ui::ZManagerWindow *ui;

public:

    QList<vdev_t> Disks;
    QList<zpool_t> Pools;

    const QString getStatusString(int status);

    vdev_t *getDiskbyName(QString name);
    vdev_t *getContainerDisk(vdev_t *device);
    zpool_t *getZpoolbyName(QString name);

    void ProgramInit();
    void GetCurrentTopology();
    explicit ZManagerWindow(QWidget *parent = 0);
    ~ZManagerWindow();
    
    // Disk management functions
    bool deviceCreatePartitionTable(vdev_t *device, int type);
    bool deviceDestroyPartitionTable(vdev_t *device);
    bool deviceUnmount(vdev_t * device);
    bool deviceMount(vdev_t * device);
    bool deviceAddPartition(vdev_t *device);
    bool deviceDestroyPartition(vdev_t* device);

    // Pool management functions
    bool zpoolCreate();
    bool zpoolDestroy(zpool_t *pool);




    bool processErrors(QStringList& output,QString command);

public slots:
    void slotSingleInstance();
    void refreshState();
    bool close();
    void zpoolContextMenu(QPoint);
    void deviceContextMenu(QPoint);
    void filesystemContextMenu(QPoint);


};

#endif // ZMANAGERWINDOW_H

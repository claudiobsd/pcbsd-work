#ifndef ZMANAGERWINDOW_H
#define ZMANAGERWINDOW_H

#include <QMainWindow>

namespace Ui {
class ZManagerWindow;
}

enum vdevStatus {
    STATE_UNKNOWN=0,
    STATE_NOTAPPLICABLE,
    STATE_ONLINE,
    STATE_OFFLINE,
    STATE_DEGRADED,
    STATE_FAULTED,
    STATE_REMOVED,
    STATE_UNAVAIL,
    STATE_AVAIL,
    STATE_EXPORTED=0x100,
    STATE_DESTROYED=0x200
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
    QString Value;
}  zprop_t;

typedef struct {
    QString Name;
    QString Type;
    int Status;
    QList<vdev_t> VDevs;
    QList<zprop_t> Properties;
    unsigned long long Size;
    unsigned long long Free;
} zpool_t;


typedef struct {
    QString PoolName;
    QString Error;
} zerror_t;


#define ITEM_NONE            0
#define ITEM_ISMIRROR        1
#define ITEM_ISRAIDZ         2
#define ITEM_ISLOG           4
#define ITEM_ISCACHE         8
#define ITEM_ISSPARE        16
#define ITEM_ISDISK         32
#define ITEM_ISPOOL         64
#define ITEM_ISOFFLINE     256
#define ITEM_ISFAULTED     512
#define ITEM_ISDEGRADED   1024
#define ITEM_ISREMOVED    2048
#define ITEM_ISUNAVAIL    4096
#define ITEM_ISEXPORTED   8192
#define ITEM_ISDESTROYED 16384


#define ITEM_TYPE       0xff
#define ITEM_STATE      0xff00
#define ITEM_ALL      0xffff
#define PARENT(a) ((a)<<16)









class ZManagerWindow : public QMainWindow
{
    Q_OBJECT

private:
    Ui::ZManagerWindow *ui;

public:

    QList<vdev_t> Disks;
    QList<zpool_t> Pools;
    QList<zerror_t> Errors;

    zpool_t *lastSelectedPool;
    vdev_t *lastSelectedVdev;
    bool needRefresh;



    const QString getStatusString(int status);

    vdev_t *getDiskbyName(QString name);
    vdev_t *getContainerDisk(vdev_t *device);

    zpool_t *getZpoolbyName(QString name, int index=-1);
    vdev_t *getVDevbyName(QString name);
    vdev_t *getContainerGroup(vdev_t* device);
    QString getPoolProperty(zpool_t *pool,QString Property);
    void    setPoolProperty(zpool_t *pool,QString Property,QString Value);

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

    // Pool auxiliary functions
    int zpoolGetVDEVType(vdev_t *);


    bool processErrors(QStringList& output,QString command);
    bool processzpoolErrors(QStringList& output);

public slots:
    void slotSingleInstance();
    void refreshState();
    bool close();
    void zpoolContextMenu(QPoint);
    void deviceContextMenu(QPoint);
    void filesystemContextMenu(QPoint);
    // Pool management functions
    void zpoolCreate(bool b);
    void zpoolRename(bool b);
    void zpoolDestroy(bool b);
    void zpoolClear(bool b);
    void zpoolExport(bool b);
    void zpoolImport(bool b);
    void zpoolEditProperties(bool b);
    void zpoolRemoveDevice(bool b);
    void zpoolAttachDevice(bool b);
    void zpoolDetachDevice(bool b);
    void zpoolOfflineDevice(bool b);
    void zpoolOnlineDevice(bool b);
    void zpoolScrub(bool b);
    void zpoolAdd(bool b);
    void zpoolAddLog(bool b);
    void zpoolAddCache(bool b);
    void zpoolAddSpare(bool b);



private slots:
    void on_toolButton_clicked();
};


struct zactions {
    const QString menutext;
    int triggermask,triggerflags;
    const char *slot;
};

QString printBytes(unsigned long long bytes, int unit=-1);
int printUnits(unsigned long long bytes);



#endif // ZMANAGERWINDOW_H

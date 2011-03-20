
// event masks
#include <sys/inotify.h>

#include <QFileInfo>
#include <QFlags>
#include <QDebug>
#include <QDir>
#include <QMutexLocker>
#include <QStringList>
#include <QTimer>

#include "mirall/inotify.h"
#include "mirall/folderwatcher.h"
#include "mirall/fileutils.h"

static const uint32_t standard_event_mask =
    IN_CLOSE_WRITE | IN_ATTRIB | IN_MOVE | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF | IN_UNMOUNT | IN_ONLYDIR | IN_DONT_FOLLOW;

/* minimum amount of seconds between two
   events  to consider it a new event */
#define MIN_EVENT_INTERVAL_SEC 2

namespace Mirall {


FolderWatcher::FolderWatcher(const QString &root, QObject *parent)
    : QObject(parent),
      _root(root),
      _lastEventTime(QTime::currentTime())
{
    _inotify = new INotify(standard_event_mask);
    slotAddFolderRecursive(root);
    QObject::connect(_inotify, SIGNAL(notifyEvent(int, int, const QString &)),
                     SLOT(slotINotifyEvent(int, int, const QString &)));
}

FolderWatcher::~FolderWatcher()
{

}

QString FolderWatcher::root() const
{
    return _root;
}

QStringList FolderWatcher::folders() const
{
    return _inotify->directories();
}

void FolderWatcher::slotAddFolderRecursive(const QString &path)
{
    qDebug() << "`-> adding " << path;
    _inotify->addPath(path);
    QStringList watchedFolders(_inotify->directories());
    //qDebug() << "currently watching " << watchedFolders;
    QStringListIterator subfoldersIt(FileUtils::subFoldersList(path, FileUtils::SubFolderRecursive));
    while (subfoldersIt.hasNext()) {
        QDir folder (subfoldersIt.next());
        if (folder.exists() && !watchedFolders.contains(folder.path())) {
            qDebug() << "`-> adding " << folder.path();
            _inotify->addPath(folder.path());
        }
        else
            qDebug() << "`-> discarding " << folder.path();
    }
}

void FolderWatcher::slotINotifyEvent(int mask, int cookie, const QString &path)
{
    QMutexLocker locker(&_mutex);
    QTime eventTime = QTime::currentTime();

    if (IN_IGNORED & mask) {
        qDebug() << "IGNORE event";
        return;
    }

    if (IN_Q_OVERFLOW & mask)
        qDebug() << "OVERFLOW";

    if (mask & IN_CREATE) {
        qDebug() << cookie << " CREATE: " << path;
        if (QFileInfo(path).isDir()) {
            slotAddFolderRecursive(path);
        }
    }
    else if (mask & IN_DELETE) {
        qDebug() << cookie << " DELETE: " << path;
        if (_inotify->directories().contains(path));
            qDebug() << "`-> removing " << path;
            _inotify->removePath(path);
    }
    else if (mask & IN_CLOSE_WRITE) {
        qDebug() << cookie << " WRITABLE CLOSED: " << path;
    }
    else if (mask & IN_MOVE) {
        qDebug() << cookie << " MOVE: " << path;
    }
    else {
        qDebug() << cookie << " OTHER " << mask << " :" << path;
    }

    if (_lastEventTime.secsTo(eventTime) < MIN_EVENT_INTERVAL_SEC) {
        qDebug() << "Last event happened less than 2 seconds ago...";
        return;
    }

    _lastEventTime = eventTime;

    emit folderChanged(path);
}

}

#include "folderwatcher.moc"

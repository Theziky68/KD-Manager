#ifndef NOTIFICATIONDATA_H
#define NOTIFICATIONDATA_H

#include <QString>
#include <QDateTime>
#include "NotificationManager.h"
#include "DocumentStatus.h"

// Структура для хранения уведомлений
struct NotificationItem {
    QString icon;
    QString title;
    QString text;
    QString filePath;
    QString folderPath;
    QDateTime time;
    DocumentStatus status;
    bool isRead;
    NotificationManager::NotificationType type;
    QString sourceUser;

    NotificationItem()
        : status(STATUS_IN_PROGRESS)
        , isRead(false)
        , type(NotificationManager::TYPE_INFO)
    {}
};

#endif // NOTIFICATIONDATA_H
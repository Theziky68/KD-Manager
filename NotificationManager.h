#ifndef NOTIFICATIONMANAGER_H
#define NOTIFICATIONMANAGER_H

#include <QObject>
#include <QMap>
#include <QList>
#include <QDateTime>
#include <QString>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QMutex>

#include "DocumentStatus.h"

class DatabaseManager;

class NotificationManager : public QObject
{
    Q_OBJECT

public:
    enum NotificationType {
        TYPE_INFO,
        TYPE_WARNING,
        TYPE_SUCCESS,
        TYPE_IMPORTANT,
        TYPE_CHANGE
    };

    struct Notification {
        NotificationType type;
        QString title;
        QString message;
        QString filePath;
        QString folderPath;
        QDateTime timestamp;
        bool isRead;
        bool isImportant;
        QString sourceUser;
        QList<ChangeRecord> changes;

        Notification()
            : type(TYPE_INFO)
            , isRead(false)
            , isImportant(false)
        {}
    };

    static NotificationManager& instance();

    void addNotification(const QString& user, const Notification& notif);
    void addNotification(const QString& user, const QString& title, const QString& message,
                         NotificationType type = TYPE_INFO);

    QList<Notification> getNotifications(const QString& user) const;
    QList<Notification> getUnreadNotifications(const QString& user) const;
    QList<Notification> getImportantNotifications(const QString& user) const;
    int getUnreadCount(const QString& user) const;
    int getTotalCount(const QString& user) const;

    void markAsRead(const QString& user, int index);
    void markAllAsRead(const QString& user);
    void clearNotifications(const QString& user);
    void removeNotification(const QString& user, int index);
    void clearAllNotifications();

    QMap<NotificationType, int> getTypeStats(const QString& user) const;

    void checkForChanges(const QString& currentUser = QString());

    // ===== BATCH-РЕЖИМ =====
    void beginBatch();
    void endBatch();
    bool isBatchMode() const;

    void setCheckInterval(int seconds);
    void setShowTrayNotifications(bool show) { m_showTrayNotifications = show; }
    void setMaxNotifications(int max) { m_maxNotifications = max; }
    void setSoundEnabled(bool enabled) { m_soundEnabled = enabled; }

signals:
    void newNotification(const QString& user, const Notification& notif);
    void notificationsUpdated(const QString& user);
    void changeDetected(const QString& filePath, const ChangeRecord& change);

private:
    NotificationManager();
    ~NotificationManager();

    void showTrayNotification(const Notification& notif);
    void saveNotifications();
    void loadNotifications();
    void addNotificationInternal(const QString& user, const Notification& notif);
    void trimNotifications(const QString& user);

    QMap<QString, QList<Notification>> m_notifications;
    QTimer* m_checkTimer;
    QSystemTrayIcon* m_trayIcon;
    bool m_showTrayNotifications;
    bool m_soundEnabled;
    QDateTime m_lastCheckTime;

    // ✅ ИСПРАВЛЕНО: добавлена потокозащита
    mutable QMutex m_notificationsMutex;
    mutable QMutex m_batchMutex;

    // ===== BATCH-РЕЖИМ =====
    bool m_batchMode;
    QList<Notification> m_batchNotifications;

    // ===== ЛИМИТ =====
    int m_maxNotifications;

    static const int MAX_NOTIFICATIONS = 200;  // <-- УМЕНЬШЕНО С 1000 ДО 200
};

#endif // NOTIFICATIONMANAGER_H
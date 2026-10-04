#ifndef ONLINEUSERSMANAGER_H
#define ONLINEUSERSMANAGER_H

#include <QString>
#include <QList>
#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <QMutex>

struct OnlineUser {
    QString fio;
    QString computerName;
    QDateTime lastSeen;
    bool isActive;

    OnlineUser(const QString& _fio = "", const QString& _computerName = "", const QDateTime& _lastSeen = QDateTime::currentDateTime())
        : fio(_fio), computerName(_computerName), lastSeen(_lastSeen), isActive(true) {}
};

class DatabaseManager;

class OnlineUsersManager : public QObject
{
    Q_OBJECT

public:
    static OnlineUsersManager& instance();

    // Добавить/обновить пользователя онлайн
    void setUserOnline(const QString& fio);
    void setUserOffline(const QString& fio);

    // Получить список онлайн пользователей
    QList<OnlineUser> getOnlineUsers() const;
    int getOnlineCount() const;

    // Проверить активность пользователя
    bool isUserOnline(const QString& fio) const;
    QDateTime getLastSeenTime(const QString& fio) const;

    // Очистить неактивных пользователей (не видели > 5 минут)
    void cleanupInactiveUsers();

signals:
    void userOnline(const QString& fio);
    void userOffline(const QString& fio);
    void onlineUsersChanged();

private:
    OnlineUsersManager();
    ~OnlineUsersManager();

    void loadOnlineUsersFromDB();
    QString getComputerName() const;

    QList<OnlineUser> m_onlineUsers;
    QTimer* m_cleanupTimer;
    QTimer* m_updateTimer;
    DatabaseManager* m_db;
    QString m_computerName;

    // ✅ ИСПРАВЛЕНО: добавлено хранение текущего ФИО и потокозащита
    QString m_currentUserFIO;
    mutable QMutex m_usersMutex;
};

#endif // ONLINEUSERSMANAGER_H


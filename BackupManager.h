// BackupManager.h

#ifndef BACKUPMANAGER_H
#define BACKUPMANAGER_H

#include <QObject>
#include <QTimer>
#include <QString>
#include <QDateTime>

class DatabaseManager;

class BackupManager : public QObject
{
    Q_OBJECT

public:
    static BackupManager& instance();

    void start();
    void stop();
    void setDatabaseManager(DatabaseManager* db);

    // ===== НАСТРОЙКИ =====
    void setBackupInterval(int hours) { m_backupInterval = hours; }
    void setRetentionDays(int days) { m_retentionDays = days; }
    void setWorkStartHour(int hour) { m_workStartHour = hour; }
    void setWorkEndHour(int hour) { m_workEndHour = hour; }

private slots:
    void onBackupTimer();
    void checkAndCleanup();

private:
    BackupManager();
    ~BackupManager();

    void createBackup();
    void cleanupOldBackups();
    QString getBackupDir() const;
    bool isWorkingTime() const;
    int getCurrentHour() const;
    int getMaxBackupsPerDay() const;  // <-- НОВЫЙ МЕТОД

    DatabaseManager* m_db;
    QTimer* m_backupTimer;
    QTimer* m_cleanupTimer;

    int m_backupInterval;      // Частота бэкапа (часы)
    int m_retentionDays;       // Хранение (дни)
    int m_workStartHour;       // Начало рабочего дня
    int m_workEndHour;         // Конец рабочего дня

    QString m_lastBackupFile;
    QString m_backupDir;
};

#endif // BACKUPMANAGER_H
#ifndef CHANGESPANEL_H
#define CHANGESPANEL_H

#include <QDockWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QCheckBox>
#include <QLineEdit>
#include <QFile>
#include <QDesktopServices>
#include <QMessageBox>
#include <QUrl>
#include <QDateTime>
#include <QFrame>
#include <QScrollArea>
#include <QTextEdit>
#include <QToolButton>
#include <QMenu>
#include <QDialog>

#include "NotificationManager.h"
#include "DocumentStatus.h"
#include "NotificationData.h"

class DatabaseManager;

class ChangesPanel : public QWidget
{
    Q_OBJECT

public:
    // ===== СИНГЛТОН =====
    static ChangesPanel& instance();
    static void initialize(DatabaseManager* db, QWidget* parent = nullptr);
    static void cleanup();

    // ===== ОСНОВНЫЕ МЕТОДЫ =====
    void loadChanges(const QString& folderPath = QString());
    void setDatabaseManager(DatabaseManager* db);

    void addNotificationItem(const NotificationItem& item);
    void clearAllNotifications();
    void refreshPanel();

    // ===== БАЗА ДАННЫХ =====
    void saveNotificationsToDatabase();
    void loadNotificationsFromDatabase();

    int getUnreadCount() const { return m_unreadCount; }

    typedef NotificationItem ChangeItem;

signals:
    void fileSelected(const QString& filePath);
    void folderSelected(const QString& folderPath);
    void statusChanged(const QString& filePath, DocumentStatus newStatus);
    void notificationAdded(const NotificationItem& item);
    void notificationsCleared();

private slots:
    void onItemClicked(QListWidgetItem* item);
    void onItemDoubleClicked(QListWidgetItem* item);
    void markAllAsRead();
    void clearAll();
    void refreshList();
    void filterChanged();
    void onNewNotification(const QString& user, const NotificationManager::Notification& notif);
    void showContextMenu(const QPoint& pos);
    void deleteSelectedNotification();
    void markSelectedAsRead();
    void showNotificationDetailsDialog();

private:
    // ===== КОНСТРУКТОР/ДЕСТРУКТОР (ПРИВАТНЫЕ) =====
    explicit ChangesPanel(QWidget* parent = nullptr);
    ~ChangesPanel();

    void setupUI();
    void updateNotificationCount();
    QString getStatusIcon(DocumentStatus status);
    QString getStatusColor(DocumentStatus status);
    void createNotificationWidget(QListWidgetItem* listItem, const ChangeItem& item);
    void showNotificationDetails(const ChangeItem& item);

    // ===== ОСНОВНЫЕ ВИДЖЕТЫ =====
    QListWidget* m_changesList;
    QPushButton* m_markReadButton;
    QPushButton* m_clearButton;
    QPushButton* m_refreshButton;
    QToolButton* m_menuButton;
    QLabel* m_notificationCountLabel;
    QComboBox* m_typeFilter;
    QComboBox* m_statusFilter;
    QLineEdit* m_searchFilter;
    QCheckBox* m_showOnlyUnread;
    QFrame* m_headerFrame;
    QFrame* m_footerFrame;

    // ===== ДАННЫЕ =====
    DatabaseManager* m_db;
    QString m_currentFolder;
    QList<ChangeItem> m_items;
    QList<QListWidgetItem*> m_itemWidgets;
    int m_unreadCount;
    bool m_isUpdating;

    // ===== СТАТИЧЕСКИЕ ЧЛЕНЫ =====
    static ChangesPanel* s_instance;
    static bool s_initialized;
    static QList<ChangeItem> s_globalNotifications;
};

#endif // CHANGESPANEL_H
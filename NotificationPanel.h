#ifndef NOTIFICATIONPANEL_H
#define NOTIFICATIONPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QPropertyAnimation>
#include <QTimer>
#include <QDateTime>
#include <QDialog>

class NotificationManager;

class NotificationDetailsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit NotificationDetailsDialog(const QString& title, const QString& message,
                                      const QString& type, const QDateTime& timestamp,
                                      QWidget* parent = nullptr);
};

class NotificationPanel : public QWidget
{
    Q_OBJECT

public:
    explicit NotificationPanel(QWidget* parent = nullptr);
    ~NotificationPanel();

    void addNotification(const QString& title, const QString& message, const QString& type = "info");
    void addNotification(const QString& title, const QString& message, const QString& type, const QString& sourceUser);
    void clearAll();
    void connectToManager(NotificationManager& manager);
    void showNotificationPanel() { showPanel(); }
    void hideNotificationPanel() { hidePanel(); }
    void updateTheme();
    void setShowInTrayOnly(bool trayOnly) { m_showInTrayOnly = trayOnly; }

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onClearAllClicked();
    void onDeleteAllClicked();
    void onNotificationItemClicked(QListWidgetItem* item);
    void onNotificationItemDoubleClicked(QListWidgetItem* item);
    void showPanel();
    void hidePanel();
    void autoHideNotification();
    void onManagerNotificationReceived(const QString& user);

private:
    void setupUI();
    void createNotificationItem(const QString& title, const QString& message,
                               const QString& type, const QDateTime& timestamp,
                               const QString& sourceUser = "");
    void updateBadge();
    QString getTypeColor(const QString& type);
    QString getTypeIcon(const QString& type);
    QString notificationTypeToString(int typeEnum);
    void applyTheme();

    // UI компоненты
    QWidget* m_panel;
    QListWidget* m_notificationList;
    QPushButton* m_clearAllButton;
    QPushButton* m_deleteAllButton;
    QLabel* m_emptyLabel;
    QPushButton* m_notificationButton;
    QLabel* m_badgeLabel;

    // Анимации
    QPropertyAnimation* m_slideAnimation;

    // Данные
    int m_unreadCount;
    bool m_isPanelVisible;
    bool m_showInTrayOnly;
    QTimer* m_autoHideTimer;
    QList<QPair<QString, QPair<QString, QDateTime>>> m_notificationsData;

    // Менеджер уведомлений
    NotificationManager* m_notificationManager;
};

#endif // NOTIFICATIONPANEL_H


#ifndef ONLINEUSERSPANEL_H
#define ONLINEUSERSPANEL_H

#include <QDockWidget>
#include <QListWidget>
#include <QLabel>
#include <QTimer>

class OnlineUsersManager;

class OnlineUsersPanel : public QDockWidget
{
    Q_OBJECT

public:
    explicit OnlineUsersPanel(QWidget *parent = nullptr);
    ~OnlineUsersPanel();

    void connectToManager(OnlineUsersManager& manager);

private slots:
    void updateUsersList();
    void onUserOnline(const QString& fio);
    void onUserOffline(const QString& fio);

private:
    void setupUI();
    void applyTheme();

    QListWidget* m_usersList;
    QLabel* m_countLabel;
    QTimer* m_updateTimer;
    OnlineUsersManager* m_manager;
};

#endif // ONLINEUSERSPANEL_H

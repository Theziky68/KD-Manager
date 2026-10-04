#ifndef USERCONFIG_H
#define USERCONFIG_H

#include <QString>
#include <QSettings>
#include <QDateTime>
#include <QRect>
#include <QByteArray>
#include <QMap>

class UserConfig
{
public:
    static UserConfig& instance();

    QString getUserFIO() const;
    void setUserFIO(const QString& fio);
    bool isFirstRun() const;

    // Настройки пользователя
    int getTheme() const;
    void setTheme(int theme);

    QDateTime getLastOnlineTime() const;
    void updateLastOnlineTime();

    // Получить путь к пользовательским настройкам
    QString getConfigPath() const;

    // ===== ГЕОМЕТРИЯ ОКНА =====
    QRect getWindowGeometry() const;
    void setWindowGeometry(const QRect& geometry);
    bool isWindowMaximized() const;
    void setWindowMaximized(bool maximized);
    QByteArray getSplitterState() const;
    void setSplitterState(const QByteArray& state);
    QMap<QString, bool> getExpandedFolders() const;
    void setExpandedFolders(const QMap<QString, bool>& expanded);

private:
    UserConfig();
    ~UserConfig();

    QSettings* m_settings;
    QString m_userFIO;
};

#endif // USERCONFIG_H

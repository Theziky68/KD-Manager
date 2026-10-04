#include "UserConfig.h"
#include <QStandardPaths>
#include <QDir>

UserConfig::UserConfig()
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);

    m_settings = new QSettings(appDataPath + "/config.ini", QSettings::IniFormat);
    m_userFIO = m_settings->value("user/fio", "").toString();
}

UserConfig::~UserConfig()
{
    if (m_settings) {
        m_settings->sync();
        delete m_settings;
    }
}

UserConfig& UserConfig::instance()
{
    static UserConfig instance;
    return instance;
}

QString UserConfig::getUserFIO() const
{
    return m_userFIO;
}

void UserConfig::setUserFIO(const QString& fio)
{
    m_userFIO = fio;
    m_settings->setValue("user/fio", fio);
    m_settings->setValue("user/firstRun", false);
    m_settings->sync();
}

bool UserConfig::isFirstRun() const
{
    return m_settings->value("user/firstRun", true).toBool();
}

int UserConfig::getTheme() const
{
    return m_settings->value("ui/theme", 0).toInt();
}

void UserConfig::setTheme(int theme)
{
    m_settings->setValue("ui/theme", theme);
    m_settings->sync();
}

QDateTime UserConfig::getLastOnlineTime() const
{
    return m_settings->value("user/lastOnline", QDateTime::currentDateTime()).toDateTime();
}

void UserConfig::updateLastOnlineTime()
{
    m_settings->setValue("user/lastOnline", QDateTime::currentDateTime());
    m_settings->sync();
}

QString UserConfig::getConfigPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

// ===== ГЕОМЕТРИЯ ОКНА =====
QRect UserConfig::getWindowGeometry() const
{
    return m_settings->value("window/geometry", QRect(100, 100, 1400, 800)).toRect();
}

void UserConfig::setWindowGeometry(const QRect& geometry)
{
    m_settings->setValue("window/geometry", geometry);
    m_settings->sync();
}

bool UserConfig::isWindowMaximized() const
{
    return m_settings->value("window/maximized", false).toBool();
}

void UserConfig::setWindowMaximized(bool maximized)
{
    m_settings->setValue("window/maximized", maximized);
    m_settings->sync();
}

QByteArray UserConfig::getSplitterState() const
{
    return m_settings->value("window/splitterState", QByteArray()).toByteArray();
}

void UserConfig::setSplitterState(const QByteArray& state)
{
    m_settings->setValue("window/splitterState", state);
    m_settings->sync();
}

QMap<QString, bool> UserConfig::getExpandedFolders() const
{
    QMap<QString, bool> result;
    m_settings->beginGroup("window/expandedFolders");
    for (const QString& key : m_settings->childKeys()) {
        result[key] = m_settings->value(key, false).toBool();
    }
    m_settings->endGroup();
    return result;
}

void UserConfig::setExpandedFolders(const QMap<QString, bool>& expanded)
{
    m_settings->beginGroup("window/expandedFolders");
    m_settings->remove("");  // Очистить группу
    m_settings->endGroup();

    m_settings->beginGroup("window/expandedFolders");
    for (auto it = expanded.begin(); it != expanded.end(); ++it) {
        m_settings->setValue(it.key(), it.value());
    }
    m_settings->endGroup();
    m_settings->sync();
}

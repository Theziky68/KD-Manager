#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QString>

class ThemeManager : public QObject
{
    Q_OBJECT

public:
    enum Theme {
        Light = 0,
        Dark = 1,
        System = 2
    };

    static ThemeManager& instance();

    void setTheme(Theme theme);
    Theme currentTheme() const { return m_currentTheme; }

    QString getStyleSheet() const;

    // Цвета для текущей темы
    QString backgroundColor() const;
    QString textColor() const;
    QString accentColor() const;
    QString borderColor() const;
    QString hoverColor() const;
    QString cardBackground() const;
    QString secondaryTextColor() const;
    QString primaryGradientStart() const;
    QString primaryGradientEnd() const;
    QString secondaryGradientStart() const;
    QString secondaryGradientEnd() const;
    QString separatorColor() const;
    QString successColor() const;
    QString warningColor() const;
    QString errorColor() const;

signals:
    void themeChanged(Theme theme);

private:
    ThemeManager();
    Theme m_currentTheme;

    QString getLightThemeStyleSheet() const;
    QString getDarkThemeStyleSheet() const;
};

#endif // THEMEMANAGER_H

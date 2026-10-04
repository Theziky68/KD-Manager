#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
    void onBrowse();
    void onCreateNew();
    void onOk();
    void onTestConnection();

private:
    void loadSettings();
    void saveSettings();

    QLineEdit* m_dbPathEdit;

    // Настройки сервера PostgreSQL
    QLineEdit* m_serverHost;
    QSpinBox* m_serverPort;
    QLineEdit* m_serverUser;
    QLineEdit* m_serverPassword;
    QLineEdit* m_serverDatabase;

    // Настройки уведомлений
    QCheckBox* m_showTrayNotif;
    QCheckBox* m_soundNotif;
    QSpinBox* m_intervalSpin;
    QSpinBox* m_limitSpin;

    // Настройки интерфейса
    QCheckBox* m_compactMode;
    QComboBox* m_themeCombo;
};

#endif // SETTINGSDIALOG_H
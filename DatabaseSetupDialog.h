#ifndef DATABASESETUPDIALOG_H
#define DATABASESETUPDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>

class DatabaseSetupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DatabaseSetupDialog(QWidget *parent = nullptr);

    // Получить введенные параметры
    QString getHostname() const;
    int getPort() const;
    QString getUsername() const;
    QString getPassword() const;
    QString getDatabaseName() const;
    QString getDatabaseType() const;  // "postgresql" или "sqlite"

    // Установить параметры (для загрузки из settings)
    void setHostname(const QString& host);
    void setPort(int port);
    void setUsername(const QString& user);
    void setPassword(const QString& pwd);
    void setDatabaseName(const QString& db);
    void setSqlitePath(const QString& path);
    QString getSqlitePath() const;

private slots:
    void onTypeChanged(const QString& type);
    void onTestConnection();
    void onOkClicked();

private:
    void setupUI();
    bool testPostgreSQLConnection();
    void showMessage(const QString& title, const QString& message, bool isError = false);

    // Тип БД
    QComboBox* m_typeCombo;

    // PostgreSQL параметры
    QLineEdit* m_hostEdit;
    QSpinBox* m_portSpinBox;
    QLineEdit* m_userEdit;
    QLineEdit* m_passwordEdit;
    QLineEdit* m_databaseEdit;

    // SQLite параметры
    QLineEdit* m_sqlitePathEdit;
    QPushButton* m_browseSqliteBtn;

    // Кнопки
    QPushButton* m_testBtn;
    QPushButton* m_okBtn;
    QPushButton* m_cancelBtn;

    // Статус
    QLabel* m_statusLabel;
};

#endif // DATABASESETUPDIALOG_H

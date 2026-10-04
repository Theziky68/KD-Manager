#include "DatabaseSetupDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QMessageBox>
#include <QFileDialog>
#include <QDebug>

DatabaseSetupDialog::DatabaseSetupDialog(QWidget *parent)
    : QDialog(parent)
    , m_typeCombo(nullptr)
    , m_hostEdit(nullptr)
    , m_portSpinBox(nullptr)
    , m_userEdit(nullptr)
    , m_passwordEdit(nullptr)
    , m_databaseEdit(nullptr)
    , m_sqlitePathEdit(nullptr)
    , m_browseSqliteBtn(nullptr)
    , m_testBtn(nullptr)
    , m_okBtn(nullptr)
    , m_cancelBtn(nullptr)
    , m_statusLabel(nullptr)
{
    setupUI();
    setWindowTitle("⚙️ Настройка базы данных");
    setModal(true);
    resize(500, 350);
}

void DatabaseSetupDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // ===== ВЫБОР ТИПА БД =====
    QGroupBox* typeGroup = new QGroupBox("Тип базы данных", this);
    QHBoxLayout* typeLayout = new QHBoxLayout(typeGroup);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem("PostgreSQL (для 50+ пользователей)", "postgresql");
    m_typeCombo->addItem("SQLite (локальная тестовая БД)", "sqlite");
    m_typeCombo->setCurrentIndex(0);

    typeLayout->addWidget(new QLabel("Выберите тип БД:"));
    typeLayout->addWidget(m_typeCombo);
    typeLayout->addStretch();

    connect(m_typeCombo, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            this, &DatabaseSetupDialog::onTypeChanged);

    mainLayout->addWidget(typeGroup);

    // ===== POSTGRESQL ПАРАМЕТРЫ =====
    QGroupBox* postgresGroup = new QGroupBox("Параметры PostgreSQL", this);
    QVBoxLayout* postgresLayout = new QVBoxLayout(postgresGroup);

    // Host
    QHBoxLayout* hostLayout = new QHBoxLayout();
    hostLayout->addWidget(new QLabel("Хост:"));
    m_hostEdit = new QLineEdit("localhost", this);
    hostLayout->addWidget(m_hostEdit);
    postgresLayout->addLayout(hostLayout);

    // Port
    QHBoxLayout* portLayout = new QHBoxLayout();
    portLayout->addWidget(new QLabel("Порт:"));
    m_portSpinBox = new QSpinBox(this);
    m_portSpinBox->setMinimum(1);
    m_portSpinBox->setMaximum(65535);
    m_portSpinBox->setValue(5432);
    portLayout->addWidget(m_portSpinBox);
    portLayout->addStretch();
    postgresLayout->addLayout(portLayout);

    // User
    QHBoxLayout* userLayout = new QHBoxLayout();
    userLayout->addWidget(new QLabel("Пользователь:"));
    m_userEdit = new QLineEdit("kd_user", this);
    userLayout->addWidget(m_userEdit);
    postgresLayout->addLayout(userLayout);

    // Password
    QHBoxLayout* passLayout = new QHBoxLayout();
    passLayout->addWidget(new QLabel("Пароль:"));
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    passLayout->addWidget(m_passwordEdit);
    postgresLayout->addLayout(passLayout);

    // Database
    QHBoxLayout* dbLayout = new QHBoxLayout();
    dbLayout->addWidget(new QLabel("База данных:"));
    m_databaseEdit = new QLineEdit("kd_documents", this);
    dbLayout->addWidget(m_databaseEdit);
    postgresLayout->addLayout(dbLayout);

    mainLayout->addWidget(postgresGroup);

    // ===== SQLITE ПАРАМЕТРЫ =====
    QGroupBox* sqliteGroup = new QGroupBox("Параметры SQLite", this);
    QVBoxLayout* sqliteLayout = new QVBoxLayout(sqliteGroup);

    QHBoxLayout* pathLayout = new QHBoxLayout();
    pathLayout->addWidget(new QLabel("Путь к файлу:"));
    m_sqlitePathEdit = new QLineEdit("N:/kd.db", this);
    pathLayout->addWidget(m_sqlitePathEdit);
    m_browseSqliteBtn = new QPushButton("...", this);
    m_browseSqliteBtn->setMaximumWidth(50);
    pathLayout->addWidget(m_browseSqliteBtn);
    sqliteLayout->addLayout(pathLayout);

    connect(m_browseSqliteBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getSaveFileName(this,
            "Выберите файл SQLite БД", "kd.db", "Database Files (*.db)");
        if (!path.isEmpty()) {
            m_sqlitePathEdit->setText(path);
        }
    });

    mainLayout->addWidget(sqliteGroup);

    // SQLite скрыт по умолчанию
    sqliteGroup->setVisible(false);

    // ===== СТАТУС И КНОПКИ =====
    m_statusLabel = new QLabel("", this);
    m_statusLabel->setStyleSheet("color: gray; font-size: 10px;");
    mainLayout->addWidget(m_statusLabel);

    QHBoxLayout* buttonsLayout = new QHBoxLayout();

    m_testBtn = new QPushButton("🔍 Проверить соединение", this);
    m_okBtn = new QPushButton("✅ OK", this);
    m_cancelBtn = new QPushButton("❌ Отмена", this);

    buttonsLayout->addWidget(m_testBtn);
    buttonsLayout->addStretch();
    buttonsLayout->addWidget(m_okBtn);
    buttonsLayout->addWidget(m_cancelBtn);

    connect(m_testBtn, &QPushButton::clicked, this, &DatabaseSetupDialog::onTestConnection);
    connect(m_okBtn, &QPushButton::clicked, this, &DatabaseSetupDialog::onOkClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    mainLayout->addLayout(buttonsLayout);

    setLayout(mainLayout);
}

void DatabaseSetupDialog::onTypeChanged(const QString& type)
{
    QWidget* parent = static_cast<QWidget*>(this);

    // Найти группы по имени
    QGroupBox* postgresGroup = nullptr;
    QGroupBox* sqliteGroup = nullptr;

    for (QObject* obj : children()) {
        QGroupBox* gb = qobject_cast<QGroupBox*>(obj);
        if (gb) {
            if (gb->title().contains("PostgreSQL")) {
                postgresGroup = gb;
            } else if (gb->title().contains("SQLite")) {
                sqliteGroup = gb;
            }
        }
    }

    if (m_typeCombo->currentData().toString() == "postgresql") {
        if (postgresGroup) postgresGroup->setVisible(true);
        if (sqliteGroup) sqliteGroup->setVisible(false);
        m_testBtn->setVisible(true);
    } else {
        if (postgresGroup) postgresGroup->setVisible(false);
        if (sqliteGroup) sqliteGroup->setVisible(true);
        m_testBtn->setVisible(false);
    }
}

void DatabaseSetupDialog::onTestConnection()
{
    if (testPostgreSQLConnection()) {
        showMessage("✅ Успешно", "Подключение к PostgreSQL успешно!", false);
        m_statusLabel->setText("✅ Соединение работает");
    } else {
        showMessage("❌ Ошибка", "Не удалось подключиться к PostgreSQL.\n\n"
            "Проверьте:\n"
            "1. IP адрес и порт\n"
            "2. Пользователя и пароль\n"
            "3. Имя БД\n"
            "4. Сервис PostgreSQL запущен\n"
            "5. Firewall открыт на порт 5432", true);
        m_statusLabel->setText("❌ Ошибка соединения");
    }
}

bool DatabaseSetupDialog::testPostgreSQLConnection()
{
    QSqlDatabase testDb = QSqlDatabase::addDatabase("QPSQL", "test_db_setup");
    testDb.setHostName(m_hostEdit->text());
    testDb.setPort(m_portSpinBox->value());
    testDb.setUserName(m_userEdit->text());
    testDb.setPassword(m_passwordEdit->text());
    testDb.setDatabaseName(m_databaseEdit->text());

    bool connected = testDb.open();

    if (connected) {
        qDebug() << "✅ DatabaseSetupDialog: Успешное подключение к PostgreSQL";
    } else {
        qDebug() << "❌ DatabaseSetupDialog: Ошибка подключения:" << testDb.lastError().text();
    }

    QSqlDatabase::removeDatabase("test_db_setup");
    return connected;
}

void DatabaseSetupDialog::onOkClicked()
{
    if (m_typeCombo->currentData().toString() == "postgresql") {
        // Проверить что поля заполнены
        if (m_hostEdit->text().isEmpty() || m_userEdit->text().isEmpty()
            || m_databaseEdit->text().isEmpty()) {
            showMessage("⚠️ Ошибка", "Заполните все поля для PostgreSQL", true);
            return;
        }
    } else {
        // SQLite
        if (m_sqlitePathEdit->text().isEmpty()) {
            showMessage("⚠️ Ошибка", "Укажите путь к файлу SQLite", true);
            return;
        }
    }

    accept();
}

void DatabaseSetupDialog::showMessage(const QString& title, const QString& message, bool isError)
{
    QMessageBox::Icon icon = isError ? QMessageBox::Warning : QMessageBox::Information;
    QMessageBox msgBox(icon, title, message, QMessageBox::Ok, this);
    msgBox.exec();
}

// ===== GETTERS =====

QString DatabaseSetupDialog::getHostname() const
{
    return m_hostEdit->text();
}

int DatabaseSetupDialog::getPort() const
{
    return m_portSpinBox->value();
}

QString DatabaseSetupDialog::getUsername() const
{
    return m_userEdit->text();
}

QString DatabaseSetupDialog::getPassword() const
{
    return m_passwordEdit->text();
}

QString DatabaseSetupDialog::getDatabaseName() const
{
    return m_databaseEdit->text();
}

QString DatabaseSetupDialog::getDatabaseType() const
{
    return m_typeCombo->currentData().toString();
}

// ===== SETTERS =====

void DatabaseSetupDialog::setHostname(const QString& host)
{
    m_hostEdit->setText(host);
}

void DatabaseSetupDialog::setPort(int port)
{
    m_portSpinBox->setValue(port);
}

void DatabaseSetupDialog::setUsername(const QString& user)
{
    m_userEdit->setText(user);
}

void DatabaseSetupDialog::setPassword(const QString& pwd)
{
    m_passwordEdit->setText(pwd);
}

void DatabaseSetupDialog::setDatabaseName(const QString& db)
{
    m_databaseEdit->setText(db);
}

void DatabaseSetupDialog::setSqlitePath(const QString& path)
{
    m_sqlitePathEdit->setText(path);
}

QString DatabaseSetupDialog::getSqlitePath() const
{
    return m_sqlitePathEdit->text();
}

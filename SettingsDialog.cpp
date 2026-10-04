#include "SettingsDialog.h"
#include "DatabaseManager.h"
#include "NotificationManager.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QFileInfo>
#include <QLabel>
#include <QCoreApplication>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QTabWidget>
#include <QFrame>
#include <QSettings>

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("⚙️ Настройки");
    setModal(true);
    resize(700, 550);

    // Диалог наследует глобальную тему из ThemeManager - локальные стили убраны

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Вкладки
    QTabWidget* tabWidget = new QTabWidget(this);

    // ============================================================
    // ВКЛАДКА 1: БАЗА ДАННЫХ
    // ============================================================
    QWidget* dbTab = new QWidget();
    QVBoxLayout* dbLayout = new QVBoxLayout(dbTab);
    dbLayout->setSpacing(16);

    QGroupBox* dbGroup = new QGroupBox("📁 База данных", dbTab);
    QVBoxLayout* dbGroupLayout = new QVBoxLayout(dbGroup);
    dbGroupLayout->setSpacing(12);

    // Путь к БД
    QLabel* pathLabel = new QLabel("Путь к файлу базы данных:", dbGroup);
    pathLabel->setStyleSheet("font-weight: 500;");
    dbGroupLayout->addWidget(pathLabel);

    m_dbPathEdit = new QLineEdit(dbGroup);
    m_dbPathEdit->setReadOnly(true);
    m_dbPathEdit->setText(DatabaseManager::instance().getDatabasePath());
    dbGroupLayout->addWidget(m_dbPathEdit);

    QHBoxLayout* pathBtnsLayout = new QHBoxLayout();
    pathBtnsLayout->setSpacing(8);

    QPushButton* browseBtn = new QPushButton("📂 Обзор...", dbGroup);
    browseBtn->setMinimumWidth(100);
    connect(browseBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowse);
    pathBtnsLayout->addWidget(browseBtn);

    QPushButton* createBtn = new QPushButton("✨ Создать новую", dbGroup);
    createBtn->setMinimumWidth(120);
    // Убираем локальные стили - будет применена глобальная тема
    connect(createBtn, &QPushButton::clicked, this, &SettingsDialog::onCreateNew);
    pathBtnsLayout->addWidget(createBtn);
    pathBtnsLayout->addStretch();

    dbGroupLayout->addLayout(pathBtnsLayout);

    // Разделитель
    QFrame* separator1 = new QFrame(dbGroup);
    separator1->setFrameShape(QFrame::HLine);
    // Убираем локальные стили - будет применена глобальная тема
    dbGroupLayout->addWidget(separator1);

    // Статус БД
    QString dbPath = DatabaseManager::instance().getDatabasePath();
    QFileInfo fi(dbPath);
    bool isNetworkDb = dbPath.contains("N:/") || dbPath.contains("ЛИЧНЫЕ ПАПКИ");
    bool isLocalDb = dbPath.contains(QCoreApplication::applicationDirPath());

    QWidget* statusWidget = new QWidget(dbGroup);
    QHBoxLayout* statusLayout = new QHBoxLayout(statusWidget);
    statusLayout->setContentsMargins(12, 12, 12, 12);
    statusLayout->setSpacing(12);

    QLabel* statusIcon = new QLabel(statusWidget);
    statusIcon->setFixedSize(40, 40);
    statusIcon->setAlignment(Qt::AlignCenter);

    QLabel* statusText = new QLabel(statusWidget);
    statusText->setWordWrap(true);

    if (fi.exists()) {
        if (isNetworkDb) {
            statusIcon->setText("✅");
            statusText->setText("<b style=''>Рабочая база данных</b><br>"
                               "<span style='font-size: 11px;'>Синхронизация с сервером активна</span>");
        } else if (isLocalDb) {
            statusIcon->setText("⚠️");
            statusText->setText("<b style=''>Локальная копия</b><br>"
                               "<span style='font-size: 11px;'>Изменения не синхронизируются с сервером</span>");
        } else {
            statusIcon->setText("✅");
            statusText->setText("<b style=''>База данных найдена</b><br>"
                               "<span style='font-size: 11px;'>Готова к работе</span>");
        }
    } else {
        statusIcon->setText("❌");
        statusText->setText("<b style=''>База данных не найдена</b><br>"
                           "<span style='font-size: 11px;'>Создайте новую или выберите существующую</span>");
    }

    statusLayout->addWidget(statusIcon);
    statusLayout->addWidget(statusText, 1);
    dbGroupLayout->addWidget(statusWidget);

    dbLayout->addWidget(dbGroup);
    dbLayout->addStretch();

    // ============================================================
    // ВКЛАДКА 2: УВЕДОМЛЕНИЯ
    // ============================================================
    QWidget* notifTab = new QWidget();
    QVBoxLayout* notifLayout = new QVBoxLayout(notifTab);
    notifLayout->setSpacing(16);

    QGroupBox* notifGroup = new QGroupBox("🔔 Настройки уведомлений", notifTab);
    QVBoxLayout* notifGroupLayout = new QVBoxLayout(notifGroup);
    notifGroupLayout->setSpacing(12);

    QCheckBox* showTrayNotif = new QCheckBox("Показывать всплывающие уведомления в трее", notifGroup);
    m_showTrayNotif = showTrayNotif;
    notifGroupLayout->addWidget(showTrayNotif);

    QCheckBox* soundNotif = new QCheckBox("Звуковое оповещение при важных уведомлениях", notifGroup);
    m_soundNotif = soundNotif;
    notifGroupLayout->addWidget(soundNotif);

    // Разделитель
    QFrame* separator2 = new QFrame(notifGroup);
    separator2->setFrameShape(QFrame::HLine);
    // Убираем локальные стили - будет применена глобальная тема
    notifGroupLayout->addWidget(separator2);

    // Интервал проверки
    QHBoxLayout* intervalLayout = new QHBoxLayout();
    QLabel* intervalLabel = new QLabel("Проверять изменения каждые:", notifGroup);
    intervalLabel->setStyleSheet("font-weight: 500; ");
    intervalLayout->addWidget(intervalLabel);

    QSpinBox* intervalSpin = new QSpinBox(notifGroup);
    intervalSpin->setMinimum(10);
    intervalSpin->setMaximum(300);
    intervalSpin->setSuffix(" сек");
    intervalSpin->setMinimumWidth(120);
    m_intervalSpin = intervalSpin;
    intervalLayout->addWidget(intervalSpin);
    intervalLayout->addStretch();
    notifGroupLayout->addLayout(intervalLayout);

    // Лимит уведомлений
    QHBoxLayout* limitLayout = new QHBoxLayout();
    QLabel* limitLabel = new QLabel("Максимум уведомлений в памяти:", notifGroup);
    limitLabel->setStyleSheet("font-weight: 500; ");
    limitLayout->addWidget(limitLabel);

    QSpinBox* limitSpin = new QSpinBox(notifGroup);
    limitSpin->setMinimum(50);
    limitSpin->setMaximum(1000);
    limitSpin->setSingleStep(50);
    limitSpin->setMinimumWidth(120);
    m_limitSpin = limitSpin;
    limitLayout->addWidget(limitSpin);
    limitLayout->addStretch();
    notifGroupLayout->addLayout(limitLayout);

    notifLayout->addWidget(notifGroup);
    notifLayout->addStretch();

    // ============================================================
    // ВКЛАДКА 3: ИНТЕРФЕЙС
    // ============================================================
    QWidget* uiTab = new QWidget();
    QVBoxLayout* uiLayout = new QVBoxLayout(uiTab);
    uiLayout->setSpacing(16);

    QGroupBox* uiGroup = new QGroupBox("🎨 Настройки интерфейса", uiTab);
    QVBoxLayout* uiGroupLayout = new QVBoxLayout(uiGroup);
    uiGroupLayout->setSpacing(12);

    QCheckBox* compactMode = new QCheckBox("Компактный режим отображения", uiGroup);
    m_compactMode = compactMode;
    uiGroupLayout->addWidget(compactMode);

    // Разделитель
    QFrame* separator3 = new QFrame(uiGroup);
    separator3->setFrameShape(QFrame::HLine);
    // Убираем локальные стили - будет применена глобальная тема
    uiGroupLayout->addWidget(separator3);

    // Тема
    QHBoxLayout* themeLayout = new QHBoxLayout();
    QLabel* themeLabel = new QLabel("Тема оформления:", uiGroup);
    themeLabel->setStyleSheet("font-weight: 500; ");
    themeLayout->addWidget(themeLabel);

    QComboBox* themeCombo = new QComboBox(uiGroup);
    themeCombo->addItems({"Светлая", "Тёмная", "Системная"});
    themeCombo->setMinimumWidth(150);
    m_themeCombo = themeCombo;
    themeLayout->addWidget(themeCombo);
    themeLayout->addStretch();
    uiGroupLayout->addLayout(themeLayout);

    uiLayout->addWidget(uiGroup);
    uiLayout->addStretch();

    // ============================================================
    // ВКЛАДКА 4: СЕРВЕР
    // ============================================================
    QWidget* serverTab = new QWidget();
    QVBoxLayout* serverLayout = new QVBoxLayout(serverTab);
    serverLayout->setSpacing(16);

    QGroupBox* serverGroup = new QGroupBox("🌐 PostgreSQL Сервер", serverTab);
    QFormLayout* serverFormLayout = new QFormLayout(serverGroup);
    serverFormLayout->setSpacing(12);

    // Хост
    m_serverHost = new QLineEdit(serverGroup);
    m_serverHost->setPlaceholderText("localhost или 192.168.x.x");
    serverFormLayout->addRow("Хост:", m_serverHost);

    // Порт
    m_serverPort = new QSpinBox(serverGroup);
    m_serverPort->setMinimum(1);
    m_serverPort->setMaximum(65535);
    m_serverPort->setValue(5432);
    serverFormLayout->addRow("Порт:", m_serverPort);

    // Пользователь
    m_serverUser = new QLineEdit(serverGroup);
    m_serverUser->setPlaceholderText("kd_user");
    serverFormLayout->addRow("Пользователь:", m_serverUser);

    // Пароль
    m_serverPassword = new QLineEdit(serverGroup);
    m_serverPassword->setEchoMode(QLineEdit::Password);
    serverFormLayout->addRow("Пароль:", m_serverPassword);

    // База данных
    m_serverDatabase = new QLineEdit(serverGroup);
    m_serverDatabase->setPlaceholderText("kd_documents");
    serverFormLayout->addRow("База данных:", m_serverDatabase);

    // Кнопка проверки
    QPushButton* testBtn = new QPushButton("🔍 Проверить соединение", serverGroup);
    connect(testBtn, &QPushButton::clicked, this, &SettingsDialog::onTestConnection);
    serverFormLayout->addRow("", testBtn);

    serverLayout->addWidget(serverGroup);
    serverLayout->addStretch();

    // Добавляем вкладки
    tabWidget->addTab(dbTab, "📁 База данных");
    tabWidget->addTab(serverTab, "🌐 Сервер");
    tabWidget->addTab(notifTab, "🔔 Уведомления");
    tabWidget->addTab(uiTab, "🎨 Интерфейс");

    mainLayout->addWidget(tabWidget);

    // Кнопки
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText("Сохранить");
    buttonBox->button(QDialogButtonBox::Cancel)->setText("Отмена");
    // Убираем локальные стили - будет применена глобальная тема

    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::onOk);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(buttonBox);

    // Загружаем сохранённые настройки
    loadSettings();
}

void SettingsDialog::onBrowse()
{
    QString filePath = QFileDialog::getOpenFileName(this,
                                                    "Выберите файл базы данных",
                                                    m_dbPathEdit->text(),
                                                    "База данных (*.db);;Все файлы (*.*)");

    if (!filePath.isEmpty()) {
        m_dbPathEdit->setText(filePath);
        // Проверяем, есть ли файл
        if (QFileInfo::exists(filePath)) {
            QMessageBox::information(this, "База данных выбрана",
                                     "Выбрана база данных:\n" + filePath + "\n\n"
                                                                           "Нажмите OK для применения.");
        } else {
            QMessageBox::warning(this, "Файл не найден",
                                 "Указанный файл не существует.\n"
                                 "Создайте новую базу данных или выберите существующий файл.");
        }
    }
}

void SettingsDialog::onCreateNew()
{
    QString filePath = QFileDialog::getSaveFileName(this,
                                                    "Создать новую базу данных",
                                                    QCoreApplication::applicationDirPath() + "/kd_db.dat",
                                                    "База данных (*.dat)");

    if (!filePath.isEmpty()) {
        // Создаем новую пустую БД
        DatabaseManager::instance().setDatabasePath(filePath);
        DatabaseManager::instance().createDefaultStructure();
        DatabaseManager::instance().saveDatabase();
        m_dbPathEdit->setText(filePath);
        QMessageBox::information(this, "База данных создана",
                                 "✅ Новая база данных успешно создана по пути:\n" + filePath + "\n\n"
                                                                                                "⚠️ Это локальная копия. Для работы с сервером укажите путь к серверной БД.");
    }
}

void SettingsDialog::onOk()
{
    QString newPath = m_dbPathEdit->text();
    QString currentPath = DatabaseManager::instance().getDatabasePath();

    // Если путь изменился, сохраняем его
    if (newPath != currentPath) {
        // Проверяем, существует ли файл
        if (!QFileInfo::exists(newPath)) {
            int result = QMessageBox::question(this, "Файл не найден",
                                               "Файл базы данных не существует по пути:\n" + newPath + "\n\n"
                                                                                                       "Создать новую базу данных?",
                                               QMessageBox::Yes | QMessageBox::No);
            if (result == QMessageBox::Yes) {
                DatabaseManager::instance().setDatabasePath(newPath);
                DatabaseManager::instance().createDefaultStructure();
                DatabaseManager::instance().saveDatabase();
            } else {
                return;
            }
        } else {
            // Сохраняем новый путь в настройках
            QSettings settings("KDManager", "Settings");
            settings.setValue("database/path", newPath);
            settings.sync();
            qDebug() << "💾 Новый путь к БД сохранен:" << newPath;

            // Показываем сообщение что нужно перезагрузиться
            QMessageBox::information(this, "Путь к БД изменен",
                                    "✅ Новый путь к БД сохранен:\n" + newPath + "\n\n"
                                    "⚠️ Для применения изменений нужно перезагрузить приложение.\n"
                                    "Пожалуйста, закройте и запустите приложение заново.");
            accept();
            return;
        }
    }

    // Сохраняем остальные настройки
    saveSettings();

    QMessageBox::information(this, "Настройки сохранены",
                            "✅ Настройки успешно применены.\n"
                            "Некоторые изменения вступят в силу после перезапуска программы.");

    accept();
}

void SettingsDialog::loadSettings()
{
    QSettings settings("KDManager", "Settings");

    // Загружаем путь к БД (если был выбран ручно)
    QString savedDbPath = settings.value("database/path", "").toString();
    if (!savedDbPath.isEmpty() && QFileInfo::exists(savedDbPath)) {
        m_dbPathEdit->setText(savedDbPath);
    }

    // Загружаем настройки уведомлений
    m_showTrayNotif->setChecked(settings.value("notifications/showTray", true).toBool());
    m_soundNotif->setChecked(settings.value("notifications/sound", false).toBool());
    m_intervalSpin->setValue(settings.value("notifications/checkInterval", 60).toInt());
    m_limitSpin->setValue(settings.value("notifications/maxNotifications", 200).toInt());

    // Загружаем настройки интерфейса
    m_compactMode->setChecked(settings.value("ui/compactMode", false).toBool());
    m_themeCombo->setCurrentIndex(settings.value("ui/theme", 0).toInt());
}

void SettingsDialog::saveSettings()
{
    QSettings settings("KDManager", "Settings");

    // Сохраняем путь к БД
    QString dbPath = m_dbPathEdit->text();
    if (!dbPath.isEmpty()) {
        settings.setValue("database/path", dbPath);
        qDebug() << "💾 Путь к БД сохранен:" << dbPath;
    }

    // Сохраняем настройки уведомлений
    settings.setValue("notifications/showTray", m_showTrayNotif->isChecked());
    settings.setValue("notifications/sound", m_soundNotif->isChecked());
    settings.setValue("notifications/checkInterval", m_intervalSpin->value());
    settings.setValue("notifications/maxNotifications", m_limitSpin->value());

    // Сохраняем настройки интерфейса
    settings.setValue("ui/compactMode", m_compactMode->isChecked());
    settings.setValue("ui/theme", m_themeCombo->currentIndex());

    // Применяем настройки к NotificationManager
    NotificationManager::instance().setShowTrayNotifications(m_showTrayNotif->isChecked());
    NotificationManager::instance().setSoundEnabled(m_soundNotif->isChecked());
    NotificationManager::instance().setCheckInterval(m_intervalSpin->value());
    NotificationManager::instance().setMaxNotifications(m_limitSpin->value());

    // Применяем выбранную тему
    ThemeManager::Theme theme = static_cast<ThemeManager::Theme>(m_themeCombo->currentIndex());
    ThemeManager::instance().setTheme(theme);

    settings.sync();
}

void SettingsDialog::onTestConnection()
{
    QString host = m_serverHost->text().isEmpty() ? "localhost" : m_serverHost->text();
    int port = m_serverPort->value();
    QString user = m_serverUser->text().isEmpty() ? "kd_user" : m_serverUser->text();
    QString password = m_serverPassword->text();
    QString database = m_serverDatabase->text().isEmpty() ? "kd_documents" : m_serverDatabase->text();

    QSqlDatabase testDb = QSqlDatabase::addDatabase("QPSQL", "test_settings");
    testDb.setHostName(host);
    testDb.setPort(port);
    testDb.setUserName(user);
    testDb.setPassword(password);
    testDb.setDatabaseName(database);

    if (testDb.open()) {
        QMessageBox::information(this, "✅ Успешно",
                                 "Соединение с PostgreSQL установлено!\n\n"
                                 "Сервер: " + host + ":" + QString::number(port) + "\n"
                                 "База данных: " + database);
        testDb.close();
    } else {
        QMessageBox::warning(this, "❌ Ошибка подключения",
                            "Не удалось подключиться к серверу PostgreSQL.\n\n"
                            "Ошибка: " + testDb.lastError().text() + "\n\n"
                            "Проверьте:\n"
                            "1. IP адрес и порт\n"
                            "2. Пользователя и пароль\n"
                            "3. Имя базы данных\n"
                            "4. Статус сервиса PostgreSQL");
    }

    QSqlDatabase::removeDatabase("test_settings");
}
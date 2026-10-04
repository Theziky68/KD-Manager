// AboutDialog.cpp
#include "AboutDialog.h"
#include "ThemeManager.h"
#include <QFile>
#include <QTextStream>
#include <QScrollArea>
#include <QGroupBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QDate>
#include <QHeaderView>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#include <QFileInfo>
#include <QPalette>
#include <QSettings>
#include <QSqlQuery>
#include <QDebug>
#include <QScrollBar>

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
    , m_dbPathLabel(nullptr)
    , m_dbSizeLabel(nullptr)
    , m_dbModifiedLabel(nullptr)
    , m_dbVersionLabel(nullptr)
    , m_dbRecordsLabel(nullptr)
    , m_dbStatusLabel(nullptr)
{
    setupUI();
    applyLightTheme();
    setWindowTitle("ℹ️ О программе");
    setModal(true);
    resize(780, 650);
    setMinimumSize(700, 550);
    updateDatabaseInfo();

    QTimer::singleShot(10, this, [this]() {
        applyLightTheme();
        update();
    });
}

// ===== ВСПОМОГАТЕЛЬНЫЙ МЕТОД ДЛЯ ПРИМЕНЕНИЯ ТЕМЫ =====
void AboutDialog::applyLightTheme()
{
    ThemeManager& tm = ThemeManager::instance();

    QPalette pal;
    pal.setColor(QPalette::Window, QColor(tm.backgroundColor()));
    pal.setColor(QPalette::WindowText, QColor(tm.textColor()));
    pal.setColor(QPalette::Base, QColor(tm.cardBackground()));
    pal.setColor(QPalette::AlternateBase, QColor(tm.hoverColor()));
    pal.setColor(QPalette::Text, QColor(tm.textColor()));
    pal.setColor(QPalette::Button, QColor(tm.backgroundColor()));
    pal.setColor(QPalette::ButtonText, QColor(tm.textColor()));
    pal.setColor(QPalette::BrightText, QColor("#FFFFFF"));
    pal.setColor(QPalette::Link, QColor(tm.accentColor()));
    pal.setColor(QPalette::Highlight, QColor(tm.accentColor()));
    pal.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
    pal.setColor(QPalette::ToolTipBase, QColor(tm.cardBackground()));
    pal.setColor(QPalette::ToolTipText, QColor(tm.textColor()));
    setPalette(pal);

    setStyleSheet(QString(
        "QDialog { background-color: %1; }"
        "QWidget { background-color: %1; color: %2; }"
        "QTabWidget { background-color: %3; }"
        "QTabWidget::pane { background-color: %3; border: 1px solid %4; border-radius: 8px; margin-top: -1px; }"
        "QTabWidget::tab-bar { alignment: center; background-color: %1; }"
        "QTabBar::tab { background: %5; color: %2; padding: 8px 20px; border: 1px solid %4; border-bottom: none; border-top-left-radius: 8px; border-top-right-radius: 8px; font-family: 'Segoe UI', 'Arial', sans-serif; font-size: 13px; font-weight: 500; }"
        "QTabBar::tab:selected { background: %3; border-bottom: 2px solid %6; }"
        "QTabBar::tab:hover { background: %1; }"
        "QLabel { color: %2; font-family: 'Segoe UI', 'Arial', sans-serif; background-color: transparent; }"
        "QGroupBox { font-weight: 600; color: %2; font-family: 'Segoe UI', 'Arial', sans-serif; border: 1px solid %4; border-radius: 8px; margin-top: 12px; padding-top: 10px; background-color: %3; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; background-color: %3; color: %2; }"
        "QTableWidget { background-color: %3; border: 1px solid %4; border-radius: 6px; gridline-color: %4; font-family: 'Segoe UI', 'Arial', sans-serif; color: %2; }"
        "QTableWidget::item { padding: 6px 12px; color: %2; background-color: %3; }"
        "QTableWidget::item:selected { background-color: %7; color: %2; }"
        "QHeaderView::section { background: %5; padding: 6px 12px; border: none; border-right: 1px solid %4; font-weight: 600; color: %2; font-family: 'Segoe UI', 'Arial', sans-serif; }"
        "QTextEdit { background-color: %3; border: 1px solid %4; border-radius: 6px; padding: 10px; font-family: 'Segoe UI', 'Arial', sans-serif; font-size: 13px; color: %2; }"
        "QTextEdit:focus { border: 2px solid %6; }"
        "QScrollArea { border: none; background-color: %3; }"
        "QScrollArea QWidget { background-color: %3; }"
        "QScrollBar:vertical { background: %1; width: 8px; margin: 0px; border-radius: 4px; }"
        "QScrollBar::handle:vertical { background: %8; border-radius: 4px; min-height: 20px; }"
        "QScrollBar::handle:vertical:hover { background: %9; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %10, stop:1 %11); color: #ffffff; border: none; border-radius: 8px; padding: 8px 30px; font-size: 13px; font-weight: 600; font-family: 'Segoe UI', 'Arial', sans-serif; }"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %12, stop:1 %13); }"
        "QPushButton:pressed { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %14, stop:1 %15); }"
        "QPushButton#refreshBtn { background: transparent; color: %6; border: 1px solid %6; border-radius: 6px; padding: 4px 12px; font-size: 11px; font-weight: 500; font-family: 'Segoe UI', 'Arial', sans-serif; }"
        "QPushButton#refreshBtn:hover { background: rgba(99, 102, 241, 0.1); }"
        "QWidget#aboutTab, QWidget#infoTab, QWidget#structureTab, QWidget#shortcutsTab, QWidget#helpTab { background-color: %3; }"
        "QListWidget { background-color: %3; border: 1px solid %4; border-radius: 6px; color: %2; font-family: 'Segoe UI', 'Arial', sans-serif; }"
        "QListWidget::item { padding: 4px 8px; }"
        "QListWidget::item:selected { background-color: %7; }"
    ).arg(tm.backgroundColor(),
          tm.textColor(),
          tm.cardBackground(),
          tm.borderColor(),
          tm.hoverColor(),
          tm.accentColor(),
          tm.currentTheme() == ThemeManager::Dark ? "#2A3F3F" : "#e8edf0",
          tm.currentTheme() == ThemeManager::Dark ? "#4B5563" : "#c0c0c0",
          tm.currentTheme() == ThemeManager::Dark ? "#6B7280" : "#a0a0a0",
          tm.primaryGradientStart(),
          tm.primaryGradientEnd(),
          tm.currentTheme() == ThemeManager::Dark ? "#5F6F7F" : "#78909C",
          tm.currentTheme() == ThemeManager::Dark ? "#475A6A" : "#546E7A",
          tm.currentTheme() == ThemeManager::Dark ? "#3C4A5A" : "#455A64",
          tm.currentTheme() == ThemeManager::Dark ? "#2C3A4A" : "#37474F"));

    update();
    repaint();
}

void AboutDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(12);

    QLabel* titleLabel = new QLabel("📂 КД Менеджер", this);
    titleLabel->setStyleSheet("QLabel { font-size: 24px; font-weight: 700; color: #263238; font-family: 'Segoe UI', 'Arial', sans-serif; background: transparent; }");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(createAboutTab(), "📋 О программе");
    m_tabWidget->addTab(createInfoTab(), "📖 Как работать");
    m_tabWidget->addTab(createStructureTab(), "🏗️ Структура");
    m_tabWidget->addTab(createShortcutsTab(), "⌨️ Горячие клавиши");
    m_tabWidget->addTab(createHelpTab(), "❓ Помощь");

    mainLayout->addWidget(m_tabWidget);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_closeBtn = new QPushButton("Закрыть", this);
    m_closeBtn->setFixedWidth(120);
    m_closeBtn->setMinimumHeight(36);
    connect(m_closeBtn, &QPushButton::clicked, this, &AboutDialog::accept);

    btnLayout->addWidget(m_closeBtn);
    mainLayout->addLayout(btnLayout);
}

void AboutDialog::updateDatabaseInfo()
{
    DatabaseManager& db = DatabaseManager::instance();
    QString dbPath = db.getDatabasePath();
    QFileInfo dbInfo(dbPath);

    if (m_dbPathLabel) {
        m_dbPathLabel->setText(dbPath);
        m_dbPathLabel->setToolTip(dbPath);
    }

    if (m_dbSizeLabel) {
        if (dbInfo.exists()) {
            qint64 size = dbInfo.size();
            if (size < 1024) {
                m_dbSizeLabel->setText(QString::number(size) + " B");
            } else if (size < 1048576) {
                m_dbSizeLabel->setText(QString::number(size / 1024) + " KB");
            } else if (size < 1073741824) {
                m_dbSizeLabel->setText(QString::number(size / 1048576) + " MB");
            } else {
                m_dbSizeLabel->setText(QString::number(size / 1073741824) + " GB");
            }
        } else {
            m_dbSizeLabel->setText("❌ Файл не найден");
            m_dbSizeLabel->setStyleSheet("color: #e53935; background: transparent;");
        }
    }

    if (m_dbModifiedLabel) {
        if (dbInfo.exists()) {
            m_dbModifiedLabel->setText(dbInfo.lastModified().toString("dd.MM.yyyy hh:mm:ss"));
        } else {
            m_dbModifiedLabel->setText("—");
        }
    }

    if (m_dbVersionLabel) {
        QString versionInfo = db.getDatabaseVersionInfo();
        m_dbVersionLabel->setText(versionInfo);
    }

    if (m_dbRecordsLabel) {
        QSqlQuery query(db.getDatabase());
        int folders = 0, documents = 0, comments = 0, notifications = 0;

        if (query.exec("SELECT COUNT(*) FROM folders") && query.next()) {
            folders = query.value(0).toInt();
        }
        if (query.exec("SELECT COUNT(*) FROM documents") && query.next()) {
            documents = query.value(0).toInt();
        }
        if (query.exec("SELECT COUNT(*) FROM comments") && query.next()) {
            comments = query.value(0).toInt();
        }
        if (query.exec("SELECT COUNT(*) FROM notifications") && query.next()) {
            notifications = query.value(0).toInt();
        }

        m_dbRecordsLabel->setText(QString("Папок: %1 | Документов: %2 | Комментариев: %3 | Уведомлений: %4")
                                      .arg(folders).arg(documents).arg(comments).arg(notifications));
    }

    if (m_dbStatusLabel) {
        bool isNetwork = db.isNetworkDatabase();
        bool isLocked = db.isDatabaseLocked();
        bool isCorrupted = db.isDatabaseCorrupted();

        QString status;
        QString color;
        if (!dbInfo.exists()) {
            status = "❌ База данных не найдена";
            color = "#e53935";
        } else if (isCorrupted) {
            status = "⚠️ База данных повреждена!";
            color = "#e53935";
        } else if (isLocked) {
            status = "🔒 База данных заблокирована";
            color = "#FF9800";
        } else if (isNetwork) {
            status = "🌐 Сетевая база данных (рабочая)";
            color = "#4CAF50";
        } else {
            status = "💻 Локальная база данных";
            color = "#2196F3";
        }
        m_dbStatusLabel->setText(status);
        m_dbStatusLabel->setStyleSheet(QString("color: %1; font-weight: bold; background: transparent;").arg(color));
    }
}

// ===== ВКЛАДКА "О ПРОГРАММЕ" =====
QWidget* AboutDialog::createAboutTab()
{
    ThemeManager& tm = ThemeManager::instance();

    QWidget* widget = new QWidget();
    widget->setObjectName("aboutTab");

    QVBoxLayout* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(30, 30, 30, 30);
    layout->setSpacing(20);

    // ===== ЗАГОЛОВОК С ИКОНОЙ И НАЗВАНИЕМ =====
    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->setAlignment(Qt::AlignCenter);
    headerLayout->setSpacing(15);

    QLabel* iconLabel = new QLabel("📂", widget);
    iconLabel->setStyleSheet(QString("QLabel { font-size: 64px; background: transparent; }"));
    iconLabel->setFixedSize(100, 100);
    iconLabel->setAlignment(Qt::AlignCenter);
    headerLayout->addWidget(iconLabel);

    QVBoxLayout* titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(5);

    QLabel* nameLabel = new QLabel("КД Менеджер", widget);
    nameLabel->setStyleSheet(QString(
        "QLabel { font-size: 36px; font-weight: 900; color: %1; "
        "font-family: 'Segoe UI', 'Arial', sans-serif; background: transparent; }"
    ).arg(tm.textColor()));
    nameLabel->setAlignment(Qt::AlignCenter);
    titleLayout->addWidget(nameLabel);

    QLabel* taglineLabel = new QLabel("Современная система управления технической документацией для инженерных организаций", widget);
    taglineLabel->setStyleSheet(QString(
        "QLabel { font-size: 13px; color: %1; "
        "font-family: 'Segoe UI', 'Arial', sans-serif; background: transparent; font-style: italic; }"
    ).arg(tm.accentColor()));
    taglineLabel->setAlignment(Qt::AlignCenter);
    titleLayout->addWidget(taglineLabel);

    headerLayout->addLayout(titleLayout);
    layout->addLayout(headerLayout);

    // ===== ВЕРСИЯ И ОПИСАНИЕ =====
    QLabel* versionLabel = new QLabel("Версия 3.0 | © 2024-2026 КД Менеджер", widget);
    versionLabel->setStyleSheet(QString(
        "QLabel { font-size: 11px; color: %1; "
        "font-family: 'Segoe UI', 'Arial', sans-serif; background: transparent; }"
    ).arg(tm.secondaryTextColor()));
    versionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(versionLabel);

    // ===== ОСНОВНЫЕ ВОЗМОЖНОСТИ =====
    QLabel* featuresTitle = new QLabel("✨ Основные возможности", widget);
    featuresTitle->setStyleSheet(QString(
        "QLabel { font-size: 14px; font-weight: 700; color: %1; background: transparent; }"
    ).arg(tm.textColor()));
    layout->addWidget(featuresTitle);

    QStringList features = {
        "📁 Иерархическое хранилище с полнотекстовым поиском",
        "🔍 Интеллектуальный поиск по параметрам и метаданным",
        "💬 Встроенная система обсуждения и комментирования",
        "🔔 Умные уведомления об изменениях статуса",
        "🧩 Интеграция с КОМПАС-3D и 3D Viewer Online",
        "📊 Контроль версий с историей всех изменений",
        "☁️ Поддержка облачного хранилища и сетевых БД",
        "🌓 Адаптивный дизайн с тёмной/светлой темой"
    };

    for (const QString& feature : features) {
        QLabel* featureLabel = new QLabel(feature, widget);
        featureLabel->setStyleSheet(QString(
            "QLabel { font-size: 12px; color: %1; padding-left: 20px; "
            "font-family: 'Segoe UI', 'Arial', sans-serif; background: transparent; }"
        ).arg(tm.textColor()));
        featureLabel->setWordWrap(true);
        layout->addWidget(featureLabel);
    }

    layout->addSpacing(15);

    // ===== ДЛЯ КОГО =====
    QLabel* audienceTitle = new QLabel("🎯 Предназначен для", widget);
    audienceTitle->setStyleSheet(QString(
        "QLabel { font-size: 14px; font-weight: 700; color: %1; background: transparent; }"
    ).arg(tm.textColor()));
    layout->addWidget(audienceTitle);

    QStringList audience = {
        "🏗️ Инженерные бюро и центры конструкторских данных",
        "🛠️ Конструкторские отделы промышленных предприятий",
        "🏭 Заводы и производственные комплексы",
        "📐 Проектные организации и дизайн-студии"
    };

    for (const QString& item : audience) {
        QLabel* itemLabel = new QLabel(item, widget);
        itemLabel->setStyleSheet(QString(
            "QLabel { font-size: 12px; color: %1; padding-left: 20px; "
            "font-family: 'Segoe UI', 'Arial', sans-serif; background: transparent; }"
        ).arg(tm.textColor()));
        layout->addWidget(itemLabel);
    }

    layout->addSpacing(15);

    // ===== ТЕХНОЛОГИИ =====
    QLabel* techTitle = new QLabel("⚙️ Технологии", widget);
    techTitle->setStyleSheet(QString(
        "QLabel { font-size: 14px; font-weight: 700; color: %1; background: transparent; }"
    ).arg(tm.textColor()));
    layout->addWidget(techTitle);

    QLabel* techLabel = new QLabel(
        "C++ 17 • Qt 6.0 • SQLite 3 • JSON • Desktop Application",
        widget
    );
    techLabel->setStyleSheet(QString(
        "QLabel { font-size: 12px; color: %1; padding-left: 20px; "
        "font-family: 'Segoe UI', 'Arial', sans-serif; background: transparent; }"
    ).arg(tm.secondaryTextColor()));
    layout->addWidget(techLabel);

    layout->addSpacing(15);

    // ===== ИНФОРМАЦИЯ О БД =====
    QGroupBox* dbGroup = new QGroupBox("📊 Информация о базе данных", widget);
    dbGroup->setStyleSheet(QString(
        "QGroupBox { font-weight: 600; color: %1; font-family: 'Segoe UI', 'Arial', sans-serif; "
        "border: 1px solid %2; border-radius: 8px; margin-top: 12px; padding-top: 10px; "
        "background-color: %3; } "
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; "
        "background-color: %3; color: %1; }"
    ).arg(tm.textColor(), tm.borderColor(), tm.cardBackground()));

    QFormLayout* dbLayout = new QFormLayout(dbGroup);
    dbLayout->setSpacing(8);
    dbLayout->setContentsMargins(12, 8, 12, 8);

    m_dbPathLabel = new QLabel(widget);
    m_dbPathLabel->setWordWrap(true);
    m_dbPathLabel->setStyleSheet(QString("QLabel { font-size: 11px; color: %1; "
        "font-family: 'Segoe UI', 'Arial', sans-serif; background: transparent; }")
        .arg(tm.secondaryTextColor()));
    dbLayout->addRow("📁 Путь:", m_dbPathLabel);

    m_dbSizeLabel = new QLabel(widget);
    m_dbSizeLabel->setStyleSheet(QString("QLabel { font-size: 11px; color: %1; "
        "font-family: 'Segoe UI', 'Arial', sans-serif; background: transparent; }")
        .arg(tm.secondaryTextColor()));
    dbLayout->addRow("📏 Размер:", m_dbSizeLabel);

    m_dbStatusLabel = new QLabel(widget);
    m_dbStatusLabel->setStyleSheet(QString("QLabel { font-size: 11px; font-family: 'Segoe UI', 'Arial', sans-serif; background: transparent; }"));
    dbLayout->addRow("🔵 Статус:", m_dbStatusLabel);

    m_dbRecordsLabel = new QLabel(widget);
    m_dbRecordsLabel->setStyleSheet(QString("QLabel { font-size: 11px; color: %1; "
        "font-family: 'Segoe UI', 'Arial', sans-serif; background: transparent; }")
        .arg(tm.secondaryTextColor()));
    dbLayout->addRow("📊 Записей:", m_dbRecordsLabel);

    layout->addWidget(dbGroup);
    layout->addStretch();

    return widget;
}

// ===== ВКЛАДКА "КАК РАБОТАТЬ" =====
QWidget* AboutDialog::createInfoTab()
{
    ThemeManager& tm = ThemeManager::instance();

    QWidget* widget = new QWidget();
    widget->setObjectName("infoTab");

    QVBoxLayout* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(20, 15, 20, 15);
    layout->setSpacing(15);

    QScrollArea* scrollArea = new QScrollArea(widget);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    QWidget* contentWidget = new QWidget(scrollArea);
    QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setSpacing(15);
    contentLayout->setContentsMargins(0, 0, 0, 0);

    struct StepInfo { QString title; QString description; QString icon; };
    QList<StepInfo> steps = {
        {"Создание изделия",
         "1️⃣ Нажмите кнопку <b>«Изделие»</b> на панели инструментов (Ctrl+N)\n2️⃣ Введите название нового изделия\n3️⃣ Автоматически создаются папки:\n   📁 Главный вид 3д модель\n   📁 Главный сборочный чертеж\n   📁 Основная спецификация\n💡 <i>Изделие — это корневая папка для всей документации</i>",
         "1️⃣"},
        {"Создание сборки (СБ)",
         "1️⃣ Нажмите правой кнопкой мыши на изделии в дереве\n2️⃣ Выберите пункт <b>«Создать СБ»</b>\n3️⃣ Введите название сборки\n4️⃣ Автоматически создаются папки:\n   📁 Детали/\n   📁 Спецификация/\n💡 <i>Сборка объединяет детали и чертежи в единую конструкцию</i>",
         "2️⃣"},
        {"Добавление файлов",
         "1️⃣ Откройте нужную папку (двойной клик по папке в дереве)\n2️⃣ Нажмите кнопку <b>«Файлы»</b> на панели инструментов\n3️⃣ В диалоге выберите:\n   🖥️ 3D-модель (.m3d, .a3d, .sldprt, .sldasm)\n   📐 Чертёж (.cdw, .frw, .dxf, .dwg)\n   📎 Дополнительный файл\n4️⃣ Или перетащите файлы в окно программы\n💡 <i>Файлы автоматически сортируются по типам</i>",
         "3️⃣"},
        {"Просмотр файлов",
         "1️⃣ <b>Один клик</b> по файлу — открывается <b>превью</b>\n2️⃣ <b>Двойной клик</b> по файлу — открывается в программе по умолчанию\n3️⃣ <b>Правый клик</b> по файлу — контекстное меню\n💡 <i>Для файлов КОМПАС доступен просмотр в 3D Viewer</i>",
         "4️⃣"},
        {"Комментарии и обсуждение",
         "1️⃣ Нажмите правой кнопкой мыши на папке или файле\n2️⃣ Выберите пункт <b>«Комментарии»</b>\n3️⃣ В открывшемся окне можно:\n   💬 Оставлять комментарии\n   ↩️ Отвечать на комментарии\n   📊 Менять статус документа\n   👤 Назначать ответственного\n💡 <i>Все комментарии сохраняются в базе данных</i>",
         "5️⃣"},
        {"Поиск файлов",
         "1️⃣ Нажмите кнопку <b>«Поиск»</b> на панели инструментов (Ctrl+F)\n2️⃣ Введите имя файла для поиска\n3️⃣ Двойной клик по результату — открывает файл\n💡 <i>Поиск работает по всем папкам и файлам</i>",
         "6️⃣"},
        {"Резервное копирование",
         "1️⃣ Нажмите кнопку <b>«Бэкап»</b> на панели инструментов (Ctrl+Shift+S)\n2️⃣ Автоматический бэкап создается каждый час\n3️⃣ Бэкапы хранятся 3 дня\n4️⃣ При повреждении БД восстанавливается из последнего бэкапа\n💡 <i>Бэкапы сохраняются в папку backups/</i>",
         "7️⃣"}
    };

    for (const StepInfo& step : steps) {
        QGroupBox* group = new QGroupBox(step.icon + " " + step.title, contentWidget);
        group->setStyleSheet(QString(
            "QGroupBox { font-weight: 600; color: %1; font-family: 'Segoe UI', 'Arial', sans-serif; border: 1px solid %2; border-radius: 8px; margin-top: 12px; padding-top: 10px; background-color: %3; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; background-color: %3; color: %1; }"
            ).arg(tm.textColor(), tm.borderColor(), tm.cardBackground()));

        QVBoxLayout* groupLayout = new QVBoxLayout(group);
        QLabel* descLabel = new QLabel(step.description, group);
        descLabel->setWordWrap(true);
        descLabel->setStyleSheet(QString("QLabel { font-size: 13px; color: %1; font-family: 'Segoe UI', 'Arial', sans-serif; font-weight: 400; padding: 4px 8px; background: transparent; }").arg(tm.textColor()));
        groupLayout->addWidget(descLabel);
        contentLayout->addWidget(group);
    }

    scrollArea->setWidget(contentWidget);
    layout->addWidget(scrollArea);
    return widget;
}

// ===== ВКЛАДКА "СТРУКТУРА" =====
QWidget* AboutDialog::createStructureTab()
{
    ThemeManager& tm = ThemeManager::instance();

    QWidget* widget = new QWidget();
    widget->setObjectName("structureTab");

    QVBoxLayout* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(20, 15, 20, 15);
    layout->setSpacing(15);

    QLabel* titleLabel = new QLabel("🏗️ Структура программы", widget);
    titleLabel->setStyleSheet(QString("QLabel { font-size: 18px; font-weight: 600; color: %1; font-family: 'Segoe UI', 'Arial', sans-serif; background: transparent; }").arg(tm.textColor()));
    layout->addWidget(titleLabel);

    QTextEdit* structureEdit = new QTextEdit(widget);
    structureEdit->setReadOnly(true);
    structureEdit->setStyleSheet(QString(
        "QTextEdit { background-color: %1; border: 1px solid %2; border-radius: 6px; padding: 15px; font-family: 'Consolas', 'Courier New', monospace; font-size: 12px; color: %3; }"
        ).arg(tm.cardBackground(), tm.borderColor(), tm.textColor()));
    structureEdit->setPlainText(
        "📁 КД Менеджер\n"
        "├── 📂 Главная страница\n"
        "│   ├── 📦 Статистика (изделия, СБ, детали, файлы)\n"
        "│   ├── 🔍 Поиск изделий\n"
        "│   └── 📋 Список изделий\n"
        "│\n"
        "├── 📂 Рабочая страница\n"
        "│   ├── 📁 Дерево папок\n"
        "│   │   ├── 🏢 Изделия\n"
        "│   │   ├── 📚 Справочники\n"
        "│   │   ├── 📦 Архив\n"
        "│   │   ├── ⚙️ Техпроцессы\n"
        "│   │   └── 📎 Другое\n"
        "│   │\n"
        "│   ├── 📄 Карточки файлов\n"
        "│   │   ├── 🖥️ 3D модели\n"
        "│   │   ├── 📐 Чертежи\n"
        "│   │   └── 📎 Прочие файлы\n"
        "│   │\n"
        "│   └── 👁️ Превью файлов\n"
        "│       ├── 🔧 Открыть в КОМПАС-3D\n"
        "│       └── 👁️ Открыть в 3D Viewer\n"
        "│\n"
        "├── 📋 Панель уведомлений\n"
        "│   ├── 🔔 Новые уведомления\n"
        "│   ├── 📝 Фильтр по типу\n"
        "│   └── 🔍 Поиск уведомлений\n"
        "│\n"
        "└── 📊 База данных (SQLite)\n"
        "    ├── 📁 folders  — папки и разделы\n"
        "    ├── 📄 documents — файлы и документы\n"
        "    ├── 💬 comments  — комментарии\n"
        "    └── 🔔 notifications — уведомления"
        );
    layout->addWidget(structureEdit);
    return widget;
}

// ===== ВКЛАДКА "ГОРЯЧИЕ КЛАВИШИ" =====
QWidget* AboutDialog::createShortcutsTab()
{
    ThemeManager& tm = ThemeManager::instance();

    QWidget* widget = new QWidget();
    widget->setObjectName("shortcutsTab");

    QVBoxLayout* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(20, 15, 20, 15);

    QLabel* titleLabel = new QLabel("⌨️ Горячие клавиши", widget);
    titleLabel->setStyleSheet(QString("QLabel { font-size: 18px; font-weight: 600; color: %1; font-family: 'Segoe UI', 'Arial', sans-serif; background: transparent; }").arg(tm.textColor()));
    layout->addWidget(titleLabel);

    QTableWidget* table = new QTableWidget(20, 2, widget);
    table->setHorizontalHeaderLabels({"Клавиша", "Действие"});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->setStyleSheet(QString(
        "QTableWidget { border: 1px solid %1; border-radius: 8px; background-color: %2; }"
        "QTableWidget::item { padding: 8px 14px; color: %3; }"
        "QTableWidget::item:selected { background-color: %4; color: %5; }"
        ).arg(tm.borderColor(), tm.cardBackground(), tm.textColor(),
              tm.currentTheme() == ThemeManager::Dark ? "#2A3F3F" : "#e8edf0",
              tm.accentColor()));

    QList<QPair<QString, QString>> shortcuts = {
        {"Ctrl+N", "Новое изделие"},
        {"Ctrl+Shift+N", "Новая папка"},
        {"Ctrl+F", "Поиск"},
        {"Ctrl+S", "Сохранить базу данных"},
        {"Ctrl+Shift+S", "Создать бэкап"},
        {"Ctrl+M", "Комментарии"},
        {"F5", "Обновить"},
        {"Delete", "Удалить файл/папку"},
        {"Esc", "Закрыть превью/уведомления"},
        {"F1", "Помощь"},
        {"Home", "Главная страница"},
        {"Ctrl+W", "Закрыть окно"},
        {"Ctrl+Tab", "Фокус на таблицу"},
        {"Ctrl+Shift+Tab", "Фокус на дерево"},
        {"Ctrl+Shift+U", "Открыть уведомления"},
        {"Ctrl+Shift+P", "Показать/скрыть превью"},
        {"Ctrl+Alt+O", "Открыть папку в проводнике"},
        {"Ctrl+Shift+F", "Расширенный поиск"},
        {"Ctrl+A", "Выделить все файлы"},
        {"Ctrl+Z", "Отменить (в разработке)"}
    };

    for (int i = 0; i < shortcuts.size(); ++i) {
        QTableWidgetItem* keyItem = new QTableWidgetItem(shortcuts[i].first);
        keyItem->setFont(QFont("Segoe UI", 11, QFont::Bold));
        keyItem->setForeground(QColor(tm.accentColor()));
        table->setItem(i, 0, keyItem);

        QTableWidgetItem* actionItem = new QTableWidgetItem(shortcuts[i].second);
        actionItem->setFont(QFont("Segoe UI", 11));
        table->setItem(i, 1, actionItem);
    }

    layout->addWidget(table);
    return widget;
}

// ===== ВКЛАДКА "ПОМОЩЬ" =====
QWidget* AboutDialog::createHelpTab()
{
    ThemeManager& tm = ThemeManager::instance();

    QWidget* widget = new QWidget();
    widget->setObjectName("helpTab");

    QVBoxLayout* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(20, 15, 20, 15);
    layout->setSpacing(12);

    QLabel* titleLabel = new QLabel("❓ Часто задаваемые вопросы", widget);
    titleLabel->setStyleSheet(QString("QLabel { font-size: 18px; font-weight: 600; color: %1; font-family: 'Segoe UI', 'Arial', sans-serif; background: transparent; }").arg(tm.textColor()));
    layout->addWidget(titleLabel);

    QScrollArea* scrollArea = new QScrollArea(widget);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    QWidget* contentWidget = new QWidget(scrollArea);
    QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setSpacing(12);
    contentLayout->setContentsMargins(0, 0, 0, 0);

    struct FAQ { QString question; QString answer; };
    QList<FAQ> faqs = {
        {"Как добавить файлы?", "Нажмите кнопку «Файлы» на панели инструментов или перетащите файлы в окно программы."},
        {"Как открыть чертёж?", "Двойной клик по файлу чертежа (.cdw, .frw) откроет его в КОМПАС-3D или Viewer."},
        {"Что делать, если не видно папку?", "Нажмите F5 или кнопку «Обновить» для перезагрузки базы данных."},
        {"Как создать сборку?", "Нажмите правой кнопкой мыши на изделии и выберите «Создать СБ»."},
        {"Как удалить изделие?", "Нажмите правой кнопкой мыши на изделии и выберите «Удалить». Потребуется ввод PIN-кода 5689."},
        {"Где хранятся файлы?", "Файлы хранятся в папке, указанной при добавлении. Сама база данных — в файле kd.db."},
        {"Как сделать бэкап?", "Нажмите кнопку «Бэкап» на панели инструментов — создаётся резервная копия базы данных."},
        {"Как обновить базу данных?", "Нажмите кнопку «Обновить» или F5 для перезагрузки базы данных."},
        {"Где находится база данных?", "База данных находится в папке с программой: kd.db. Путь можно посмотреть в настройках."},
        {"Как восстановить поврежденную БД?", "Программа автоматически проверяет целостность БД при запуске и восстанавливает из последнего бэкапа."},
        {"Как найти файл?", "Нажмите кнопку «Поиск» на панели инструментов или Ctrl+F, введите имя файла."},
        {"Что такое статус документа?", "Статус показывает этап работы над документом: «В работе», «На проверке», «Утвержден» и т.д."},
        {"Как назначить ответственного?", "Откройте комментарии к файлу (правый клик → «Обсуждение»). Нажмите кнопку «Назначить» и введите имя."}
    };

    for (const FAQ& faq : faqs) {
        QGroupBox* group = new QGroupBox("❓ " + faq.question, contentWidget);
        group->setStyleSheet(QString(
            "QGroupBox { font-weight: 600; color: %1; font-family: 'Segoe UI', 'Arial', sans-serif; border: 1px solid %2; border-radius: 8px; margin-top: 12px; padding-top: 10px; background-color: %3; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; background-color: %3; color: %1; }"
            ).arg(tm.textColor(), tm.borderColor(), tm.cardBackground()));

        QVBoxLayout* groupLayout = new QVBoxLayout(group);
        QLabel* answerLabel = new QLabel(faq.answer, group);
        answerLabel->setWordWrap(true);
        answerLabel->setStyleSheet(QString("QLabel { font-size: 13px; color: %1; font-family: 'Segoe UI', 'Arial', sans-serif; font-weight: 400; padding: 4px 8px; background: transparent; }").arg(tm.textColor()));
        groupLayout->addWidget(answerLabel);
        contentLayout->addWidget(group);
    }

    scrollArea->setWidget(contentWidget);
    layout->addWidget(scrollArea);
    return widget;
}
void AboutDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    applyLightTheme();
    updateDatabaseInfo();
}
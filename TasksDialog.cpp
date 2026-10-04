#include "TasksDialog.h"
#include "DatabaseManager.h"
#include "CommentDialog.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDateTime>
#include <algorithm>
#include <QMessageBox>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QRegularExpression>

TasksDialog::TasksDialog(const QString& currentUser, DatabaseManager* db, QWidget* parent)
    : QDialog(parent), m_currentUser(currentUser), m_db(db)
{
    setWindowTitle("📋 Задачи");
    setGeometry(100, 100, 900, 600);
    setMinimumSize(800, 500);

    qDebug() << "\n✅ TasksDialog открыта для:" << m_currentUser;

    // ===== ПРОВЕРКА СИНХРОНИЗАЦИИ С БД (v0.21) =====
    if (m_db) {
        QString activeUser = m_db->getCurrentActiveUserFIO();
        if (!activeUser.isEmpty() && activeUser != m_currentUser) {
            qWarning() << "⚠️ Активный пользователь в БД:" << activeUser
                       << "но TasksDialog открыта для:" << m_currentUser;
        }
    }

    setupUI();
    loadAndDisplay();
    applyTheme();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &TasksDialog::applyTheme);
    connect(m_db, &DatabaseManager::assignmentChanged, this, &TasksDialog::onAssignmentChanged);
}

TasksDialog::~TasksDialog()
{
}

void TasksDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QLabel* titleLabel = new QLabel("📋 Мои задачи", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    QLabel* userLabel = new QLabel("Задачи ответственного: " + m_currentUser, this);
    userLabel->setStyleSheet("color: #666; font-size: 10pt; padding: 2px;");
    mainLayout->addWidget(userLabel);

    QGroupBox* searchGroup = new QGroupBox("Поиск", this);
    QHBoxLayout* searchLayout = new QHBoxLayout(searchGroup);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Введите имя файла или ответственного...");
    searchLayout->addWidget(m_searchEdit);

    searchLayout->addStretch();
    mainLayout->addWidget(searchGroup);

    m_taskList = new QListWidget(this);
    ThemeManager& tm = ThemeManager::instance();
    QString bgColor = tm.backgroundColor();
    QString textColor = tm.textColor();
    QString hoverColor = tm.hoverColor();
    QString accentColor = tm.accentColor();

    m_taskList->setStyleSheet(
        QString(
            "QListWidget { border: 1px solid #ccc; border-radius: 4px; background-color: %1; color: %2; } "
            "QListWidget::item { padding: 10px; color: %2; } "
            "QListWidget::item:selected { background-color: %3; color: %2; } "
            "QListWidget::item:hover { background-color: %4; color: %2; }"
        ).arg(bgColor, textColor, accentColor, hoverColor)
    );
    mainLayout->addWidget(m_taskList, 1);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setStyleSheet("color: #666; font-size: 9pt; padding: 5px;");
    mainLayout->addWidget(m_infoLabel);

    QHBoxLayout* buttonLayout = new QHBoxLayout();

    m_openBtn = new QPushButton("📄 Открыть", this);
    m_openBtn->setEnabled(false);

    m_refreshBtn = new QPushButton("🔄 Обновить", this);

    QPushButton* closeBtn = new QPushButton("Закрыть", this);

    buttonLayout->addWidget(m_openBtn);
    buttonLayout->addWidget(m_refreshBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeBtn);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);

    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &TasksDialog::loadAndDisplay);
    connect(m_taskList, &QListWidget::itemDoubleClicked,
            this, &TasksDialog::onTaskDoubleClicked);
    connect(m_taskList, &QListWidget::itemClicked,
            this, &TasksDialog::onTaskClicked);
    connect(m_openBtn, &QPushButton::clicked,
            this, &TasksDialog::openTask);
    connect(m_refreshBtn, &QPushButton::clicked,
            this, &TasksDialog::loadAndDisplay);
    connect(closeBtn, &QPushButton::clicked,
            this, &QDialog::accept);
}

bool TasksDialog::matchesCurrentUser(const QString& responsible) const
{
    if (responsible.isEmpty() || m_currentUser.isEmpty()) {
        return false;
    }

    QString resp = responsible.trimmed().toLower();
    QString current = m_currentUser.trimmed().toLower();

    if (resp == current) return true;

    QRegularExpression re("\\s+");
    QStringList respParts = resp.split(re, Qt::SkipEmptyParts);
    QStringList currentParts = current.split(re, Qt::SkipEmptyParts);

    for (const QString& part : respParts) {
        for (const QString& curPart : currentParts) {
            if (part.startsWith(curPart) || curPart.startsWith(part)) {
                return true;
            }
        }
    }

    return false;
}

void TasksDialog::loadAndDisplay()
{
    m_taskList->clear();
    m_tasks.clear();

    const QMap<QString, DocInfo>& docs = m_db->documents();

    qDebug() << "\n📊 Всего документов в БД:" << docs.count();

    if (!m_db->getDatabase().isOpen()) {
        qDebug() << "❌ БД не открыта";
        return;
    }

    // ДИАГНОСТИКА: проверим таблицу assignments и documents
    {
        QSqlQuery diagQuery(m_db->getDatabase());
        if (diagQuery.exec("SELECT COUNT(*) FROM assignments")) {
            if (diagQuery.next()) {
                int assignCount = diagQuery.value(0).toInt();
                qDebug() << "📊 Всего записей в таблице assignments:" << assignCount;
            }
        } else {
            qDebug() << "❌ Ошибка запроса assignments:" << diagQuery.lastError().text();
        }

        // Также проверим документы с responsible_user
        diagQuery.exec("SELECT COUNT(*) FROM documents WHERE responsible_user IS NOT NULL AND responsible_user != ''");
        if (diagQuery.next()) {
            int respCount = diagQuery.value(0).toInt();
            qDebug() << "📊 Документов с responsible_user в БД:" << respCount;
        }
    }

    int myTasksCount = 0;
    int totalProcessed = 0;

    for (auto it = docs.begin(); it != docs.end(); ++it) {
        const DocInfo& doc = it.value();

        if (doc.responsibleUser.isEmpty()) {
            continue;
        }

        totalProcessed++;

        // ✅ КРИТИЧЕСКИЙ ФИЛЬТР: показываем только задачи ТЕКУЩЕГО пользователя
        if (doc.responsibleUser != m_currentUser) {
            qDebug() << "⏭️ Пропускаем (не текущий пользователь):" << doc.name
                     << "→ ответственный:" << doc.responsibleUser
                     << "(текущий:" << m_currentUser << ")";
            continue;
        }

        QString docKey = doc.folderPath + "|" + doc.path;

        bool isManual = true;  // По умолчанию считаем ручным, если есть ответственный
        {
            QSqlQuery query(m_db->getDatabase());
            query.prepare("SELECT COUNT(*) FROM assignments WHERE doc_key = :doc_key AND assigned_to = :assigned_to");
            query.bindValue(":doc_key", docKey);
            query.bindValue(":assigned_to", doc.responsibleUser);

            if (query.exec() && query.next()) {
                int assignmentCount = query.value(0).toInt();
                if (assignmentCount > 0) {
                    // Есть запись в assignments - проверим is_manual
                    QSqlQuery detailQuery(m_db->getDatabase());
                    detailQuery.prepare("SELECT is_manual FROM assignments WHERE doc_key = :doc_key AND assigned_to = :assigned_to ORDER BY assigned_date DESC LIMIT 1");
                    detailQuery.bindValue(":doc_key", docKey);
                    detailQuery.bindValue(":assigned_to", doc.responsibleUser);

                    if (detailQuery.exec() && detailQuery.next()) {
                        isManual = detailQuery.value(0).toBool();
                        qDebug() << "✅ doc_key=" << docKey << ", assigned_to=" << doc.responsibleUser << ", is_manual=" << isManual;
                    }
                } else {
                    // Нет записи в assignments, но есть responsible_user в документе
                    // Это значит что назначение было сделано давно или программой
                    qDebug() << "⚠️ Нет записи в assignments для doc_key=" << docKey << ", считаем ручным";
                    isManual = true;  // Если в документе есть ответственный, показываем его
                }
            } else {
                qDebug() << "❌ Ошибка запроса assignments:" << query.lastError().text();
                isManual = true;  // На ошибку - показываем
            }
        }

        if (!isManual) {
            qDebug() << "⏭️ Пропускаем неручное назначение: doc_key=" << docKey;
            continue;
        }

        myTasksCount++;

        QString status = doc.status.isEmpty() ? "Новый" : doc.status;

        Task task;
        task.filePath = doc.path;
        task.fileName = doc.name;
        task.folderPath = doc.folderPath;
        task.status = status;
        task.responsible = doc.responsibleUser;
        task.modified = doc.modified;
        task.isManual = isManual;

        int lastSlash = task.folderPath.lastIndexOf('/');
        task.folderName = (lastSlash >= 0) ? task.folderPath.mid(lastSlash + 1) : task.folderPath;

        m_tasks.append(task);
    }

    qDebug() << "✅ Загружено задач ДЛЯ текущего пользователя:" << myTasksCount << "из" << totalProcessed << "с назначениями";

    QString searchText = m_searchEdit->text().toLower();
    QList<Task> toDisplay;

    for (const Task& task : m_tasks) {
        bool show = true;

        if (!searchText.isEmpty()) {
            show = task.fileName.toLower().contains(searchText) ||
                   task.folderName.toLower().contains(searchText) ||
                   task.responsible.toLower().contains(searchText);
        }

        if (show) {
            toDisplay.append(task);
        }
    }

    std::sort(toDisplay.begin(), toDisplay.end(), [](const Task& a, const Task& b) {
        return a.modified > b.modified;
    });

    for (const Task& task : toDisplay) {
        QString icon = getStatusIcon(task.status);

        QString text = QString("%1 %2\n📁 %3 | 👤 %4 | %5")
            .arg(icon, task.fileName, task.folderName, task.responsible, task.status);

        QListWidgetItem* item = new QListWidgetItem(text);
        item->setData(Qt::UserRole, task.filePath);
        item->setSizeHint(QSize(0, 70));
        m_taskList->addItem(item);
    }

    // ✅ Счётчик показывает только отфильтрованные задачи (так как фильтр уже применён выше)
    QString info = QString("Всего задач для вас: %1").arg(toDisplay.count());
    m_infoLabel->setText(info);

    qDebug() << "📋 Показано в списке:" << toDisplay.count() << "задач для" << m_currentUser;
}

QString TasksDialog::getStatusIcon(const QString& status) const
{
    if (status.contains("Новый", Qt::CaseInsensitive)) return "⚪";
    if (status.contains("Утверждён", Qt::CaseInsensitive) || status.contains("Утвержден", Qt::CaseInsensitive)) return "✅";
    if (status.contains("В работе", Qt::CaseInsensitive)) return "🔄";
    if (status.contains("На проверке", Qt::CaseInsensitive)) return "👀";
    if (status.contains("Требует", Qt::CaseInsensitive)) return "🔧";
    if (status.contains("Архив", Qt::CaseInsensitive)) return "📦";
    return "❓";
}

void TasksDialog::onTaskClicked(QListWidgetItem* item)
{
    m_openBtn->setEnabled(item != nullptr);
}

void TasksDialog::onTaskDoubleClicked(QListWidgetItem* item)
{
    if (item) openTask();
}

void TasksDialog::openTask()
{
    QListWidgetItem* item = m_taskList->currentItem();
    if (!item) return;

    QString filePath = item->data(Qt::UserRole).toString();

    for (const Task& task : m_tasks) {
        if (task.filePath == filePath) {
            CommentDialog* dialog = new CommentDialog(task.filePath, task.folderPath, m_db, this);
            dialog->exec();
            delete dialog;
            loadAndDisplay();
            break;
        }
    }
}

void TasksDialog::applyTheme()
{
    ThemeManager& tm = ThemeManager::instance();
    QString bg = tm.backgroundColor();
    QString fg = tm.textColor();
    QString border = tm.borderColor();

    setStyleSheet(QString(
        "QDialog { background-color: %1; color: %2; }"
        "QLabel { color: %2; }"
        "QComboBox { background-color: %1; color: %2; border: 1px solid %3; padding: 5px; }"
        "QLineEdit { background-color: %1; color: %2; border: 1px solid %3; padding: 5px; }"
        "QPushButton { background-color: #2196F3; color: white; border: none; padding: 6px 12px; border-radius: 3px; font-weight: bold; }"
        "QPushButton:hover { background-color: #1976D2; }"
        "QPushButton:pressed { background-color: #1565C0; }"
        "QGroupBox { color: %2; border: 1px solid %3; border-radius: 4px; padding-top: 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px 0 3px; }"
    ).arg(bg, fg, border));
}

void TasksDialog::onAssignmentChanged(const QString& docKey, const QString& assignedTo)
{
    qDebug() << "🔄 TasksDialog::onAssignmentChanged вызван для docKey=" << docKey << ", assigned=" << assignedTo;

    if (!m_db || m_db->documents().isEmpty()) {
        qDebug() << "⚠️ БД пуста, пропускаем обновление";
        return;
    }

    // Обновляем список задач
    QTimer::singleShot(100, this, &TasksDialog::loadAndDisplay);
}

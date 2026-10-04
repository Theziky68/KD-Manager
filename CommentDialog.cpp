#include "CommentDialog.h"
#include "DatabaseManager.h"
#include "NotificationManager.h"
#include "UserConfig.h"
#include "ThemeManager.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QFileInfo>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QFrame>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QScrollArea>
#include <QEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QSplitter>
#include <QMenu>
#include <QDebug>

CommentDialog::CommentDialog(const QString& filePath, const QString& folderPath,
                             DatabaseManager* db, QWidget* parent)
    : QDialog(parent)
    , m_filePath(filePath)
    , m_folderPath(folderPath)
    , m_db(db)
    , m_currentStatus(STATUS_IN_PROGRESS)
    , m_currentCommentId(0)
    , m_selectedCommentId(-1)
{
    setWindowTitle("Комментарии: " + QFileInfo(filePath).fileName());
    resize(1200, 700);

    setupUI();
    loadStatusAndResponsible();
    loadComments();
    updateStatusDisplay();
    applyTheme();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &CommentDialog::applyTheme);
}

CommentDialog::~CommentDialog()
{
}

void CommentDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    // Заголовок с файлом и метаданными
    QHBoxLayout* headerLayout = new QHBoxLayout();
    QLabel* fileLabel = new QLabel("📄 " + QFileInfo(m_filePath).fileName());
    fileLabel->setStyleSheet("font-weight: bold; font-size: 14px; border: none;");
    headerLayout->addWidget(fileLabel);
    headerLayout->addStretch();

    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet("font-size: 11px; border: none;");
    headerLayout->addWidget(m_statusLabel);

    m_responsibleLabel = new QLabel();
    m_responsibleLabel->setStyleSheet("font-size: 11px; border: none;");
    headerLayout->addWidget(m_responsibleLabel);

    mainLayout->addLayout(headerLayout);

    // Кнопки действий
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    m_statusButton = new QPushButton("🔄 Статус");
    connect(m_statusButton, &QPushButton::clicked, this, &CommentDialog::changeStatus);
    buttonLayout->addWidget(m_statusButton);

    m_assignButton = new QPushButton("👤 Назначить");
    connect(m_assignButton, &QPushButton::clicked, this, &CommentDialog::assignUser);
    buttonLayout->addWidget(m_assignButton);

    QPushButton* clearButton = new QPushButton("❌ Удалить назначение");
    connect(clearButton, &QPushButton::clicked, this, &CommentDialog::clearResponsible);
    buttonLayout->addWidget(clearButton);

    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    // Разделитель
    QFrame* divider1 = new QFrame();
    divider1->setFrameShape(QFrame::HLine);
    mainLayout->addWidget(divider1);

    // Список комментариев
    QLabel* commentsLabel = new QLabel("Комментарии:");
    commentsLabel->setStyleSheet("font-weight: 600; font-size: 12px;");
    mainLayout->addWidget(commentsLabel);

    m_leftPanel = new QWidget();
    QVBoxLayout* leftLayout = new QVBoxLayout(m_leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);

    QWidget* scrollWidget = new QWidget();
    m_commentListLayout = new QVBoxLayout(scrollWidget);
    m_commentListLayout->setContentsMargins(0, 0, 0, 0);
    m_commentListLayout->setSpacing(6);

    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidget(scrollWidget);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { border: none; }");

    leftLayout->addWidget(scrollArea);
    mainLayout->addWidget(m_leftPanel, 1);

    // Разделитель
    QFrame* divider2 = new QFrame();
    divider2->setFrameShape(QFrame::HLine);
    mainLayout->addWidget(divider2);

    // Добавить комментарий
    QLabel* inputLabel = new QLabel("Добавить комментарий:");
    inputLabel->setStyleSheet("font-weight: 500; font-size: 11px; border: none;");
    mainLayout->addWidget(inputLabel);

    m_commentEdit = new QTextEdit();
    m_commentEdit->setMaximumHeight(70);
    m_commentEdit->setPlaceholderText("Введите новый комментарий...");
    mainLayout->addWidget(m_commentEdit);

    QHBoxLayout* actionLayout = new QHBoxLayout();
    m_addButton = new QPushButton("➕ Добавить");
    connect(m_addButton, &QPushButton::clicked, this, &CommentDialog::addComment);
    actionLayout->addStretch();
    actionLayout->addWidget(m_addButton);
    mainLayout->addLayout(actionLayout);
}

void CommentDialog::loadComments()
{
    if (!m_db || !m_db->getDatabase().isOpen()) {
        return;
    }

    m_comments.clear();

    QSqlQuery query(m_db->getDatabase());
    // Используем только file_path + folder_path (надежный способ)
    query.prepare("SELECT id, user, text, time, is_reply, reply_to FROM comments WHERE file_path = :fp AND folder_path = :folder_path ORDER BY time ASC");
    query.bindValue(":fp", m_filePath);
    query.bindValue(":folder_path", m_folderPath);

    if (!query.exec()) {
        qDebug() << "❌ Ошибка загрузки комментариев:" << query.lastError().text();
        return;
    }

    qDebug() << "✅ Загружены комментарии для файла:" << m_filePath;

    while (query.next()) {
        CommentEntry entry;
        entry.id = query.value("id").toInt();
        entry.user = query.value("user").toString();
        entry.text = query.value("text").toString();
        entry.time = query.value("time").toDateTime();
        entry.isReply = query.value("is_reply").toBool();
        entry.replyTo = query.value("reply_to").toInt();
        m_comments.append(entry);
    }

    rebuildCommentList();
}

void CommentDialog::rebuildCommentList()
{
    // Очистить старые комментарии
    QLayoutItem* item;
    while ((item = m_commentListLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    m_commentWidgets.clear();

    // Добавить только основные комментарии (не ответы)
    for (const auto& entry : m_comments) {
        if (!entry.isReply) {
            QWidget* commentCard = new QWidget();
            QVBoxLayout* cardLayout = new QVBoxLayout(commentCard);
            cardLayout->setContentsMargins(10, 8, 10, 8);
            cardLayout->setSpacing(4);

            QString textColor = ThemeManager::instance().textColor();
            QString secondaryColor = ThemeManager::instance().secondaryTextColor();
            QString hoverColor = ThemeManager::instance().hoverColor();

            QLabel* author = new QLabel(entry.user);
            author->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 600;").arg(textColor));
            cardLayout->addWidget(author);

            QLabel* text = new QLabel(entry.text);
            text->setWordWrap(true);
            text->setStyleSheet(QString("color: %1; font-size: 10px;").arg(textColor));
            cardLayout->addWidget(text);

            QLabel* time = new QLabel(entry.time.toString("dd.MM hh:mm"));
            time->setStyleSheet(QString("color: %1; font-size: 8px; margin-top: 4px;").arg(secondaryColor));
            cardLayout->addWidget(time);

            commentCard->setStyleSheet(QString("QWidget { background-color: %1; border: none; border-radius: 6px; }").arg(hoverColor));
            commentCard->setCursor(Qt::PointingHandCursor);

            // Клик по карточке
            connect(commentCard, &QWidget::customContextMenuRequested, this, [this, entry](const QPoint&) {
                QMenu menu;
                QAction* deleteAction = menu.addAction("Удалить");
                if (menu.exec(QCursor::pos()) == deleteAction) {
                    deleteCommentWithId(entry.id);
                }
            });
            commentCard->setContextMenuPolicy(Qt::CustomContextMenu);

            m_commentListLayout->addWidget(commentCard);
            m_commentWidgets[entry.id] = commentCard;
        }
    }

    m_commentListLayout->addStretch();
}

void CommentDialog::addComment()
{
    QString text = m_commentEdit->toPlainText().trimmed();
    if (text.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите комментарий");
        return;
    }

    if (!m_db || !m_db->getDatabase().isOpen()) {
        QMessageBox::warning(this, "Ошибка", "БД не открыта");
        return;
    }

    // ===== ИСПРАВЛЕНИЕ v0.15: Привязываем комментарий через doc_key =====
    QString docKey = m_folderPath + "|" + m_filePath;
    QString userName = UserConfig::instance().getUserFIO();

    QSqlQuery query(m_db->getDatabase());
    query.prepare("INSERT INTO comments (file_path, folder_path, doc_key, user, text, time, is_reply, reply_to) "
                  "VALUES (:file_path, :folder_path, :doc_key, :user, :text, :time, :is_reply, :reply_to)");
    query.bindValue(":file_path", m_filePath);
    query.bindValue(":folder_path", m_folderPath);
    query.bindValue(":doc_key", docKey);  // НОВОЕ: добавляем doc_key для надёжной привязки
    query.bindValue(":user", userName);
    query.bindValue(":text", text);
    query.bindValue(":time", QDateTime::currentDateTime());
    query.bindValue(":is_reply", 0);
    query.bindValue(":reply_to", -1);

    if (!query.exec()) {
        QMessageBox::warning(this, "Ошибка", "Не удалось сохранить комментарий");
        qDebug() << "❌ Ошибка добавления комментария:" << query.lastError().text();
        return;
    }

    qDebug() << "✅ Комментарий добавлен для doc_key:" << docKey << "пользователем:" << userName;

    m_commentEdit->clear();
    loadComments();

    // Сохраняем БД после добавления комментария
    m_db->forceSave();
    qDebug() << "✅ БД сохранена после добавления комментария";

    try {
        NotificationManager::Notification notif;
        notif.type = NotificationManager::TYPE_SUCCESS;
        notif.title = "Комментарий добавлен";
        notif.message = QString("Файл: %1").arg(QFileInfo(m_filePath).fileName());
        NotificationManager::instance().addNotification(userName, notif);
    } catch (...) {}
}

void CommentDialog::deleteCommentWithId(int commentId)
{
    if (!m_db || !m_db->getDatabase().isOpen()) {
        return;
    }

    // ===== ИСПРАВЛЕНИЕ v0.15: Удаляем комментарии через doc_key для надёжности =====
    QString docKey = m_folderPath + "|" + m_filePath;

    QSqlQuery query(m_db->getDatabase());
    // Пытаемся удалить через doc_key (надёжный способ)
    query.prepare("DELETE FROM comments WHERE (id = :id OR reply_to = :id) AND doc_key = :doc_key");
    query.bindValue(":id", commentId);
    query.bindValue(":doc_key", docKey);

    if (!query.exec()) {
        qDebug() << "⚠️ Ошибка удаления по doc_key, пробуем старый способ:" << query.lastError().text();
        // Fallback на старый способ для совместимости
        query.prepare("DELETE FROM comments WHERE (id = :id OR reply_to = :id) AND file_path = :fp AND folder_path = :folder_path");
        query.bindValue(":id", commentId);
        query.bindValue(":fp", m_filePath);
        query.bindValue(":folder_path", m_folderPath);

        if (!query.exec()) {
            qDebug() << "❌ Ошибка удаления комментария:" << query.lastError().text();
            return;
        }
    }

    qDebug() << "✅ Комментарий удалён (id=" << commentId << ", doc_key=" << docKey << ")";

    loadComments();

    // Сохраняем БД после удаления комментария
    m_db->forceSave();
    qDebug() << "✅ БД сохранена после удаления комментария";
}

void CommentDialog::changeStatus()
{
    QStringList statuses;
    statuses << "✅ Утвержден" << "🔄 В работе" << "👀 На проверке"
             << "🔧 Требует доработки" << "⚠️ Устарел" << "📦 В архиве";

    bool ok;
    QString statusStr = QInputDialog::getItem(this, "Статус", "Выберите:", statuses, 1, false, &ok);
    if (!ok) return;

    // Преобразуем в текст БД
    QString dbStatus;
    if (statusStr.contains("Утвержден")) dbStatus = "Утвержден";
    else if (statusStr.contains("В работе")) dbStatus = "В работе";
    else if (statusStr.contains("На проверке")) dbStatus = "На проверке";
    else if (statusStr.contains("Требует")) dbStatus = "Требует доработки";
    else if (statusStr.contains("Устарел")) dbStatus = "Устарел";
    else if (statusStr.contains("В архиве")) dbStatus = "В архиве";

    if (dbStatus.isEmpty()) return;

    // Сохраняем старый статус для истории
    QString oldStatus;
    switch(m_currentStatus) {
        case STATUS_APPROVED: oldStatus = "Утвержден"; break;
        case STATUS_IN_PROGRESS: oldStatus = "В работе"; break;
        case STATUS_REVIEW: oldStatus = "На проверке"; break;
        case STATUS_NEEDS_CHANGE: oldStatus = "Требует доработки"; break;
        case STATUS_OBSOLETE: oldStatus = "Устарел"; break;
        case STATUS_ARCHIVED: oldStatus = "В архиве"; break;
        default: oldStatus = "В работе"; break;
    }

    m_currentStatus = DocumentState::stringToStatus(dbStatus);
    updateStatusDisplay();

    // НОВОЕ: вызываем addStatusChange для ведения истории
    m_db->addStatusChange(
        m_folderPath,
        m_filePath,
        oldStatus,
        dbStatus,
        UserConfig::instance().getUserFIO(),
        "Статус изменен через диалог комментариев"
    );

    saveStatusAndResponsible();

    qDebug() << "✅ Статус сохранён:" << dbStatus;
}

void CommentDialog::assignUser()
{
    if (!m_db || !m_db->getDatabase().isOpen()) {
        QMessageBox::warning(this, "Ошибка", "БД не открыта");
        return;
    }

    // Получаем последних онлайн пользователей за день
    QStringList onlineUsers;
    {
        QSqlQuery query(m_db->getDatabase());
        query.prepare(
            "SELECT DISTINCT fio FROM users_online "
            "WHERE last_seen > datetime('now', '-1 day') "
            "ORDER BY last_seen DESC"
        );

        if (query.exec()) {
            while (query.next()) {
                QString fio = query.value(0).toString().trimmed();
                if (!fio.isEmpty()) {
                    onlineUsers.append(fio);
                }
            }
            qDebug() << "✅ Загружено онлайн пользователей за день:" << onlineUsers.count();
        } else {
            qDebug() << "❌ Ошибка запроса users_online:" << query.lastError().text();
        }
    }

    // Если текущий пользователь не в списке - добавим его
    QString currentUser = UserConfig::instance().getUserFIO();
    if (!onlineUsers.contains(currentUser)) {
        onlineUsers.prepend(currentUser);
    }

    if (onlineUsers.isEmpty()) {
        QMessageBox::information(this, "Назначить", "Нет онлайн пользователей");
        return;
    }

    // Показываем диалог выбора
    bool ok;
    QString user = QInputDialog::getItem(
        this,
        "👤 Назначить ответственного",
        "Выберите пользователя или введите ФИО:",
        onlineUsers,
        0,
        true,  // editable - можно вводить свой текст
        &ok
    );

    if (!ok) return;

    QString newUser = user.trimmed();
    if (newUser.isEmpty()) return;

    // Сохраняем старого ответственного для истории
    QString oldUser = m_responsibleUser;

    m_responsibleUser = newUser;
    updateStatusDisplay();

    // НОВОЕ: вызываем addAssignment для ведения истории
    m_db->addAssignment(
        m_folderPath,
        m_filePath,
        newUser,
        UserConfig::instance().getUserFIO(),
        oldUser.isEmpty() ? QString() : oldUser,
        "Назначено через диалог комментариев",
        true  // isManual = true - это ручное назначение
    );

    saveStatusAndResponsible();

    // Отправляем уведомление назначенному пользователю
    NotificationManager::instance().addNotification(
        newUser,
        "👤 Новое назначение",
        QString("Вам назначен файл: %1").arg(QFileInfo(m_filePath).fileName()),
        NotificationManager::TYPE_INFO
    );

    qDebug() << "✅ Ответственный назначен:" << m_responsibleUser << "| Назначил:" << currentUser;
}

void CommentDialog::clearResponsible()
{
    m_responsibleUser = "";
    updateStatusDisplay();

    // Добавляем запись в историю с пустым значением
    m_db->addAssignment(
        m_folderPath,
        m_filePath,
        "",
        UserConfig::instance().getUserFIO(),
        m_responsibleUser,
        "Назначение удалено",
        true
    );

    saveStatusAndResponsible();

    qDebug() << "✅ Ответственный удалён";
}

void CommentDialog::saveStatusAndResponsible()
{
    if (!m_db || !m_db->getDatabase().isOpen()) {
        qDebug() << "❌ БД не открыта";
        return;
    }

    // ===== ИСПРАВЛЕНИЕ v0.16: ОБНОВЛЯЕМ ТОЛЬКО КОНКРЕТНЫЙ ФАЙЛ =====
    // БЕЗ forceSave() который перезаписывает остальные файлы!

    QString statusText;
    switch(m_currentStatus) {
        case STATUS_APPROVED: statusText = "Утвержден"; break;
        case STATUS_IN_PROGRESS: statusText = "В работе"; break;
        case STATUS_REVIEW: statusText = "На проверке"; break;
        case STATUS_NEEDS_CHANGE: statusText = "Требует доработки"; break;
        case STATUS_OBSOLETE: statusText = "Устарел"; break;
        case STATUS_ARCHIVED: statusText = "В архиве"; break;
        default: statusText = "В работе"; break;
    }

    qDebug() << "💾 Обновляем БД напрямую (БЕЗ forceSave): folderPath=" << m_folderPath << ", path=" << m_filePath
             << ", status=" << statusText << ", responsible=" << m_responsibleUser;

    QString docKey = m_folderPath + "|" + m_filePath;

    // ===== ОБНОВЛЯЕМ ПАМЯТЬ =====
    if (m_db->documents().contains(docKey)) {
        m_db->documents()[docKey].status = statusText;
        m_db->documents()[docKey].responsibleUser = m_responsibleUser;
        qDebug() << "✅ Обновлена память для:" << docKey;
    }

    // ===== ОБНОВЛЯЕМ БД НАПРЯМУЮ (ТОЛЬКО КОНКРЕТНЫЙ ФАЙЛ) =====
    // Сначала статус
    QSqlQuery statusQuery(m_db->getDatabase());
    statusQuery.prepare("UPDATE documents SET status = :status WHERE folder_path = :fp AND path = :p");
    statusQuery.bindValue(":status", statusText);
    statusQuery.bindValue(":fp", m_folderPath);
    statusQuery.bindValue(":p", m_filePath);

    if (!statusQuery.exec()) {
        qDebug() << "❌ Ошибка обновления статуса:" << statusQuery.lastError().text();
        return;
    }
    qDebug() << "✅ Статус обновлён в БД";

    // Потом ответственный (ТОЛЬКО если не пустой!)
    if (!m_responsibleUser.isEmpty()) {
        QSqlQuery respQuery(m_db->getDatabase());
        respQuery.prepare("UPDATE documents SET responsible_user = :resp WHERE folder_path = :fp AND path = :p");
        respQuery.bindValue(":resp", m_responsibleUser);
        respQuery.bindValue(":fp", m_folderPath);
        respQuery.bindValue(":p", m_filePath);

        if (!respQuery.exec()) {
            qDebug() << "❌ Ошибка обновления ответственного:" << respQuery.lastError().text();
            return;
        }
        qDebug() << "✅ Ответственный обновлён в БД";
    }

    // ===== КРИТИЧЕСКОЕ: СОХРАНЯЕМ В БД =====
    m_db->forceSave();  // Записываем все изменения на диск

    // ===== ОБНОВЛЯЕМ ПАМЯТЬ (DocInfo) =====
    if (m_db->documents().contains(docKey)) {
        m_db->documents()[docKey].responsibleUser = m_responsibleUser;
        m_db->documents()[docKey].status = statusText;
        qDebug() << "✅ Обновлена память DocInfo:" << docKey;
    }

    qDebug() << "✅ БД обновлена и сохранена на диск";
}

void CommentDialog::loadStatusAndResponsible()
{
    if (!m_db || !m_db->getDatabase().isOpen()) {
        qDebug() << "❌ БД не открыта при загрузке статуса";
        return;
    }

    qDebug() << "🔍 Загружаем статус для: folderPath=" << m_folderPath << ", path=" << m_filePath;

    // Загружаем со статус из documents по полному ключу
    QSqlQuery query(m_db->getDatabase());
    query.prepare("SELECT status, responsible_user FROM documents WHERE folder_path = :fp AND path = :p LIMIT 1");
    query.bindValue(":fp", m_folderPath);
    query.bindValue(":p", m_filePath);

    if (!query.exec()) {
        qDebug() << "❌ Ошибка запроса:" << query.lastError().text();
        return;
    }

    if (query.next()) {
        QString statusStr = query.value(0).toString();
        m_responsibleUser = query.value(1).toString();

        if (!statusStr.isEmpty()) {
            m_currentStatus = DocumentState::stringToStatus(statusStr);
            qDebug() << "✅ Загружен статус:" << statusStr;
        }
        if (!m_responsibleUser.isEmpty()) {
            qDebug() << "✅ Загружен ответственный:" << m_responsibleUser;
        }
    } else {
        qDebug() << "ℹ️ Документ не найден в БД";
    }
}

void CommentDialog::updateStatusDisplay()
{
    DocumentState state;
    state.status = m_currentStatus;
    m_statusLabel->setText("Статус: " + state.statusToString());
    m_responsibleLabel->setText("Ответств: " + (m_responsibleUser.isEmpty() ? "Не назначен" : m_responsibleUser));
}

void CommentDialog::applyTheme()
{
    ThemeManager& tm = ThemeManager::instance();
    QString bgColor = tm.backgroundColor();
    QString textColor = tm.textColor();
    QString cardBg = tm.cardBackground();
    QString borderColor = tm.borderColor();

    setStyleSheet(
        QString(
            "QDialog { background-color: %1; color: %2; } "
            "QLabel { color: %2; } "
            "QTextEdit { background-color: %3; border: 1px solid %4; border-radius: 6px; color: %2; padding: 6px; } "
            "QPushButton { background-color: #2196F3; color: white; border: none; padding: 6px 12px; border-radius: 3px; font-weight: bold; } "
            "QPushButton:hover { background-color: #1976D2; }"
        ).arg(bgColor, textColor, cardBg, borderColor)
    );

    m_statusLabel->setStyleSheet(QString("font-size: 11px; border: none; color: %1;").arg(textColor));
    m_responsibleLabel->setStyleSheet(QString("font-size: 11px; border: none; color: %1;").arg(textColor));

    rebuildCommentList();
}

void CommentDialog::keyPressEvent(QKeyEvent* event)
{
    if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_Return) {
        addComment();
    } else {
        QDialog::keyPressEvent(event);
    }
}

void CommentDialog::addReply() {}
void CommentDialog::deleteComment() {}
void CommentDialog::onCommentSelected(int commentId) { Q_UNUSED(commentId); }
void CommentDialog::showReplies(int commentId) { Q_UNUSED(commentId); }
void CommentDialog::setStatus(DocumentStatus status) { m_currentStatus = status; updateStatusDisplay(); }
void CommentDialog::setResponsibleUser(const QString& user) { m_responsibleUser = user; updateStatusDisplay(); }

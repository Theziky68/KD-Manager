#include "FilterDialog.h"
#include "DatabaseManager.h"
#include "ThemeManager.h"
#include <QSqlQuery>
#include <QMessageBox>
#include <QSet>

FilterDialog::FilterDialog(DatabaseManager* db, QWidget* parent)
    : QDialog(parent), m_db(db)
{
    setWindowTitle("Фильтр по статусу и ответственному");
    resize(400, 150);
    setupUI();
    loadResponsibleUsers();
    applyTheme();
}

FilterDialog::~FilterDialog()
{
}

void FilterDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Статус
    QHBoxLayout* statusLayout = new QHBoxLayout();
    QLabel* statusLabel = new QLabel("Статус:");
    m_statusCombo = new QComboBox();
    m_statusCombo->addItem("(Все)", "");
    m_statusCombo->addItem("Утвержден", "Утвержден");
    m_statusCombo->addItem("В работе", "В работе");
    m_statusCombo->addItem("На проверке", "На проверке");
    m_statusCombo->addItem("Требует доработки", "Требует доработки");
    m_statusCombo->addItem("Устарел", "Устарел");
    m_statusCombo->addItem("В архиве", "В архиве");
    statusLayout->addWidget(statusLabel);
    statusLayout->addWidget(m_statusCombo);
    mainLayout->addLayout(statusLayout);

    // Ответственный
    QHBoxLayout* respLayout = new QHBoxLayout();
    QLabel* respLabel = new QLabel("Ответственный:");
    m_responsibleCombo = new QComboBox();
    m_responsibleCombo->addItem("(Все)", "");
    respLayout->addWidget(respLabel);
    respLayout->addWidget(m_responsibleCombo);
    mainLayout->addLayout(respLayout);

    // Кнопки
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* applyBtn = new QPushButton("Применить");
    QPushButton* clearBtn = new QPushButton("Очистить");
    connect(applyBtn, &QPushButton::clicked, this, &FilterDialog::onApplyFilter);
    connect(clearBtn, &QPushButton::clicked, this, &FilterDialog::onClearFilter);
    buttonLayout->addWidget(applyBtn);
    buttonLayout->addWidget(clearBtn);
    mainLayout->addLayout(buttonLayout);
}

void FilterDialog::loadResponsibleUsers()
{
    QSet<QString> users;

    // Загружаем из documents
    QSqlQuery q1(m_db->getDatabase());
    q1.prepare("SELECT DISTINCT responsible_user FROM documents WHERE responsible_user != ''");
    if (q1.exec()) {
        while (q1.next()) {
            QString user = q1.value(0).toString();
            if (!user.isEmpty()) {
                users.insert(user);
            }
        }
    }

    // Загружаем из folders
    QSqlQuery q2(m_db->getDatabase());
    q2.prepare("SELECT DISTINCT responsible_user FROM folders WHERE responsible_user != ''");
    if (q2.exec()) {
        while (q2.next()) {
            QString user = q2.value(0).toString();
            if (!user.isEmpty()) {
                users.insert(user);
            }
        }
    }

    // Добавляем в комбобокс
    for (const QString& user : users) {
        m_responsibleCombo->addItem(user, user);
    }
}

QString FilterDialog::getSelectedStatus() const
{
    return m_statusCombo->currentData().toString();
}

QString FilterDialog::getSelectedResponsible() const
{
    return m_responsibleCombo->currentData().toString();
}

void FilterDialog::onApplyFilter()
{
    QString status = getSelectedStatus();
    QString responsible = getSelectedResponsible();
    emit filterApplied(status, responsible);
    accept();
}

void FilterDialog::onClearFilter()
{
    m_statusCombo->setCurrentIndex(0);
    m_responsibleCombo->setCurrentIndex(0);
    emit filterCleared();
    accept();
}

void FilterDialog::applyTheme()
{
    ThemeManager& tm = ThemeManager::instance();
    QString bgColor = tm.backgroundColor();
    QString textColor = tm.textColor();

    setStyleSheet(
        QString(
            "QDialog { background-color: %1; color: %2; } "
            "QLabel { color: %2; } "
            "QComboBox { background-color: %1; color: %2; border: 1px solid #ccc; padding: 4px; } "
            "QPushButton { background-color: #6366F1; color: white; border: none; padding: 6px 12px; border-radius: 4px; } "
            "QPushButton:hover { background-color: #4F46E5; }"
        ).arg(bgColor, textColor)
    );
}

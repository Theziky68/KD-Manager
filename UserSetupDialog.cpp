#include "UserSetupDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFont>

UserSetupDialog::UserSetupDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Первый запуск - Введите ваши данные");
    setModal(true);
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    setMinimumWidth(400);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QLabel* titleLabel = new QLabel("Добро пожаловать в Менеджер документов!");
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    QLabel* infoLabel = new QLabel("Пожалуйста, введите ваше ФИО для комментариев и уведомлений:");
    mainLayout->addWidget(infoLabel);

    m_fioInput = new QLineEdit();
    m_fioInput->setPlaceholderText("Иванов Иван Иванович");
    mainLayout->addWidget(m_fioInput);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* okBtn = new QPushButton("Продолжить");
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, this, &UserSetupDialog::onAccept);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okBtn);

    mainLayout->addLayout(buttonLayout);

    m_fioInput->setFocus();
}

QString UserSetupDialog::getUserFIO() const
{
    return m_fioInput->text();
}

void UserSetupDialog::onAccept()
{
    if (m_fioInput->text().trimmed().isEmpty()) {
        m_fioInput->setFocus();
        return;
    }
    accept();
}

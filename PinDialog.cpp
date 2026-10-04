#include "PinDialog.h"
#include <QMessageBox>
#include <QTimer>

const QString PinDialog::CORRECT_PIN = "5689";

PinDialog::PinDialog(const QString& message, QWidget *parent)
    : QDialog(parent)
    , m_pinCorrect(false)
    , m_attempts(0)
{
    setupUI();
    m_messageLabel->setText(message);
    setWindowTitle("🔒 Подтверждение удаления");
    setModal(true);
    setFixedSize(350, 180);

    // Фокус на поле ввода
    m_pinEdit->setFocus();
}

void PinDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // Сообщение
    m_messageLabel = new QLabel("Введите PIN-код для подтверждения удаления:", this);
    m_messageLabel->setStyleSheet(
        "QLabel {"
        "   color: #37474F;"
        "   font-size: 13px;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
        );
    m_messageLabel->setWordWrap(true);
    mainLayout->addWidget(m_messageLabel);

    // Поле ввода PIN
    m_pinEdit = new QLineEdit(this);
    m_pinEdit->setPlaceholderText("Введите PIN-код (4 цифры)");
    m_pinEdit->setEchoMode(QLineEdit::Password);
    m_pinEdit->setMaxLength(4);
    m_pinEdit->setAlignment(Qt::AlignCenter);
    m_pinEdit->setStyleSheet(
        "QLineEdit {"
        "   background-color: #f8f8f8;"
        "   border: 2px solid #d0d0d0;"
        "   border-radius: 8px;"
        "   padding: 8px 12px;"
        "   font-size: 18px;"
        "   font-weight: bold;"
        "   color: #263238;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
        "QLineEdit:focus {"
        "   border: 2px solid #607D8B;"
        "}"
        );
    mainLayout->addWidget(m_pinEdit);

    // Ошибка
    m_errorLabel = new QLabel(this);
    m_errorLabel->setStyleSheet(
        "QLabel {"
        "   color: #e53935;"
        "   font-size: 11px;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
        );
    m_errorLabel->setVisible(false);
    mainLayout->addWidget(m_errorLabel);

    // Кнопки
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);

    m_cancelBtn = new QPushButton("Отмена", this);
    m_cancelBtn->setStyleSheet(
        "QPushButton {"
        "   background: transparent;"
        "   color: #607D8B;"
        "   border: 1px solid #607D8B;"
        "   border-radius: 6px;"
        "   padding: 8px 20px;"
        "   font-size: 13px;"
        "   font-weight: 500;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
        "QPushButton:hover {"
        "   background: rgba(96,125,139,0.1);"
        "}"
        );
    connect(m_cancelBtn, &QPushButton::clicked, this, &PinDialog::onCancelClicked);
    buttonLayout->addWidget(m_cancelBtn);

    buttonLayout->addStretch();

    m_okBtn = new QPushButton("✅ Подтвердить", this);
    m_okBtn->setStyleSheet(
        "QPushButton {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                               stop:0 #e53935, stop:1 #c62828);"
        "   color: white;"
        "   border: none;"
        "   border-radius: 6px;"
        "   padding: 8px 20px;"
        "   font-size: 13px;"
        "   font-weight: 600;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
        "QPushButton:hover {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "                               stop:0 #ef5350, stop:1 #d32f2f);"
        "}"
        );
    connect(m_okBtn, &QPushButton::clicked, this, &PinDialog::onOkClicked);
    buttonLayout->addWidget(m_okBtn);

    mainLayout->addLayout(buttonLayout);

    // Обработка нажатия Enter
    connect(m_pinEdit, &QLineEdit::returnPressed, this, &PinDialog::onOkClicked);
}

void PinDialog::onOkClicked()
{
    QString pin = m_pinEdit->text().trimmed();

    if (pin.isEmpty()) {
        m_errorLabel->setText("⚠️ Введите PIN-код");
        m_errorLabel->setVisible(true);
        return;
    }

    if (pin == CORRECT_PIN) {
        m_pinCorrect = true;
        accept();
    } else {
        m_attempts++;
        int remaining = MAX_ATTEMPTS - m_attempts;

        if (remaining <= 0) {
            m_errorLabel->setText("❌ Превышено количество попыток!");
            m_errorLabel->setVisible(true);
            m_okBtn->setEnabled(false);
            m_pinEdit->setEnabled(false);

            QTimer::singleShot(2000, this, [this]() {
                reject();
            });
        } else {
            m_errorLabel->setText(QString("❌ Неверный PIN-код! Осталось попыток: %1").arg(remaining));
            m_errorLabel->setVisible(true);
            m_pinEdit->clear();
            m_pinEdit->setFocus();

            // Подсвечиваем поле красным
            m_pinEdit->setStyleSheet(
                "QLineEdit {"
                "   background-color: #ffebee;"
                "   border: 2px solid #e53935;"
                "   border-radius: 8px;"
                "   padding: 8px 12px;"
                "   font-size: 18px;"
                "   font-weight: bold;"
                "   color: #263238;"
                "   font-family: 'Segoe UI', 'Arial', sans-serif;"
                "}"
                );
        }
    }
}

void PinDialog::onCancelClicked()
{
    m_pinCorrect = false;
    reject();
}
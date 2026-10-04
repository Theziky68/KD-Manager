#ifndef PINDIALOG_H
#define PINDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

class PinDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PinDialog(const QString& message = "Введите PIN-код для подтверждения удаления:", QWidget *parent = nullptr);
    bool isPinCorrect() const { return m_pinCorrect; }

private slots:
    void onOkClicked();
    void onCancelClicked();

private:
    void setupUI();

    QLineEdit* m_pinEdit;
    QPushButton* m_okBtn;
    QPushButton* m_cancelBtn;
    QLabel* m_messageLabel;
    QLabel* m_errorLabel;
    bool m_pinCorrect;
    int m_attempts;
    static const int MAX_ATTEMPTS = 3;
    static const QString CORRECT_PIN;
};

#endif // PINDIALOG_H
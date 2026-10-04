#ifndef USERSETUP_H
#define USERSETUP_H

#include <QDialog>
#include <QLineEdit>

class UserSetupDialog : public QDialog
{
    Q_OBJECT

public:
    UserSetupDialog(QWidget *parent = nullptr);
    QString getUserFIO() const;

private slots:
    void onAccept();

private:
    QLineEdit* m_fioInput;
};

#endif // USERSETUP_H

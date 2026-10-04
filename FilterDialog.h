#ifndef FILTERDIALOG_H
#define FILTERDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

class DatabaseManager;

class FilterDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FilterDialog(DatabaseManager* db, QWidget* parent = nullptr);
    virtual ~FilterDialog();

    QString getSelectedStatus() const;
    QString getSelectedResponsible() const;

private slots:
    void onApplyFilter();
    void onClearFilter();

signals:
    void filterApplied(const QString& status, const QString& responsible);
    void filterCleared();

private:
    void setupUI();
    void loadResponsibleUsers();
    void applyTheme();

    QComboBox* m_statusCombo;
    QComboBox* m_responsibleCombo;
    DatabaseManager* m_db;
};

#endif // FILTERDIALOG_H

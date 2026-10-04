#ifndef TASKSDIALOG_H
#define TASKSDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QDateTime>
#include <QList>

class DatabaseManager;

class TasksDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TasksDialog(const QString& currentUser, DatabaseManager* db, QWidget* parent = nullptr);
    ~TasksDialog();

private slots:
    void loadAndDisplay();
    void onTaskClicked(QListWidgetItem* item);
    void onTaskDoubleClicked(QListWidgetItem* item);
    void openTask();
    void applyTheme();
    void onAssignmentChanged(const QString& docKey, const QString& assignedTo);

private:
    struct Task {
        QString filePath;
        QString folderPath;
        QString folderName;
        QString fileName;
        QString status;
        QString responsible;
        QDateTime modified;
        bool isManual;
    };

    void setupUI();
    QString getStatusIcon(const QString& status) const;
    bool matchesCurrentUser(const QString& responsible) const;

    QString m_currentUser;
    DatabaseManager* m_db;
    QList<Task> m_tasks;

    QComboBox* m_filterCombo;
    QLineEdit* m_searchEdit;
    QListWidget* m_taskList;
    QPushButton* m_openBtn;
    QPushButton* m_refreshBtn;
    QLabel* m_infoLabel;
};

#endif // TASKSDIALOG_H

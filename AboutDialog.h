// AboutDialog.h
#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QTextEdit>
#include <QTableWidget>
#include <QGroupBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QScrollArea>

#include "DatabaseManager.h"

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void setupUI();
    void applyLightTheme();
    QWidget* createAboutTab();
    QWidget* createInfoTab();
    QWidget* createShortcutsTab();
    QWidget* createHelpTab();
    QWidget* createStructureTab();
    void updateDatabaseInfo();

    QTabWidget* m_tabWidget;
    QPushButton* m_closeBtn;

    QLabel* m_dbPathLabel;
    QLabel* m_dbSizeLabel;
    QLabel* m_dbModifiedLabel;
    QLabel* m_dbVersionLabel;
    QLabel* m_dbRecordsLabel;
    QLabel* m_dbStatusLabel;
};

#endif // ABOUTDIALOG_H
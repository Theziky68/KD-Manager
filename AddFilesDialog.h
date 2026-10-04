// AddFilesDialog.h
#ifndef ADDFILESDIALOG_H
#define ADDFILESDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QFileDialog>
#include <QListWidget>
#include <QStringList>

class AddFilesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddFilesDialog(const QString& folderPath, QWidget *parent = nullptr);

    QStringList getSelectedFiles() const { return m_selectedFiles; }
    bool hasFiles() const { return !m_selectedFiles.isEmpty(); }
    QString getResponsible() const;  // БАГ 5: НОВЫЙ МЕТОД

private slots:
    void onAddMultipleFiles();
    void onAddModels();
    void onAddDrawings();
    void onAddOtherFiles();
    void onOkClicked();
    void onCancelClicked();
    void clearFileList();

private:
    void setupUI();
    void applyTheme();
    void addFilesToList(const QStringList& files);
    void updateInfoLabel();

    QString m_folderPath;
    QStringList m_selectedFiles;

    QListWidget* m_filesList;
    QLineEdit* m_modelEdit;
    QLineEdit* m_drawingEdit;
    QLineEdit* m_additionalEdit;
    QLineEdit* m_responsibleEdit;  // БАГ 5: НОВОЕ ПОЛЕ ДЛЯ ОТВЕТСТВЕННОГО
    QPushButton* m_modelBrowseBtn;
    QPushButton* m_drawingBrowseBtn;
    QPushButton* m_additionalBrowseBtn;
    QPushButton* m_okBtn;
    QPushButton* m_cancelBtn;
    QComboBox* m_typeCombo;
    QLabel* m_infoLabel;
};

#endif // ADDFILESDIALOG_H
// AddFilesDialog.cpp
#include "AddFilesDialog.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QFileInfo>
#include <QPalette>
#include <QListWidget>

AddFilesDialog::AddFilesDialog(const QString& folderPath, QWidget *parent)
    : QDialog(parent)
    , m_folderPath(folderPath)
{
    setupUI();
    applyTheme();
    setWindowTitle("➕ Добавить файлы");
    setModal(true);
    resize(700, 450);
}

void AddFilesDialog::applyTheme()
{
    ThemeManager& tm = ThemeManager::instance();
    bool isDark = (tm.currentTheme() == ThemeManager::Dark);

    QString bgColor = isDark ? "#1E1E1E" : "#f0f0f0";
    QString textColor = isDark ? "#E0E0E0" : "#37474F";
    QString cardBg = isDark ? "#2A2A2A" : "#ffffff";
    QString borderColor = isDark ? "#3A3A3A" : "#d0d0d0";
    QString inputBg = isDark ? "#333333" : "#ffffff";
    QString inputText = isDark ? "#E0E0E0" : "#37474F";

    setStyleSheet(QString(
        "QDialog {"
        "   background-color: %1;"
        "}"
        "QWidget {"
        "   background-color: %1;"
        "   color: %2;"
        "}"
        "QLabel {"
        "   color: %2;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "   background: transparent;"
        "}"
        "QGroupBox {"
        "   font-weight: 600;"
        "   color: %2;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "   border: 1px solid %3;"
        "   border-radius: 8px;"
        "   margin-top: 8px;"
        "   padding-top: 10px;"
        "   background-color: %4;"
        "}"
        "QGroupBox::title {"
        "   subcontrol-origin: margin;"
        "   left: 12px;"
        "   padding: 0 6px;"
        "   background-color: %4;"
        "   color: %2;"
        "}"
        "QListWidget {"
        "   background-color: %4;"
        "   border: 1px solid %3;"
        "   border-radius: 6px;"
        "   color: %2;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "   min-height: 60px;"
        "}"
        "QListWidget::item {"
        "   padding: 4px 8px;"
        "}"
        "QListWidget::item:selected {"
        "   background-color: %5;"
        "}"
        "QLineEdit {"
        "   background-color: %5;"
        "   border: 1px solid %3;"
        "   border-radius: 6px;"
        "   padding: 6px 10px;"
        "   font-size: 12px;"
        "   color: %6;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
        "QLineEdit:focus {"
        "   border: 2px solid #0084ff;"
        "}"
        "QLineEdit:disabled {"
        "   background-color: %7;"
        "   color: %8;"
        "}"
        "QPushButton {"
        "   background: #0084ff;"
        "   color: #ffffff;"
        "   border: none;"
        "   border-radius: 6px;"
        "   padding: 8px 20px;"
        "   font-size: 13px;"
        "   font-weight: 600;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "}"
        "QPushButton:hover {"
        "   background: #0073e6;"
        "}"
        "QPushButton:pressed {"
        "   background: #0063cc;"
        "}"
        "QPushButton#okBtn {"
        "   background: #4CAF50;"
        "   color: white;"
        "}"
        "QPushButton#okBtn:hover {"
        "   background: #45a049;"
        "}"
        "QPushButton#cancelBtn {"
        "   background: transparent;"
        "   color: %2;"
        "   border: 1px solid %3;"
        "}"
        "QPushButton#cancelBtn:hover {"
        "   background: %9;"
        "}"
        "QPushButton#clearBtn {"
        "   background: transparent;"
        "   color: #ff3b30;"
        "   border: 1px solid #ff3b30;"
        "   padding: 4px 12px;"
        "   font-size: 11px;"
        "}"
        "QPushButton#clearBtn:hover {"
        "   background: rgba(255,59,48,0.1);"
        "}"
    ).arg(bgColor, textColor, borderColor, cardBg, inputBg, inputText,
          isDark ? "#252525" : "#f5f5f5",
          isDark ? "#666666" : "#888888",
          isDark ? "rgba(100,100,100,0.2)" : "rgba(96,125,139,0.1)"));
}

void AddFilesDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // ===== ЗАГОЛОВОК =====
    QLabel* titleLabel = new QLabel("📄 Добавление файлов в папку:", this);
    titleLabel->setStyleSheet(
        "QLabel {"
        "   font-size: 14px;"
        "   font-weight: 600;"
        "   color: #263238;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "   background: transparent;"
        "}"
        );
    mainLayout->addWidget(titleLabel);

    QLabel* folderLabel = new QLabel("📁 " + m_folderPath, this);
    folderLabel->setStyleSheet(
        "QLabel {"
        "   color: #607D8B;"
        "   font-size: 12px;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "   background: transparent;"
        "}"
        );
    folderLabel->setWordWrap(true);
    mainLayout->addWidget(folderLabel);

    mainLayout->addSpacing(10);

    // ===== СПИСОК ВЫБРАННЫХ ФАЙЛОВ =====
    QGroupBox* filesGroup = new QGroupBox("📋 Выбранные файлы", this);
    QVBoxLayout* filesLayout = new QVBoxLayout(filesGroup);

    m_filesList = new QListWidget(this);
    m_filesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_filesList->setDragEnabled(true);
    m_filesList->setDropIndicatorShown(true);
    filesLayout->addWidget(m_filesList);

    QHBoxLayout* listButtonsLayout = new QHBoxLayout();
    listButtonsLayout->addStretch();

    QPushButton* clearListBtn = new QPushButton("🗑️ Очистить список", this);
    clearListBtn->setObjectName("clearBtn");
    connect(clearListBtn, &QPushButton::clicked, this, &AddFilesDialog::clearFileList);
    listButtonsLayout->addWidget(clearListBtn);

    filesLayout->addLayout(listButtonsLayout);
    mainLayout->addWidget(filesGroup);

    // ===== КНОПКИ ДОБАВЛЕНИЯ =====
    QHBoxLayout* addButtonsLayout = new QHBoxLayout();
    addButtonsLayout->setSpacing(8);

    QPushButton* addFilesBtn = new QPushButton("📄 Добавить файлы", this);
    connect(addFilesBtn, &QPushButton::clicked, this, &AddFilesDialog::onAddMultipleFiles);
    addButtonsLayout->addWidget(addFilesBtn);

    QPushButton* addModelsBtn = new QPushButton("🖥️ 3D модели", this);
    connect(addModelsBtn, &QPushButton::clicked, this, &AddFilesDialog::onAddModels);
    addButtonsLayout->addWidget(addModelsBtn);

    QPushButton* addDrawingsBtn = new QPushButton("📐 Чертежи", this);
    connect(addDrawingsBtn, &QPushButton::clicked, this, &AddFilesDialog::onAddDrawings);
    addButtonsLayout->addWidget(addDrawingsBtn);

    QPushButton* addOtherBtn = new QPushButton("📎 Другие файлы", this);
    connect(addOtherBtn, &QPushButton::clicked, this, &AddFilesDialog::onAddOtherFiles);
    addButtonsLayout->addWidget(addOtherBtn);

    addButtonsLayout->addStretch();
    mainLayout->addLayout(addButtonsLayout);

    // ===== ИНФО-ЛЕЙБЛ =====
    m_infoLabel = new QLabel("💡 Выберите файлы для добавления. Можно выбрать несколько файлов одновременно.", this);
    m_infoLabel->setStyleSheet(
        "QLabel {"
        "   color: #607D8B;"
        "   font-size: 11px;"
        "   font-family: 'Segoe UI', 'Arial', sans-serif;"
        "   background: transparent;"
        "}"
        );
    mainLayout->addWidget(m_infoLabel);

    // БАГ 5: ДОБАВЛЯЕМ ПОЛЕ ДЛЯ ОТВЕТСТВЕННОГО =====
    QGroupBox* responsibleGroup = new QGroupBox("👤 Назначить ответственного", this);
    QVBoxLayout* responsibleLayout = new QVBoxLayout(responsibleGroup);

    m_responsibleEdit = new QLineEdit(this);
    m_responsibleEdit->setPlaceholderText("Введите ФИО (опционально)");
    responsibleLayout->addWidget(m_responsibleEdit);

    mainLayout->addWidget(responsibleGroup);

    // ===== КНОПКИ =====
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_cancelBtn = new QPushButton("Отмена", this);
    m_cancelBtn->setObjectName("cancelBtn");
    connect(m_cancelBtn, &QPushButton::clicked, this, &AddFilesDialog::onCancelClicked);
    buttonLayout->addWidget(m_cancelBtn);

    m_okBtn = new QPushButton("✅ Добавить", this);
    m_okBtn->setObjectName("okBtn");
    connect(m_okBtn, &QPushButton::clicked, this, &AddFilesDialog::onOkClicked);
    buttonLayout->addWidget(m_okBtn);

    mainLayout->addLayout(buttonLayout);
}

// ============================================================
// МЕТОДЫ ДОБАВЛЕНИЯ ФАЙЛОВ
// ============================================================

void AddFilesDialog::addFilesToList(const QStringList& files)
{
    for (const QString& file : files) {
        if (!file.isEmpty()) {
            // Проверяем, есть ли уже такой файл в списке
            bool exists = false;
            for (int i = 0; i < m_filesList->count(); ++i) {
                if (m_filesList->item(i)->text() == file) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                QListWidgetItem* item = new QListWidgetItem(QFileInfo(file).fileName(), m_filesList);
                item->setData(Qt::UserRole, file);
                item->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
                m_filesList->addItem(item);
                m_selectedFiles.append(file);
            }
        }
    }
    updateInfoLabel();
}

void AddFilesDialog::clearFileList()
{
    m_filesList->clear();
    m_selectedFiles.clear();
    updateInfoLabel();
}

void AddFilesDialog::updateInfoLabel()
{
    int count = m_filesList->count();
    if (count == 0) {
        m_infoLabel->setText("💡 Выберите файлы для добавления. Можно выбрать несколько файлов одновременно.");
        m_infoLabel->setStyleSheet("color: #607D8B; font-size: 11px; background: transparent;");
    } else {
        m_infoLabel->setText(QString("✅ Выбрано файлов: %1").arg(count));
        m_infoLabel->setStyleSheet("color: #4CAF50; font-size: 11px; font-weight: bold; background: transparent;");
    }
}

void AddFilesDialog::onAddMultipleFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(this,
                                                      "Выберите файлы для добавления",
                                                      "",
                                                      "Все файлы (*.*)");
    if (!files.isEmpty()) {
        addFilesToList(files);
    }
}

void AddFilesDialog::onAddModels()
{
    QStringList files = QFileDialog::getOpenFileNames(this,
                                                      "Выберите 3D-модели",
                                                      "",
                                                      "3D модели (*.m3d *.a3d *.sldprt *.sldasm *.step *.stp);;Все файлы (*.*)");
    if (!files.isEmpty()) {
        addFilesToList(files);
    }
}

void AddFilesDialog::onAddDrawings()
{
    QStringList files = QFileDialog::getOpenFileNames(this,
                                                      "Выберите чертежи",
                                                      "",
                                                      "Чертежи (*.cdw *.frw *.dxf *.dwg *.spw);;Все файлы (*.*)");
    if (!files.isEmpty()) {
        addFilesToList(files);
    }
}

void AddFilesDialog::onAddOtherFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(this,
                                                      "Выберите дополнительные файлы",
                                                      "",
                                                      "Все файлы (*.*)");
    if (!files.isEmpty()) {
        addFilesToList(files);
    }
}

void AddFilesDialog::onOkClicked()
{
    qDebug() << "=== AddFilesDialog::onOkClicked ===";
    qDebug() << "Выбрано файлов:" << m_selectedFiles.size();

    if (m_selectedFiles.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Выберите хотя бы один файл!");
        return;
    }
    accept();
}

void AddFilesDialog::onCancelClicked()
{
    reject();
}

// БАГ 5: НОВЫЙ МЕТОД =====
QString AddFilesDialog::getResponsible() const
{
    return m_responsibleEdit ? m_responsibleEdit->text().trimmed() : "";
}
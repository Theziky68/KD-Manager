#include "PreviewWidget.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QImageReader>
#include <QResizeEvent>
#include <QApplication>
#include <QStyle>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#include <QMessageBox>
#include <QDir>
#include <QDebug>
#include <QProcess>
#include <QDateTime>
#include <QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

PreviewWidget::PreviewWidget(QWidget *parent)
    : QWidget(parent)
    , m_hasViewer(false)
    , m_hasCompass(false)
    , m_hasPortableViewer(false)
    , m_viewerStatusChecked(false)
    , m_model3DViewer(nullptr)
{
    setupUI();
    clear();
    setMinimumSize(350, 500);
}

PreviewWidget::~PreviewWidget()
{
}

void PreviewWidget::setupUI()
{
    setMinimumSize(350, 450);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    m_previewContainer = new QWidget(this);
    QVBoxLayout* previewLayout = new QVBoxLayout(m_previewContainer);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(0);

    m_model3DViewer = new Model3DViewer(m_previewContainer);
    m_model3DViewer->setMinimumHeight(200);
    m_model3DViewer->setVisible(false);
    previewLayout->addWidget(m_model3DViewer);

    m_iconLabel = new QLabel(m_previewContainer);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setMinimumHeight(200);
    m_iconLabel->setScaledContents(false);
    previewLayout->addWidget(m_iconLabel);

    layout->addWidget(m_previewContainer);

    m_nameLabel = new QLabel(this);
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setWordWrap(true);
    m_nameLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    layout->addWidget(m_nameLabel);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setAlignment(Qt::AlignCenter);
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setStyleSheet(QString("color: %1; font-size: 12px;").arg(ThemeManager::instance().secondaryTextColor()));
    layout->addWidget(m_infoLabel);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 13px; padding: 4px;").arg(ThemeManager::instance().warningColor()));
    m_statusLabel->setVisible(false);
    layout->addWidget(m_statusLabel);

    m_buttonContainer = new QWidget(this);
    QVBoxLayout* btnLayout = new QVBoxLayout(m_buttonContainer);
    btnLayout->setContentsMargins(0, 10, 0, 10);
    btnLayout->setSpacing(12);

    m_openCompassBtn = new QPushButton("🔧 Открыть в КОМПАС-3D", this);
    m_openCompassBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_openCompassBtn->setMinimumHeight(50);
    m_openCompassBtn->setMinimumWidth(250);
    m_openCompassBtn->setStyleSheet(
        "QPushButton { background-color: #4CAF50; color: white; border: none; border-radius: 10px; padding: 14px 24px; font-weight: bold; font-size: 16px; }"
        "QPushButton:hover { background-color: #388E3C; }"
        "QPushButton:pressed { background-color: #1B5E20; }");
    m_openCompassBtn->setVisible(false);
    connect(m_openCompassBtn, &QPushButton::clicked, this, &PreviewWidget::onOpenWithCompass);
    btnLayout->addWidget(m_openCompassBtn);

    m_openViewerBtn = new QPushButton("👁️ Открыть в 3D Viewer", this);
    m_openViewerBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_openViewerBtn->setMinimumHeight(50);
    m_openViewerBtn->setMinimumWidth(250);
    m_openViewerBtn->setStyleSheet(
        "QPushButton { background-color: #FF9800; color: white; border: none; border-radius: 10px; padding: 14px 24px; font-weight: bold; font-size: 16px; }"
        "QPushButton:hover { background-color: #F57C00; }"
        "QPushButton:pressed { background-color: #E65100; }");
    m_openViewerBtn->setVisible(false);
    connect(m_openViewerBtn, &QPushButton::clicked, this, &PreviewWidget::onOpenWithCompassViewer);
    btnLayout->addWidget(m_openViewerBtn);

    btnLayout->addStretch();
    layout->addWidget(m_buttonContainer);

    clear();
}

void PreviewWidget::ensureViewerStatus()
{
    if (!m_viewerStatusChecked) {
        checkViewerStatus();
        m_viewerStatusChecked = true;
    }
}

void PreviewWidget::checkViewerStatus()
{
    m_hasViewer = findCompassViewer();
    m_hasCompass = findCompassInstalled();

    QString appDir = QCoreApplication::applicationDirPath();
    QStringList portablePaths = {
        appDir + "/KompasViewer/Bin/Viewer.exe",
        appDir + "/KompasViewer/Viewer.exe",
        appDir + "/KompasViewer/Bin/kViewer.exe",
        appDir + "/KompasViewer/kViewer.exe",
        appDir + "/Viewer.exe",
        appDir + "/KOMPAS-3D Viewer/Bin/Viewer.exe"
    };

    m_hasPortableViewer = false;
    for (const QString& path : portablePaths) {
        if (QFile::exists(path)) {
            m_portableViewerPath = path;
            m_hasPortableViewer = true;
            break;
        }
    }
}

bool PreviewWidget::findCompassViewer()
{
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList systemPaths = {
        "C:/Program Files/ASCON/KOMPAS-3D Viewer/Viewer.exe",
        "C:/Program Files (x86)/ASCON/KOMPAS-3D Viewer/Viewer.exe",
        "C:/Program Files/ASCON/KOMPAS-3D v22/Bin/Viewer.exe",
        "C:/Program Files/ASCON/KOMPAS-3D v21/Bin/Viewer.exe",
        "C:/Program Files/ASCON/KOMPAS-3D v20/Bin/Viewer.exe",
    };

    for (const QString& path : systemPaths) {
        if (QFile::exists(path)) {
            m_viewerPath = path;
            return true;
        }
    }

    m_viewerPath.clear();
    return false;
}

bool PreviewWidget::findCompassInstalled()
{
    QStringList possiblePaths = {
        "C:/Program Files/ASCON/KOMPAS-3D v22/Bin/kompas.exe",
        "C:/Program Files/ASCON/KOMPAS-3D v21/Bin/kompas.exe",
        "C:/Program Files/ASCON/KOMPAS-3D v20/Bin/kompas.exe",
    };

    for (const QString& path : possiblePaths) {
        if (QFile::exists(path)) {
            m_compassPath = path;
            return true;
        }
    }

    m_compassPath.clear();
    return false;
}

bool PreviewWidget::isCompassFile(const QString& filePath) const
{
    QString ext = QFileInfo(filePath).suffix().toLower();
    QStringList compassFormats = {
        "cdw", "frw", "spw",
        "m3d", "a3d",
        "prt", "asm", "cat",
        "dxf", "dwg",
        "step", "stp", "igs", "iges"
    };
    return compassFormats.contains(ext);
}

void PreviewWidget::load3DModel(const QString& filePath)
{
    if (!m_model3DViewer) return;

    QString ext = QFileInfo(filePath).suffix().toLower();

    if (ext == "step" || ext == "stp" || ext == "iges" || ext == "igs") {
        if (m_model3DViewer->loadModel(filePath)) {
            m_model3DViewer->setVisible(true);
            m_iconLabel->setVisible(false);
            return;
        }
    }

    m_model3DViewer->clear();
    m_model3DViewer->setVisible(false);
    m_iconLabel->setVisible(true);
}

void PreviewWidget::setFile(const QString& filePath)
{
    m_filePath = filePath;

    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        clear();
        return;
    }

    ensureViewerStatus();

    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    m_nameLabel->setText(fi.fileName());

    QString info = "";
    info += "Размер: " + formatFileSize(fi.size()) + "\n";
    info += "Изменен: " + fi.lastModified().toString("dd.MM.yyyy hh:mm") + "\n";
    info += "Тип: " + (ext.isEmpty() ? "Неизвестно" : ext.toUpper());

    bool isCompass = isCompassFile(filePath);

    updateButtons(filePath);

    if (isCompass) {
        info += "\n\n🔧 Файл КОМПАС-3D";
        m_iconLabel->setText("🔧");
        m_iconLabel->setPixmap(QPixmap());
        m_infoLabel->setText(info);

        load3DModel(filePath);
        return;
    }

    if (ext == "pdf") {
        info += "\n\n📄 PDF-документ";
        m_iconLabel->setText("📄");
        m_iconLabel->setPixmap(QPixmap());
        m_infoLabel->setText(info);
        return;
    }

    m_infoLabel->setText(info);
    loadPreview(filePath);
}

void PreviewWidget::loadPreview(const QString& filePath)
{
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    bool isImage = (ext == "jpg" || ext == "jpeg" || ext == "png" ||
                    ext == "bmp" || ext == "gif" || ext == "tiff" || ext == "tif");

    if (isImage) {
        QImageReader reader(filePath);
        if (reader.canRead()) {
            QImage image = reader.read();
            if (!image.isNull()) {
                QPixmap pixmap = QPixmap::fromImage(image);
                if (!pixmap.isNull()) {
                    m_currentPixmap = pixmap;
                    int maxSize = qMin(m_iconLabel->width() - 40, m_iconLabel->height() - 40);
                    if (maxSize > 20) {
                        QPixmap scaled = pixmap.scaled(maxSize, maxSize,
                                                       Qt::KeepAspectRatio,
                                                       Qt::SmoothTransformation);
                        m_iconLabel->setPixmap(scaled);
                        m_iconLabel->setText("");
                        return;
                    }
                }
            }
        }
    }

    QFileIconProvider provider;
    QIcon icon = provider.icon(fi);
    if (!icon.isNull()) {
        QPixmap pix = icon.pixmap(128, 128);
        if (!pix.isNull()) {
            m_currentPixmap = pix;
            m_iconLabel->setPixmap(pix);
            m_iconLabel->setText("");
            return;
        }
    }

    m_iconLabel->setText("📄");
    m_iconLabel->setPixmap(QPixmap());
}

QString PreviewWidget::formatFileSize(qint64 size)
{
    if (size < 1024) return QString::number(size) + " B";
    if (size < 1048576) return QString::number(size / 1024) + " KB";
    if (size < 1073741824) return QString::number(size / 1048576) + " MB";
    return QString::number(size / 1073741824) + " GB";
}

void PreviewWidget::updateButtons(const QString& filePath)
{
    ensureViewerStatus();

    m_openCompassBtn->setVisible(false);
    m_openViewerBtn->setVisible(false);
    m_statusLabel->setVisible(false);
    m_buttonContainer->setVisible(false);

    if (filePath.isEmpty()) {
        return;
    }

    if (isCompassFile(filePath)) {
        m_buttonContainer->setVisible(true);

        m_openCompassBtn->setVisible(true);
        m_openCompassBtn->setEnabled(true);
        m_openCompassBtn->setText("🔧 Открыть в КОМПАС-3D");

        m_openViewerBtn->setVisible(true);

        if (m_hasViewer || m_hasPortableViewer) {
            m_openViewerBtn->setEnabled(true);
            m_openViewerBtn->setText("👁️ Открыть в 3D Viewer");
        } else {
            m_openViewerBtn->setEnabled(false);
            m_openViewerBtn->setText("❌ 3D Viewer не найден");
        }
    }
}

bool PreviewWidget::openWithCompass(const QString& filePath)
{
#ifdef Q_OS_WIN
    if (!QFile::exists(filePath)) {
        QMessageBox::warning(this, "Ошибка", "Файл не найден:\n" + filePath);
        return false;
    }

    bool result = QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));

    if (result) {
        return true;
    }

    QString filePathNormal = QDir::toNativeSeparators(filePath);
    std::wstring filePathW = filePathNormal.toStdWString();

    HINSTANCE shellResult = ShellExecuteW(
        (HWND)this->winId(),
        L"open",
        filePathW.c_str(),
        NULL,
        NULL,
        SW_SHOWNORMAL
    );

    if ((INT_PTR)shellResult > 32) {
        return true;
    }

    QMessageBox::warning(this, "Не удалось открыть файл",
                         "Не удалось открыть файл.\n\nПуть к файлу:\n" + filePath);

    return false;
#else
    Q_UNUSED(filePath);
    return false;
#endif
}

bool PreviewWidget::openWithCompassViewer(const QString& filePath)
{
#ifdef Q_OS_WIN
    if (!QFile::exists(filePath)) {
        QMessageBox::warning(this, "Ошибка", "Файл не найден:\n" + filePath);
        return false;
    }

    bool result = QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));

    if (result) {
        return true;
    }

    if (m_viewerPath.isEmpty()) {
        if (!findCompassViewer()) {
            QMessageBox::warning(this, "Ошибка",
                                 "3D Viewer не найден.");
            return false;
        }
    }

    if (!QFile::exists(m_viewerPath)) {
        QMessageBox::warning(this, "Ошибка", "3D Viewer не найден:\n" + m_viewerPath);
        return false;
    }

    QString viewerPath = QDir::toNativeSeparators(m_viewerPath);
    QString filePathNormal = QDir::toNativeSeparators(filePath);
    QFileInfo viewerInfo(m_viewerPath);
    QString viewerDir = QDir::toNativeSeparators(viewerInfo.absolutePath());

    std::wstring exePath = viewerPath.toStdWString();
    std::wstring filePathW = filePathNormal.toStdWString();
    std::wstring workDirW = viewerDir.toStdWString();

    HINSTANCE shellResult = ShellExecuteW(
        (HWND)this->winId(),
        L"open",
        exePath.c_str(),
        filePathW.c_str(),
        workDirW.c_str(),
        SW_SHOWNORMAL
    );

    if ((INT_PTR)shellResult > 32) {
        return true;
    }

    QMessageBox::warning(this, "Ошибка",
                         "Не удалось запустить 3D Viewer.\n\nПуть к Viewer:\n" + m_viewerPath);

    return false;
#else
    Q_UNUSED(filePath);
    return false;
#endif
}

void PreviewWidget::onOpenWithCompass()
{
    if (m_filePath.isEmpty() || !QFile::exists(m_filePath)) {
        QMessageBox::warning(this, "Ошибка", "Файл не найден!");
        return;
    }

    openWithCompass(m_filePath);
}

void PreviewWidget::onOpenWithCompassViewer()
{
    if (m_filePath.isEmpty() || !QFile::exists(m_filePath)) {
        QMessageBox::warning(this, "Ошибка", "Файл не найден!");
        return;
    }

    openWithCompassViewer(m_filePath);
}

void PreviewWidget::onModel3DLoaded()
{
    update();
}

void PreviewWidget::clear()
{
    m_filePath = "";
    m_currentPixmap = QPixmap();
    m_iconLabel->setText("📂");
    m_iconLabel->setPixmap(QPixmap());
    m_nameLabel->setText("");
    m_infoLabel->setText("Выберите файл для просмотра");
    m_openCompassBtn->setVisible(false);
    m_openViewerBtn->setVisible(false);
    m_statusLabel->setVisible(false);
    m_buttonContainer->setVisible(false);
    if (m_model3DViewer) {
        m_model3DViewer->clear();
        m_model3DViewer->setVisible(false);
    }
    m_iconLabel->setVisible(true);
}

void PreviewWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void PreviewWidget::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    QPainter painter(this);
    QColor borderColor = ThemeManager::instance().currentTheme() == ThemeManager::Dark ? QColor("#374151") : QColor(200, 200, 200);
    painter.setPen(QPen(borderColor, 1));
    painter.drawRect(rect().adjusted(1, 1, -1, -1));
}

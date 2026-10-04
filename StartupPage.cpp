#include "StartupPage.h"
#include "FolderData.h"
#include "AboutDialog.h"
#include "ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QFileInfo>
#include <QDateTime>
#include <QEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QLabel>
#include <QPushButton>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QSettings>
#include <QDebug>
#include <QGraphicsDropShadowEffect>
#include <QColor>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QTimer>
#include <algorithm>

namespace {
QGraphicsDropShadowEffect* makeShadow(QWidget* parent, int blur = 24, int yOffset = 6, int alpha = 28)
{
    auto* effect = new QGraphicsDropShadowEffect(parent);
    effect->setBlurRadius(blur);
    effect->setOffset(0, yOffset);
    effect->setColor(QColor(30, 30, 46, alpha));
    return effect;
}
}

StartupPage::StartupPage(DatabaseManager& db, QWidget *parent)
    : QWidget(parent)
    , m_db(db)
    , m_statsContainer(nullptr)
    , m_aboutBtn(nullptr)
{
    setupUI();
    loadPinnedState();
    loadProducts();
    updateStatistics();
}

void StartupPage::setupUI()
{
    ThemeManager& themeManager = ThemeManager::instance();

    // Фон страницы - используем основной цвет фона
    QString backgroundColor = themeManager.backgroundColor();
    setStyleSheet(
        QString(
            "StartupPage {"
            "   background: %1;"
            "}"
            ).arg(backgroundColor)
        );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(48, 32, 48, 28);
    mainLayout->setSpacing(14);

    // ===== ЗАГОЛОВОК =====
    QLabel* titleLabel = new QLabel("КД Менеджер", this);
    titleLabel->setStyleSheet(
        QString(
            "QLabel {"
            "   font-size: 32px;"
            "   font-weight: 800;"
            "   color: %1;"
            "   font-family: 'Segoe UI', 'Inter', 'Arial', sans-serif;"
            "   background: transparent;"
            "}"
            ).arg(themeManager.textColor())
        );
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // ===== ТОНКИЙ АКЦЕНТНЫЙ АКЦЕНТ ПОД ЗАГОЛОВКОМ =====
    QFrame* titleAccent = new QFrame(this);
    titleAccent->setFixedSize(56, 4);
    titleAccent->setStyleSheet(
        QString(
            "QFrame {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
            "                               stop:0 %1, stop:1 %2);"
            "   border-radius: 2px;"
            "   border: none;"
            "}"
            ).arg(themeManager.primaryGradientStart(), themeManager.primaryGradientEnd())
        );
    QHBoxLayout* titleAccentLayout = new QHBoxLayout();
    titleAccentLayout->setAlignment(Qt::AlignCenter);
    titleAccentLayout->setContentsMargins(0, 0, 0, 4);
    titleAccentLayout->addWidget(titleAccent);
    mainLayout->addLayout(titleAccentLayout);

    QLabel* subtitleLabel = new QLabel("Выберите изделие для работы", this);
    subtitleLabel->setStyleSheet(
        QString(
            "QLabel {"
            "   font-size: 14px;"
            "   color: %1;"
            "   font-family: 'Segoe UI', 'Inter', 'Arial', sans-serif;"
            "   background: transparent;"
            "}"
            ).arg(themeManager.secondaryTextColor())
        );
    subtitleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(subtitleLabel);

    // ============================================================
    // СТАТИСТИКА
    // ============================================================
    m_statsContainer = new QWidget(this);
    m_statsContainer->setStyleSheet("QWidget { background: transparent; }");
    QHBoxLayout* statsLayout = new QHBoxLayout(m_statsContainer);
    statsLayout->setSpacing(15);
    statsLayout->setAlignment(Qt::AlignCenter);
    statsLayout->setContentsMargins(0, 5, 0, 5);

    auto createStatCard = [&](const QString& icon, const QString& label, const QString& color) -> QFrame* {
        QFrame* card = new QFrame(m_statsContainer);
        card->setFixedSize(126, 92);
        card->setStyleSheet(
            "QFrame {"
            "   background: " + themeManager.cardBackground() + ";"
            "   border-radius: 18px;"
            "   border: none;"
            "}"
            );
        card->setGraphicsEffect(makeShadow(card, 20, 5, 22));

        QVBoxLayout* layout = new QVBoxLayout(card);
        layout->setSpacing(4);
        layout->setContentsMargins(10, 12, 10, 10);
        layout->setAlignment(Qt::AlignCenter);

        QHBoxLayout* topRow = new QHBoxLayout();
        topRow->setAlignment(Qt::AlignCenter);
        topRow->setSpacing(6);

        QLabel* iconBadge = new QLabel(icon, card);
        iconBadge->setFixedSize(24, 24);
        iconBadge->setAlignment(Qt::AlignCenter);
        iconBadge->setStyleSheet(
            "QLabel {"
            "   font-size: 13px;"
            "   background: " + color + "1A;" // ~10% opacity tint
                               "   border-radius: 12px;"
                               "   border: none;"
                               "}"
            );
        topRow->addWidget(iconBadge);

        QLabel* valueLabel = new QLabel("0", card);
        valueLabel->setObjectName("statValue");
        valueLabel->setStyleSheet(
            "QLabel#statValue {"
            "   font-size: 22px;"
            "   font-weight: 800;"
            "   color: " + themeManager.textColor() + ";"
                                      "   font-family: 'Segoe UI', 'Arial', sans-serif;"
                                      "   background: transparent;"
                                      "   border: none;"
                                      "}"
            );
        valueLabel->setAlignment(Qt::AlignCenter);
        topRow->addWidget(valueLabel);

        layout->addLayout(topRow);

        QLabel* labelLabel = new QLabel(label, card);
        labelLabel->setStyleSheet(
            "QLabel {"
            "   font-size: 11px;"
            "   color: " + themeManager.secondaryTextColor() + ";"
                                     "   font-family: 'Segoe UI', 'Arial', sans-serif;"
                                     "   background: transparent;"
                                     "   border: none;"
                                     "   font-weight: 600;"
                                     "}"
            );
        labelLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(labelLabel);

        return card;
    };

    m_statCards["products"] = createStatCard("📦", "Изделий", "#22C55E");
    m_statCards["assemblies"] = createStatCard("🔧", "СБ", "#3B82F6");
    m_statCards["details"] = createStatCard("⚙️", "Деталей", "#F59E0B");
    m_statCards["files"] = createStatCard("📄", "Файлов", "#A855F7");

    statsLayout->addWidget(m_statCards["products"]);
    statsLayout->addWidget(m_statCards["assemblies"]);
    statsLayout->addWidget(m_statCards["details"]);
    statsLayout->addWidget(m_statCards["files"]);

    mainLayout->addWidget(m_statsContainer);

    // ============================================================
    // ПОИСК
    // ============================================================
    QFrame* searchFrame = new QFrame(this);
    searchFrame->setObjectName("searchFrame");
    searchFrame->setStyleSheet(
        QString(
            "QFrame#searchFrame {"
            "   background-color: %1;"
            "   border-radius: 22px;"
            "   border: 1px solid transparent;"
            "}"
            "QFrame#searchFrame:focus-within {"
            "   border: 1px solid %2;"
            "}"
            ).arg(themeManager.cardBackground(), themeManager.accentColor())
        );
    searchFrame->setFixedHeight(46);
    searchFrame->setMaximumWidth(480);
    searchFrame->setGraphicsEffect(makeShadow(searchFrame, 18, 4, 18));

    QHBoxLayout* searchLayout = new QHBoxLayout(searchFrame);
    searchLayout->setContentsMargins(18, 0, 18, 0);
    searchLayout->setSpacing(10);

    QLabel* searchIcon = new QLabel("🔍", searchFrame);
    searchIcon->setStyleSheet("font-size: 15px; background: transparent;");
    searchLayout->addWidget(searchIcon);

    m_searchEdit = new QLineEdit(searchFrame);
    m_searchEdit->setPlaceholderText("Поиск изделия...");
    m_searchEdit->setStyleSheet(
        QString(
            "QLineEdit {"
            "   border: none;"
            "   background: transparent;"
            "   font-size: 14px;"
            "   color: %1;"
            "   font-family: 'Segoe UI', 'Arial', sans-serif;"
            "}"
            "QLineEdit::placeholder {"
            "   color: %2;"
            "}"
            ).arg(themeManager.textColor(), themeManager.secondaryTextColor())
        );
    connect(m_searchEdit, &QLineEdit::textChanged, this, &StartupPage::onSearchTextChanged);
    searchLayout->addWidget(m_searchEdit);

    QHBoxLayout* searchWrapLayout = new QHBoxLayout();
    searchWrapLayout->setAlignment(Qt::AlignCenter);
    searchWrapLayout->addWidget(searchFrame);
    mainLayout->addLayout(searchWrapLayout);

    // ============================================================
    // СЕТКА ИЗДЕЛИЙ
    // ============================================================
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setStyleSheet(
        "QScrollArea {"
        "   background: transparent;"
        "   border: none;"
        "}"
        "QScrollBar:vertical {"
        "   background: transparent;"
        "   width: 6px;"
        "   margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "   background: " + themeManager.borderColor() + ";"
        "   border-radius: 3px;"
        "   min-height: 20px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "   background: " + themeManager.secondaryTextColor() + ";"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "   height: 0px;"
        "}"
        );

    QWidget* scrollWidget = new QWidget(this);
    scrollWidget->setStyleSheet("background: transparent;");
    m_productsLayout = new QGridLayout(scrollWidget);
    m_productsLayout->setContentsMargins(0, 10, 0, 10);
    m_productsLayout->setSpacing(20);
    m_productsLayout->setAlignment(Qt::AlignCenter);

    m_scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(m_scrollArea);

    // ============================================================
    // НИЖНЯЯ ПАНЕЛЬ: КНОПКА СОЗДАНИЯ + КНОПКА О ПРОГРАММЕ
    // ============================================================
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(12);
    bottomLayout->setAlignment(Qt::AlignCenter);

    // Кнопка "Создать изделие"
    QPushButton* createBtn = new QPushButton("Создать изделие", this);
    createBtn->setStyleSheet(
        QString(
            "QPushButton {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
            "                               stop:0 %1, stop:1 %2);"
            "   color: white;"
            "   border: none;"
            "   border-radius: 14px;"
            "   padding: 13px 32px;"
            "   font-size: 15px;"
            "   font-weight: 700;"
            "   font-family: 'Segoe UI', 'Arial', sans-serif;"
            "   min-width: 180px;"
            "}"
            "QPushButton:hover {"
            "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
            "                               stop:0 %3, stop:1 %1);"
            "}"
            "QPushButton:pressed {"
            "   background: %4;"
            "}"
            ).arg(themeManager.primaryGradientStart(), themeManager.primaryGradientEnd(),
                  themeManager.accentColor(), themeManager.primaryGradientEnd())
        );
    createBtn->setCursor(Qt::PointingHandCursor);
    createBtn->setGraphicsEffect(makeShadow(createBtn, 20, 6, 45));
    connect(createBtn, &QPushButton::clicked, this, &StartupPage::createNewProduct);
    bottomLayout->addWidget(createBtn);

    // ===== КНОПКА "О ПРОГРАММЕ" =====
    m_aboutBtn = new QPushButton("📖 О программе", this);
    m_aboutBtn->setStyleSheet(
        QString(
            "QPushButton {"
            "   background: %1;"
            "   color: %2;"
            "   border: 2px solid %3;"
            "   border-radius: 14px;"
            "   padding: 13px 22px;"
            "   font-size: 14px;"
            "   font-weight: 600;"
            "   font-family: 'Segoe UI', 'Arial', sans-serif;"
            "   min-width: 140px;"
            "}"
            "QPushButton:hover {"
            "   background: %4;"
            "   border: 2px solid %5;"
            "}"
            "QPushButton:pressed {"
            "   background: %6;"
            "   border: 2px solid %7;"
            "}"
            ).arg(themeManager.currentTheme() == ThemeManager::Dark ? "#2A3F3F" : themeManager.cardBackground(),
                  themeManager.textColor(),
                  themeManager.accentColor(),
                  themeManager.accentColor(),
                  themeManager.primaryGradientStart(),
                  themeManager.primaryGradientStart(),
                  themeManager.primaryGradientEnd())
        );
    m_aboutBtn->setCursor(Qt::PointingHandCursor);
    m_aboutBtn->setGraphicsEffect(makeShadow(m_aboutBtn, 16, 4, 15));

    // ===== ИСПРАВЛЕНИЕ: ТОЛЬКО ОДИН КОННЕКТ =====
    connect(m_aboutBtn, &QPushButton::clicked, this, &StartupPage::onAboutClicked);
    bottomLayout->addWidget(m_aboutBtn);

    mainLayout->addLayout(bottomLayout);
}

void StartupPage::onAboutClicked()
{
    emit showAbout();
}

// ============================================================
// ОСТАЛЬНЫЕ МЕТОДЫ (без изменений)
// ============================================================

void StartupPage::updateStatistics()
{
    int productCount = 0, assemblyCount = 0, detailCount = 0, fileCount = 0;

    // ===== НАХОДИМ РАЗДЕЛ "ИЗДЕЛИЯ" =====
    QString productSectionPath;
    for (auto it = m_db.folders().begin(); it != m_db.folders().end(); ++it) {
        if (it.key().isEmpty()) continue;
        int lastSlash = it.key().lastIndexOf('/');
        QString parentPath = (lastSlash != -1) ? it.key().left(lastSlash) : "";
        if (parentPath.isEmpty() && it.value().name == "Изделия" && it.value().type == TYPE_PRODUCT) {
            productSectionPath = it.key();
            break;
        }
    }

    // ===== ПОДСЧЕТ ТОЛЬКО ПАПОК-ИЗДЕЛИЙ (НЕ СЧИТАЕМ САМ РАЗДЕЛ "ИЗДЕЛИЯ") =====
    for (auto it = m_db.folders().begin(); it != m_db.folders().end(); ++it) {
        if (it.key().isEmpty()) continue;

        // Пропускаем сам раздел "Изделия"
        if (it.key() == productSectionPath) {
            continue;
        }

        FolderType type = it.value().type;

        // Считаем только изделия (папки типа PRODUCT внутри раздела "Изделия")
        if (type == TYPE_PRODUCT && !productSectionPath.isEmpty()) {
            if (it.key().startsWith(productSectionPath + "/")) {
                productCount++;
            }
        }

        // Считаем сборки (любые, не только внутри изделий)
        if (type == TYPE_ASSEMBLY) {
            assemblyCount++;
        }

        // Считаем детали (любые)
        if (type == TYPE_DETAIL) {
            detailCount++;
        }

        fileCount += it.value().files.size();
    }

    auto updateStatCard = [this](const QString& key, int value) {
        QFrame* card = m_statCards.value(key);
        if (!card) return;
        QLabel* valueLabel = card->findChild<QLabel*>("statValue");
        if (!valueLabel) return;

        bool ok = false;
        int startValue = valueLabel->text().toInt(&ok);
        if (!ok) startValue = 0;
        if (startValue == value) return;

        auto* anim = new QVariantAnimation(valueLabel);
        anim->setDuration(550);
        anim->setStartValue(startValue);
        anim->setEndValue(value);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(anim, &QVariantAnimation::valueChanged, valueLabel, [valueLabel](const QVariant& v) {
            valueLabel->setText(QString::number(v.toInt()));
        });
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    };

    updateStatCard("products", productCount);
    updateStatCard("assemblies", assemblyCount);
    updateStatCard("details", detailCount);
    updateStatCard("files", fileCount);
}

void StartupPage::loadProducts()
{
    m_productCards.clear();

    QLayoutItem* child;
    while ((child = m_productsLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    this->setUpdatesEnabled(false);

    struct ProductInfo {
        QString path;
        QString name;
        FolderData data;
        bool isPinned;
    };

    QList<ProductInfo> products;

    // ===== НАХОДИМ РАЗДЕЛ "ИЗДЕЛИЯ" =====
    QString productSectionPath;
    for (auto it = m_db.folders().begin(); it != m_db.folders().end(); ++it) {
        if (it.key().isEmpty()) continue;
        int lastSlash = it.key().lastIndexOf('/');
        QString parentPath = (lastSlash != -1) ? it.key().left(lastSlash) : "";
        if (parentPath.isEmpty() && it.value().name == "Изделия" && it.value().type == TYPE_PRODUCT) {
            productSectionPath = it.key();
            break;
        }
    }

    qDebug() << "productSectionPath:" << productSectionPath;

    // ===== СОБИРАЕМ ТОЛЬКО ПАПКИ-ИЗДЕЛИЯ (ТИП PRODUCT) ВНУТРИ РАЗДЕЛА "ИЗДЕЛИЯ" =====
    for (auto it = m_db.folders().begin(); it != m_db.folders().end(); ++it) {
        if (it.key().isEmpty()) continue;

        // Пропускаем сам раздел "Изделия"
        if (it.key() == productSectionPath) {
            continue;
        }

        // Проверяем, что это изделие (тип PRODUCT) и оно находится внутри раздела "Изделия"
        if (it.value().type == TYPE_PRODUCT && !productSectionPath.isEmpty()) {
            // Проверяем, что папка находится внутри "Изделия"
            if (it.key().startsWith(productSectionPath + "/")) {
                ProductInfo info;
                info.path = it.key();
                info.name = it.value().name;
                info.data = it.value();
                info.isPinned = m_pinnedProducts.contains(it.key());
                products.append(info);
                qDebug() << "Найдено изделие:" << info.name << "путь:" << info.path;
            }
        }
    }

    // Сортируем: сначала закрепленные, потом по алфавиту
    std::sort(products.begin(), products.end(),
              [](const ProductInfo& a, const ProductInfo& b) {
                  if (a.isPinned && !b.isPinned) return true;
                  if (!a.isPinned && b.isPinned) return false;
                  return QString::localeAwareCompare(a.name, b.name) < 0;
              });

    int row = 0;
    int col = 0;
    int index = 0;

    for (const ProductInfo& info : products) {
        QFrame* card = createProductCard(info.path, info.data);
        m_productCards[info.path] = card;
        m_productsLayout->addWidget(card, row, col);

        // ===== ПЛАВНОЕ "ПРОЯВЛЕНИЕ" КАРТОЧКИ ПРИ ЗАГРУЗКЕ (staggered) =====
        if (auto* effect = qobject_cast<QGraphicsDropShadowEffect*>(card->graphicsEffect())) {
            QColor restColor = effect->color();
            qreal restBlur = effect->blurRadius();
            effect->setBlurRadius(0);
            QColor hiddenColor = restColor;
            hiddenColor.setAlpha(0);
            effect->setColor(hiddenColor);

            int delayMs = 50 + index * 40;
            QTimer::singleShot(delayMs, card, [effect, restColor, restBlur]() {
                auto* blurAnim = new QPropertyAnimation(effect, "blurRadius", effect);
                blurAnim->setDuration(300);
                blurAnim->setEndValue(restBlur);
                blurAnim->setEasingCurve(QEasingCurve::OutCubic);
                blurAnim->start(QAbstractAnimation::DeleteWhenStopped);

                auto* colorAnim = new QPropertyAnimation(effect, "color", effect);
                colorAnim->setDuration(300);
                colorAnim->setEndValue(restColor);
                colorAnim->start(QAbstractAnimation::DeleteWhenStopped);
            });
        }

        index++;
        col++;
        if (col > 2) {
            col = 0;
            row++;
        }
    }

    if (products.isEmpty()) {
        QWidget* emptyWidget = new QWidget(this);
        QVBoxLayout* emptyLayout = new QVBoxLayout(emptyWidget);
        emptyLayout->setAlignment(Qt::AlignCenter);

        QLabel* emptyIcon = new QLabel("📭", emptyWidget);
        emptyIcon->setStyleSheet(
            "QLabel {"
            "   font-size: 64px;"
            "   background: transparent;"
            "}"
            );
        emptyIcon->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(emptyIcon);

        QLabel* emptyLabel = new QLabel("Нет созданных изделий", emptyWidget);
        emptyLabel->setStyleSheet(
            QString(
                "QLabel {"
                "   font-size: 18px;"
                "   color: %1;"
                "   font-family: 'Segoe UI', 'Arial', sans-serif;"
                "   font-weight: 500;"
                "}"
                ).arg(ThemeManager::instance().secondaryTextColor())
            );
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(emptyLabel);

        m_productsLayout->addWidget(emptyWidget, 0, 0, 1, 3);
    }

    this->setUpdatesEnabled(true);
}

void StartupPage::addProductCard(const QString& productPath)
{
    if (m_productCards.contains(productPath)) {
        return;
    }

    if (!m_db.folders().contains(productPath)) {
        return;
    }

    const FolderData& data = m_db.folders()[productPath];
    if (data.type != TYPE_PRODUCT) {
        return;
    }

    if (m_productsLayout->count() == 1) {
        QLayoutItem* item = m_productsLayout->itemAt(0);
        if (item && item->widget()) {
            QWidget* widget = item->widget();
            QList<QLabel*> labels = widget->findChildren<QLabel*>();
            if (!labels.isEmpty() && labels[0]->text() == "📭") {
                delete widget;
                while (m_productsLayout->count() > 0) {
                    QLayoutItem* child = m_productsLayout->takeAt(0);
                    delete child;
                }
            }
        }
    }

    loadProducts();
    updateStatistics();
}

void StartupPage::removeProductCard(const QString& productPath)
{
    if (!m_productCards.contains(productPath)) {
        return;
    }

    QFrame* card = m_productCards[productPath];
    m_productsLayout->removeWidget(card);
    delete card;
    m_productCards.remove(productPath);
    m_pinnedProducts.remove(productPath);
    savePinnedState();
    updateStatistics();
}

QFrame* StartupPage::createProductCard(const QString& path, const FolderData& data)
{
    QString displayName = data.name;
    if (displayName.isEmpty()) {
        int lastSlash = path.lastIndexOf('/');
        if (lastSlash != -1) {
            displayName = path.mid(lastSlash + 1);
        } else {
            displayName = path;
        }
    }

    bool isPinned = m_pinnedProducts.contains(path);
    ThemeManager& themeManager = ThemeManager::instance();

    QFrame* card = new QFrame(this);
    card->setObjectName("productCard");
    card->setStyleSheet(
        QString(
            "QFrame#productCard {"
            "   background: %1;"
            "   border-radius: 20px;"
            "   border: 1px solid %2;"
            "}"
            "QFrame#productCard:hover {"
            "   border: 1px solid %3;"
            "}"
            ).arg(themeManager.cardBackground(),
                  themeManager.borderColor(),
                  themeManager.accentColor())
        );
    card->setCursor(Qt::PointingHandCursor);
    card->setFixedSize(196, 196);
    card->setProperty("productPath", path);
    card->setGraphicsEffect(makeShadow(card, 22, 8, 20));

    QVBoxLayout* layout = new QVBoxLayout(card);
    layout->setSpacing(6);
    layout->setContentsMargins(14, 12, 14, 14);
    layout->setAlignment(Qt::AlignCenter);

    QHBoxLayout* pinLayout = new QHBoxLayout();
    pinLayout->setContentsMargins(0, 0, 0, 0);
    pinLayout->setAlignment(Qt::AlignRight);

    if (isPinned) {
        QLabel* pinIcon = new QLabel("📌", card);
        pinIcon->setStyleSheet(
            "QLabel {"
            "   font-size: 16px;"
            "   background: transparent;"
            "   border: none;"
            "}"
            );
        pinIcon->setAlignment(Qt::AlignRight);
        pinLayout->addWidget(pinIcon);
    } else {
        QLabel* emptyPin = new QLabel("", card);
        emptyPin->setStyleSheet("background: transparent;");
        pinLayout->addWidget(emptyPin);
    }

    layout->addLayout(pinLayout);

    // ===== ИКОНКА В ЦВЕТНОМ "БЕЙДЖЕ" ВМЕСТО ГОЛОЙ ЭМОДЗИ =====
    QLabel* iconLabel = new QLabel(isPinned ? "⭐" : "📁", card);
    iconLabel->setFixedSize(64, 64);
    QString iconBadgeBg = themeManager.hoverColor();
    iconLabel->setStyleSheet(
        QString(
            "QLabel {"
            "   font-size: 30px;"
            "   background: %1;"
            "   border-radius: 32px;"
            "   border: none;"
            "}"
            ).arg(iconBadgeBg)
        );
    iconLabel->setAlignment(Qt::AlignCenter);

    QHBoxLayout* iconWrap = new QHBoxLayout();
    iconWrap->setAlignment(Qt::AlignCenter);
    iconWrap->addWidget(iconLabel);
    layout->addLayout(iconWrap);

    QLabel* nameLabel = new QLabel(displayName, card);
    nameLabel->setStyleSheet(
        QString(
            "QLabel {"
            "   font-size: 14px;"
            "   font-weight: 700;"
            "   color: %1;"
            "   font-family: 'Segoe UI', 'Arial', sans-serif;"
            "   background: transparent;"
            "   border: none;"
            "}"
            ).arg(themeManager.textColor())
        );
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setWordWrap(true);
    nameLabel->setMaximumWidth(170);
    layout->addWidget(nameLabel);

    if (isPinned) {
        QLabel* pinnedLabel = new QLabel("Закреплено", card);
        pinnedLabel->setStyleSheet(
            QString(
                "QLabel {"
                "   color: %1;"
                "   font-size: 10px;"
                "   font-weight: 700;"
                "   font-family: 'Segoe UI', 'Arial', sans-serif;"
                "   background: transparent;"
                "   border: none;"
                "}"
                ).arg(themeManager.warningColor())
            );
        pinnedLabel->setAlignment(Qt::AlignCenter);
        layout->addWidget(pinnedLabel);
    }

    card->installEventFilter(this);
    card->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(card, &QFrame::customContextMenuRequested, this, [this, path](const QPoint& pos) {
        m_currentContextPath = path;
        showCardContextMenu(pos);
    });

    return card;
}

void StartupPage::showCardContextMenu(const QPoint& pos)
{
    QMenu menu(this);
    ThemeManager& themeManager = ThemeManager::instance();
    menu.setStyleSheet(
        QString(
            "QMenu {"
            "   background-color: %1;"
            "   border: 1px solid %2;"
            "   border-radius: 10px;"
            "   padding: 6px;"
            "}"
            "QMenu::item {"
            "   padding: 8px 24px;"
            "   border-radius: 6px;"
            "   color: %3;"
            "   font-family: 'Segoe UI', 'Arial', sans-serif;"
            "   font-size: 13px;"
            "}"
            "QMenu::item:selected {"
            "   background-color: %4;"
            "   color: %5;"
            "}"
            "QMenu::separator {"
            "   height: 1px;"
            "   background: %2;"
            "   margin: 4px 8px;"
            "}"
            ).arg(themeManager.cardBackground(),
                  themeManager.borderColor(),
                  themeManager.textColor(),
                  themeManager.hoverColor(),
                  themeManager.accentColor())
        );

    bool isPinned = m_pinnedProducts.contains(m_currentContextPath);

    QAction* actionPin = new QAction(isPinned ? "📌 Открепить" : "📌 Закрепить", this);
    connect(actionPin, &QAction::triggered, [this, isPinned]() {
        if (isPinned) {
            onUnpinCard(m_currentContextPath);
        } else {
            onPinCard(m_currentContextPath);
        }
    });
    menu.addAction(actionPin);

    menu.exec(QCursor::pos());
}

void StartupPage::onPinCard(const QString& productPath)
{
    m_pinnedProducts.insert(productPath);
    savePinnedState();
    loadProducts();
}

void StartupPage::onUnpinCard(const QString& productPath)
{
    m_pinnedProducts.remove(productPath);
    savePinnedState();
    loadProducts();
}

void StartupPage::savePinnedState()
{
    QSettings settings("KDManager", "StartupPage");
    QStringList pinnedList = m_pinnedProducts.values();
    settings.setValue("pinnedProducts", pinnedList.join("|"));
}

void StartupPage::loadPinnedState()
{
    QSettings settings("KDManager", "StartupPage");
    QString pinnedStr = settings.value("pinnedProducts", "").toString();

    if (!pinnedStr.isEmpty()) {
        QStringList pinnedList = pinnedStr.split("|", Qt::SkipEmptyParts);
        for (const QString& path : pinnedList) {
            if (m_db.folders().contains(path) &&
                m_db.folders()[path].type == TYPE_PRODUCT) {
                m_pinnedProducts.insert(path);
            }
        }
    }
}

bool StartupPage::eventFilter(QObject* obj, QEvent* event)
{
    QFrame* card = qobject_cast<QFrame*>(obj);

    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            if (card) {
                QString path = card->property("productPath").toString();
                if (!path.isEmpty()) {
                    // Получаем имя изделия из карточки
                    QList<QLabel*> labels = card->findChildren<QLabel*>();
                    QString productName;
                    for (QLabel* label : labels) {
                        if (label->objectName().isEmpty() && !label->text().isEmpty() && label->text() != "📌" && label->text() != "Закреплено") {
                            productName = label->text();
                            break;
                        }
                    }
                    emit showProductTree(productName, path);
                }
            }
        } else if (mouseEvent->button() == Qt::RightButton) {
            if (card) {
                QString path = card->property("productPath").toString();
                if (!path.isEmpty()) {
                    // Получаем имя изделия из карточки
                    QList<QLabel*> labels = card->findChildren<QLabel*>();
                    QString productName;
                    for (QLabel* label : labels) {
                        if (label->objectName().isEmpty() && !label->text().isEmpty() && label->text() != "📌" && label->text() != "Закреплено") {
                            productName = label->text();
                            break;
                        }
                    }
                    showCardContextMenu(path, productName, mouseEvent->globalPos());
                }
            }
        }
    } else if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
        if (card && card->objectName() == "productCard") {
            auto* effect = qobject_cast<QGraphicsDropShadowEffect*>(card->graphicsEffect());
            if (effect) {
                bool hovering = (event->type() == QEvent::Enter);

                auto* blurAnim = new QPropertyAnimation(effect, "blurRadius", effect);
                blurAnim->setDuration(160);
                blurAnim->setEndValue(hovering ? 40 : 22);
                blurAnim->setEasingCurve(QEasingCurve::OutCubic);
                blurAnim->start(QAbstractAnimation::DeleteWhenStopped);

                auto* yAnim = new QPropertyAnimation(effect, "yOffset", effect);
                yAnim->setDuration(160);
                yAnim->setEndValue(hovering ? 14 : 8);
                yAnim->setEasingCurve(QEasingCurve::OutCubic);
                yAnim->start(QAbstractAnimation::DeleteWhenStopped);

                ThemeManager& themeManager = ThemeManager::instance();
                QColor targetColor = hovering
                                         ? QColor(themeManager.accentColor()).lighter(105)
                                         : QColor(30, 30, 46);
                targetColor.setAlpha(hovering ? 75 : 20);

                auto* colorAnim = new QPropertyAnimation(effect, "color", effect);
                colorAnim->setDuration(160);
                colorAnim->setEndValue(targetColor);
                colorAnim->start(QAbstractAnimation::DeleteWhenStopped);
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void StartupPage::showCardContextMenu(const QString& productPath, const QString& productName, const QPoint& pos)
{
    QMenu menu(this);
    ThemeManager& themeManager = ThemeManager::instance();
    menu.setStyleSheet(
        QString(
            "QMenu {"
            "   background-color: %1;"
            "   border: 1px solid %2;"
            "   border-radius: 10px;"
            "   padding: 6px;"
            "}"
            "QMenu::item {"
            "   padding: 8px 24px;"
            "   border-radius: 6px;"
            "   color: %3;"
            "   font-family: 'Segoe UI', 'Arial', sans-serif;"
            "   font-size: 13px;"
            "}"
            "QMenu::item:selected {"
            "   background-color: %4;"
            "   color: %5;"
            "}"
            "QMenu::separator {"
            "   height: 1px;"
            "   background: %2;"
            "   margin: 4px 8px;"
            "}"
            ).arg(themeManager.cardBackground(),
                  themeManager.borderColor(),
                  themeManager.textColor(),
                  themeManager.hoverColor(),
                  themeManager.accentColor())
        );

    bool isPinned = m_pinnedProducts.contains(productPath);

    QAction* actionPin = new QAction(isPinned ? "📌 Открепить" : "📌 Закрепить", this);
    connect(actionPin, &QAction::triggered, [this, isPinned, productPath]() {
        if (isPinned) {
            onUnpinCard(productPath);
        } else {
            onPinCard(productPath);
        }
    });
    menu.addAction(actionPin);

    QAction* actionOpenTree = new QAction("🌳 Открыть интерактивное дерево", this);
    connect(actionOpenTree, &QAction::triggered, [this, productName, productPath]() {
        emit openInteractiveTree(productName, productPath);
    });
    menu.addAction(actionOpenTree);

    menu.exec(pos);
}

void StartupPage::onSearchTextChanged(const QString& text)
{
    QString searchText = text.toLower().trimmed();
    for (int i = 0; i < m_productsLayout->count(); ++i) {
        QLayoutItem* item = m_productsLayout->itemAt(i);
        if (item && item->widget()) {
            QWidget* widget = item->widget();
            QList<QLabel*> labels = widget->findChildren<QLabel*>();
            bool visible = true;
            if (!searchText.isEmpty()) {
                visible = false;
                for (QLabel* label : labels) {
                    if (label->text().toLower().contains(searchText)) {
                        visible = true;
                        break;
                    }
                }
            }
            widget->setVisible(visible);
        }
    }
}
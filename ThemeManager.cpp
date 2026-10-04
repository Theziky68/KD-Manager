#include "ThemeManager.h"
#include <QApplication>
#include <QSettings>
#include <QPalette>
#include "UserConfig.h"

ThemeManager::ThemeManager()
    : m_currentTheme(Light)
{
    // Загружаем сохраненную тему из пользовательских настроек
    int themeIndex = UserConfig::instance().getTheme();
    m_currentTheme = static_cast<Theme>(themeIndex);
}

ThemeManager& ThemeManager::instance()
{
    static ThemeManager instance;
    return instance;
}

void ThemeManager::setTheme(Theme theme)
{
    if (m_currentTheme == theme) return;

    m_currentTheme = theme;

    // Применяем тему ко всему приложению
    QString styleSheet = getStyleSheet();
    qApp->setStyleSheet(styleSheet);

    // Сохраняем выбор темы в пользовательские настройки
    UserConfig::instance().setTheme(static_cast<int>(theme));

    emit themeChanged(theme);
}

QString ThemeManager::getStyleSheet() const
{
    switch (m_currentTheme) {
    case Dark:
        return getDarkThemeStyleSheet();
    case Light:
    case System:
    default:
        return getLightThemeStyleSheet();
    }
}

QString ThemeManager::backgroundColor() const
{
    return m_currentTheme == Dark ? "#1F2937" : "#F9FAFB";
}

QString ThemeManager::textColor() const
{
    return m_currentTheme == Dark ? "#F9FAFB" : "#111827";
}

QString ThemeManager::accentColor() const
{
    return "#6366F1"; // Индиго - одинаковый для обеих тем
}

QString ThemeManager::borderColor() const
{
    return m_currentTheme == Dark ? "#374151" : "#E5E7EB";
}

QString ThemeManager::hoverColor() const
{
    return m_currentTheme == Dark ? "#374151" : "#F3F4F6";
}

QString ThemeManager::cardBackground() const
{
    return m_currentTheme == Dark ? "#111827" : "#FFFFFF";
}

QString ThemeManager::secondaryTextColor() const
{
    return m_currentTheme == Dark ? "#9CA3AF" : "#6B7280";
}

QString ThemeManager::primaryGradientStart() const
{
    return m_currentTheme == Dark ? "#4F46E5" : "#6C5CE7";
}

QString ThemeManager::primaryGradientEnd() const
{
    return m_currentTheme == Dark ? "#4338CA" : "#5541D6";
}

QString ThemeManager::secondaryGradientStart() const
{
    return m_currentTheme == Dark ? "#374151" : "#E8E8E8";
}

QString ThemeManager::secondaryGradientEnd() const
{
    return m_currentTheme == Dark ? "#1F2937" : "#D5D5D5";
}

QString ThemeManager::separatorColor() const
{
    return m_currentTheme == Dark ? "#374151" : "#D0D0D0";
}

QString ThemeManager::successColor() const
{
    return "#4CAF50";
}

QString ThemeManager::warningColor() const
{
    return "#FF9800";
}

QString ThemeManager::errorColor() const
{
    return "#F44336";
}

QString ThemeManager::getLightThemeStyleSheet() const
{
    return R"(
        /* ===== ОСНОВНЫЕ ЭЛЕМЕНТЫ ===== */
        QMainWindow, QDialog, QWidget {
            background-color: #F9FAFB;
            color: #111827;
        }

        /* ===== МЕНЮ ===== */
        QMenuBar {
            background-color: #FFFFFF;
            border-bottom: 1px solid #E5E7EB;
            color: #374151;
            padding: 4px;
        }

        QMenuBar::item {
            background-color: transparent;
            padding: 6px 12px;
            border-radius: 4px;
        }

        QMenuBar::item:selected {
            background-color: #F3F4F6;
            color: #6366F1;
        }

        QMenu {
            background-color: #FFFFFF;
            border: 1px solid #E5E7EB;
            border-radius: 8px;
            padding: 4px;
        }

        QMenu::item {
            padding: 8px 24px 8px 12px;
            border-radius: 4px;
            color: #374151;
        }

        QMenu::item:selected {
            background-color: #EEF2FF;
            color: #4F46E5;
        }

        /* ===== КНОПКИ ===== */
        QPushButton {
            background-color: #FFFFFF;
            border: 1px solid #D1D5DB;
            border-radius: 6px;
            padding: 8px 16px;
            color: #374151;
            font-weight: 500;
        }

        QPushButton:hover {
            background-color: #F9FAFB;
            border-color: #9CA3AF;
        }

        QPushButton:pressed {
            background-color: #F3F4F6;
        }

        QPushButton:disabled {
            background-color: #F3F4F6;
            color: #9CA3AF;
            border-color: #E5E7EB;
        }

        /* ===== ПОЛЯ ВВОДА ===== */
        QLineEdit, QTextEdit, QPlainTextEdit {
            background-color: #FFFFFF;
            border: 1px solid #D1D5DB;
            border-radius: 6px;
            padding: 8px 12px;
            color: #111827;
            selection-background-color: #6366F1;
            selection-color: white;
        }

        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
            border: 2px solid #6366F1;
            padding: 7px 11px;
        }

        QLineEdit:disabled, QTextEdit:disabled {
            background-color: #F3F4F6;
            color: #9CA3AF;
        }

        /* ===== СПИСКИ ===== */
        QListWidget, QTreeWidget, QTableWidget {
            background-color: #FFFFFF;
            border: 1px solid #E5E7EB;
            border-radius: 8px;
            color: #111827;
        }

        QListWidget::item, QTreeWidget::item, QTableWidget::item {
            padding: 4px;
            border-radius: 4px;
        }

        QListWidget::item:hover, QTreeWidget::item:hover, QTableWidget::item:hover {
            background-color: #F3F4F6;
        }

        QListWidget::item:selected, QTreeWidget::item:selected, QTableWidget::item:selected {
            background-color: #EEF2FF;
            color: #4F46E5;
        }

        /* ===== ТАБЛИЦЫ ===== */
        QTableWidget {
            gridline-color: #E5E7EB;
        }

        QHeaderView::section {
            background-color: #F9FAFB;
            border: none;
            border-bottom: 1px solid #E5E7EB;
            border-right: 1px solid #E5E7EB;
            padding: 8px 12px;
            color: #6B7280;
            font-weight: 600;
        }

        /* ===== СКРОЛЛБАРЫ ===== */
        QScrollBar:vertical {
            background-color: #F9FAFB;
            width: 10px;
            border-radius: 5px;
        }

        QScrollBar::handle:vertical {
            background-color: #D1D5DB;
            border-radius: 5px;
            min-height: 20px;
        }

        QScrollBar::handle:vertical:hover {
            background-color: #9CA3AF;
        }

        QScrollBar:horizontal {
            background-color: #F9FAFB;
            height: 10px;
            border-radius: 5px;
        }

        QScrollBar::handle:horizontal {
            background-color: #D1D5DB;
            border-radius: 5px;
            min-width: 20px;
        }

        QScrollBar::handle:horizontal:hover {
            background-color: #9CA3AF;
        }

        QScrollBar::add-line, QScrollBar::sub-line {
            border: none;
            background: none;
        }

        /* ===== ЧЕКБОКСЫ ===== */
        QCheckBox {
            color: #374151;
            spacing: 8px;
        }

        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border: 2px solid #D1D5DB;
            border-radius: 4px;
            background-color: #FFFFFF;
        }

        QCheckBox::indicator:checked {
            background-color: #6366F1;
            border-color: #6366F1;
        }

        /* ===== ГРУППЫ ===== */
        QGroupBox {
            background-color: #FFFFFF;
            border: 1px solid #E5E7EB;
            border-radius: 8px;
            margin-top: 12px;
            padding: 16px;
            font-weight: 600;
            color: #111827;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 8px;
            background-color: #FFFFFF;
        }

        /* ===== DOCK WIDGET ===== */
        QDockWidget {
            background-color: #FFFFFF;
            border: 1px solid #E5E7EB;
            titlebar-close-icon: url(none);
            titlebar-normal-icon: url(none);
        }

        QDockWidget::title {
            background-color: #6366F1;
            color: #FFFFFF;
            padding: 10px 14px;
            font-weight: 700;
        }

        /* ===== СТАТУС БАР ===== */
        QStatusBar {
            background-color: #FFFFFF;
            border-top: 1px solid #E5E7EB;
            color: #6B7280;
        }

        /* ===== ВКЛАДКИ ===== */
        QTabWidget::pane {
            border: none;
            background-color: transparent;
        }

        QTabBar::tab {
            background-color: #FFFFFF;
            border: 1px solid #E5E7EB;
            border-bottom: none;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            padding: 10px 20px;
            margin-right: 4px;
            color: #6B7280;
        }

        QTabBar::tab:selected {
            background-color: #F9FAFB;
            color: #6366F1;
            font-weight: 600;
        }

        QTabBar::tab:hover {
            background-color: #F9FAFB;
        }

        /* ===== КОМБОБОКСЫ ===== */
        QComboBox {
            background-color: #FFFFFF;
            border: 1px solid #D1D5DB;
            border-radius: 6px;
            padding: 6px 12px;
            color: #111827;
        }

        QComboBox::drop-down {
            border: none;
            width: 20px;
        }

        QComboBox QAbstractItemView {
            background-color: #FFFFFF;
            border: 1px solid #D1D5DB;
            border-radius: 6px;
            selection-background-color: #EEF2FF;
            selection-color: #4F46E5;
            color: #111827;
            padding: 4px;
        }

        QComboBox QAbstractItemView::item {
            padding: 8px 12px;
            border-radius: 4px;
        }

        QComboBox QAbstractItemView::item:hover {
            background-color: #F3F4F6;
        }

        /* ===== СПИНБОКСЫ ===== */
        QSpinBox {
            background-color: #FFFFFF;
            border: 1px solid #D1D5DB;
            border-radius: 6px;
            padding: 6px 12px;
            color: #111827;
        }

        /* ===== СЛАЙДЕРЫ ===== */
        QSlider::groove:horizontal {
            background-color: #E5E7EB;
            height: 4px;
            border-radius: 2px;
        }

        QSlider::handle:horizontal {
            background-color: #6366F1;
            width: 16px;
            height: 16px;
            margin: -6px 0;
            border-radius: 8px;
        }

        QSlider::handle:horizontal:hover {
            background-color: #4F46E5;
        }

        /* ===== ПРОГРЕСС БАРЫ ===== */
        QProgressBar {
            background-color: #E5E7EB;
            border: none;
            border-radius: 4px;
            text-align: center;
            color: #6B7280;
        }

        QProgressBar::chunk {
            background-color: #6366F1;
            border-radius: 4px;
        }

        /* ===== ТУЛТИПЫ ===== */
        QToolTip {
            background-color: #111827;
            color: #F9FAFB;
            border: none;
            border-radius: 6px;
            padding: 6px 10px;
        }
    )";
}

QString ThemeManager::getDarkThemeStyleSheet() const
{
    return R"(
        /* ===== ОСНОВНЫЕ ЭЛЕМЕНТЫ ===== */
        QMainWindow, QDialog, QWidget {
            background-color: #1F2937;
            color: #F9FAFB;
        }

        /* ===== МЕНЮ ===== */
        QMenuBar {
            background-color: #111827;
            border-bottom: 1px solid #374151;
            color: #D1D5DB;
            padding: 4px;
        }

        QMenuBar::item {
            background-color: transparent;
            padding: 6px 12px;
            border-radius: 4px;
        }

        QMenuBar::item:selected {
            background-color: #374151;
            color: #818CF8;
        }

        QMenu {
            background-color: #111827;
            border: 1px solid #374151;
            border-radius: 8px;
            padding: 4px;
        }

        QMenu::item {
            padding: 8px 24px 8px 12px;
            border-radius: 4px;
            color: #D1D5DB;
        }

        QMenu::item:selected {
            background-color: #1F2937;
            color: #818CF8;
        }

        /* ===== КНОПКИ ===== */
        QPushButton {
            background-color: #374151;
            border: 1px solid #4B5563;
            border-radius: 6px;
            padding: 8px 16px;
            color: #F9FAFB;
            font-weight: 500;
        }

        QPushButton:hover {
            background-color: #4B5563;
            border-color: #6B7280;
        }

        QPushButton:pressed {
            background-color: #1F2937;
        }

        QPushButton:disabled {
            background-color: #1F2937;
            color: #6B7280;
            border-color: #374151;
        }

        /* ===== ПОЛЯ ВВОДА ===== */
        QLineEdit, QTextEdit, QPlainTextEdit {
            background-color: #111827;
            border: 1px solid #374151;
            border-radius: 6px;
            padding: 8px 12px;
            color: #F9FAFB;
            selection-background-color: #6366F1;
            selection-color: white;
        }

        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
            border: 2px solid #6366F1;
            padding: 7px 11px;
        }

        QLineEdit:disabled, QTextEdit:disabled {
            background-color: #1F2937;
            color: #6B7280;
        }

        /* ===== СПИСКИ ===== */
        QListWidget, QTreeWidget, QTableWidget {
            background-color: #111827;
            border: 1px solid #374151;
            border-radius: 8px;
            color: #F9FAFB;
        }

        QListWidget::item, QTreeWidget::item, QTableWidget::item {
            padding: 4px;
            border-radius: 4px;
        }

        QListWidget::item:hover, QTreeWidget::item:hover, QTableWidget::item:hover {
            background-color: #374151;
        }

        QListWidget::item:selected, QTreeWidget::item:selected, QTableWidget::item:selected {
            background-color: #1F2937;
            color: #818CF8;
        }

        /* ===== ТАБЛИЦЫ ===== */
        QTableWidget {
            gridline-color: #374151;
        }

        QHeaderView::section {
            background-color: #1F2937;
            border: none;
            border-bottom: 1px solid #374151;
            border-right: 1px solid #374151;
            padding: 8px 12px;
            color: #9CA3AF;
            font-weight: 600;
        }

        /* ===== СКРОЛЛБАРЫ ===== */
        QScrollBar:vertical {
            background-color: #1F2937;
            width: 10px;
            border-radius: 5px;
        }

        QScrollBar::handle:vertical {
            background-color: #4B5563;
            border-radius: 5px;
            min-height: 20px;
        }

        QScrollBar::handle:vertical:hover {
            background-color: #6B7280;
        }

        QScrollBar:horizontal {
            background-color: #1F2937;
            height: 10px;
            border-radius: 5px;
        }

        QScrollBar::handle:horizontal {
            background-color: #4B5563;
            border-radius: 5px;
            min-width: 20px;
        }

        QScrollBar::handle:horizontal:hover {
            background-color: #6B7280;
        }

        QScrollBar::add-line, QScrollBar::sub-line {
            border: none;
            background: none;
        }

        /* ===== ЧЕКБОКСЫ ===== */
        QCheckBox {
            color: #D1D5DB;
            spacing: 8px;
        }

        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border: 2px solid #4B5563;
            border-radius: 4px;
            background-color: #111827;
        }

        QCheckBox::indicator:checked {
            background-color: #6366F1;
            border-color: #6366F1;
        }

        /* ===== ГРУППЫ ===== */
        QGroupBox {
            background-color: #111827;
            border: 1px solid #374151;
            border-radius: 8px;
            margin-top: 12px;
            padding: 16px;
            font-weight: 600;
            color: #F9FAFB;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 8px;
            background-color: #111827;
        }

        /* ===== DOCK WIDGET ===== */
        QDockWidget {
            background-color: #111827;
            border: 1px solid #374151;
            titlebar-close-icon: url(none);
            titlebar-normal-icon: url(none);
        }

        QDockWidget::title {
            background-color: #6366F1;
            color: #FFFFFF;
            padding: 10px 14px;
            font-weight: 700;
        }

        /* ===== СТАТУС БАР ===== */
        QStatusBar {
            background-color: #111827;
            border-top: 1px solid #374151;
            color: #9CA3AF;
        }

        /* ===== ВКЛАДКИ ===== */
        QTabWidget::pane {
            border: none;
            background-color: transparent;
        }

        QTabBar::tab {
            background-color: #111827;
            border: 1px solid #374151;
            border-bottom: none;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            padding: 10px 20px;
            margin-right: 4px;
            color: #9CA3AF;
        }

        QTabBar::tab:selected {
            background-color: #1F2937;
            color: #818CF8;
            font-weight: 600;
        }

        QTabBar::tab:hover {
            background-color: #1F2937;
        }

        /* ===== КОМБОБОКСЫ ===== */
        QComboBox {
            background-color: #111827;
            border: 1px solid #374151;
            border-radius: 6px;
            padding: 6px 12px;
            color: #F9FAFB;
        }

        QComboBox::drop-down {
            border: none;
            width: 20px;
        }

        QComboBox QAbstractItemView {
            background-color: #111827;
            border: 1px solid #374151;
            border-radius: 6px;
            selection-background-color: #1F2937;
            selection-color: #818CF8;
            color: #F9FAFB;
            padding: 4px;
        }

        QComboBox QAbstractItemView::item {
            padding: 8px 12px;
            border-radius: 4px;
        }

        QComboBox QAbstractItemView::item:hover {
            background-color: #374151;
        }

        /* ===== СПИНБОКСЫ ===== */
        QSpinBox {
            background-color: #111827;
            border: 1px solid #374151;
            border-radius: 6px;
            padding: 6px 12px;
            color: #F9FAFB;
        }

        /* ===== СЛАЙДЕРЫ ===== */
        QSlider::groove:horizontal {
            background-color: #374151;
            height: 4px;
            border-radius: 2px;
        }

        QSlider::handle:horizontal {
            background-color: #6366F1;
            width: 16px;
            height: 16px;
            margin: -6px 0;
            border-radius: 8px;
        }

        QSlider::handle:horizontal:hover {
            background-color: #818CF8;
        }

        /* ===== ПРОГРЕСС БАРЫ ===== */
        QProgressBar {
            background-color: #374151;
            border: none;
            border-radius: 4px;
            text-align: center;
            color: #9CA3AF;
        }

        QProgressBar::chunk {
            background-color: #6366F1;
            border-radius: 4px;
        }

        /* ===== ТУЛТИПЫ ===== */
        QToolTip {
            background-color: #F9FAFB;
            color: #111827;
            border: none;
            border-radius: 6px;
            padding: 6px 10px;
        }
    )";
}

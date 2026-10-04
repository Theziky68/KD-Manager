#include <QApplication>
#include <QGuiApplication>
#include <QFile>
#include <QDateTime>
#include <windows.h>
#include <objbase.h>

#include "DatabaseManager.h"
#include "MainWindow.h"
#include "BackupManager.h"
#include "ChangesPanel.h"
#include "NotificationManager.h"
#include "ThemeManager.h"
#include "UserConfig.h"
#include "UserSetupDialog.h"
#include "OnlineUsersManager.h"

int main(int argc, char *argv[])
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough
        );
#endif

    QApplication app(argc, argv);

    // Инициализируем COM для работы с ShellExecute
    CoInitialize(NULL);

    // ============================================================
    // ПРОВЕРКА ПЕРВОГО ЗАПУСКА И ВВОД ФИО
    // ============================================================
    UserConfig& userConfig = UserConfig::instance();
    if (userConfig.isFirstRun()) {
        UserSetupDialog setupDialog;
        if (setupDialog.exec() == QDialog::Accepted) {
            userConfig.setUserFIO(setupDialog.getUserFIO());
        }
    }

    // ============================================================
    // ИНИЦИАЛИЗАЦИЯ БАЗЫ ДАННЫХ
    // ============================================================
    DatabaseManager& db = DatabaseManager::instance();

    qDebug() << "=== ПРОВЕРКА ПОСЛЕ ИНИЦИАЛИЗАЦИИ БД ===";
    qDebug() << "Папок в m_folders:" << db.folders().size();

    QSqlQuery checkQuery(db.getDatabase());
    checkQuery.exec("SELECT path, name, type FROM folders ORDER BY path");
    qDebug() << "=== СОДЕРЖИМОЕ ТАБЛИЦЫ folders ===";
    while (checkQuery.next()) {
        qDebug() << "  path:" << checkQuery.value(0).toString()
        << "name:" << checkQuery.value(1).toString()
        << "type:" << checkQuery.value(2).toInt();
    }
    qDebug() << "=== КОНЕЦ ПРОВЕРКИ ===";

    // ============================================================
    // ИНИЦИАЛИЗАЦИЯ МЕНЕДЖЕРА БЭКАПОВ
    // ============================================================
    BackupManager& backup = BackupManager::instance();
    backup.setDatabaseManager(&db);
    backup.start();

    // Сохраняем ФИО для отключения при выходе
    QString currentUserFIO = userConfig.getUserFIO();

    // ===== РЕГИСТРАЦИЯ СЕССИИ ПОЛЬЗОВАТЕЛЯ (v0.21) =====
    db.registerUserSession(currentUserFIO);
    qDebug() << "✅ Сессия пользователя зарегистрирована:" << currentUserFIO;

    OnlineUsersManager::instance().setUserOnline(currentUserFIO);
    qDebug() << "✅ Пользователь зарегистрирован онлайн:" << currentUserFIO;

    // ============================================================
    // СОЗДАНИЕ ГЛАВНОГО ОКНА
    // ============================================================
    MainWindow window;
    window.show();

    // ============================================================
    // ТЕСТОВОЕ УВЕДОМЛЕНИЕ
    // ============================================================
    qDebug() << "🔍 Отправляем тестовое уведомление...";
    NotificationManager::Notification testNotif;
    testNotif.type = NotificationManager::TYPE_SUCCESS;
    testNotif.title = "✅ Приложение запущено";
    testNotif.message = "Менеджер документов готов к работе";
    testNotif.timestamp = QDateTime::currentDateTime();
    testNotif.isRead = false;
    testNotif.isImportant = false;
    testNotif.sourceUser = currentUserFIO;
    NotificationManager::instance().addNotification("", testNotif);
    qDebug() << "✅ Тестовое уведомление отправлено";

    // ============================================================
    // ЗАПУСК ЦИКЛА СОБЫТИЙ
    // ============================================================
    int result = app.exec();

    // ============================================================
    // ЗАВЕРШЕНИЕ РАБОТЫ
    // ============================================================
    qDebug() << "=== ЗАВЕРШЕНИЕ РАБОТЫ ===";

    // ===== ЗАКРЫТИЕ СЕССИИ ПОЛЬЗОВАТЕЛЯ (v0.21) =====
    db.closeUserSession(currentUserFIO);
    qDebug() << "✅ Сессия пользователя закрыта:" << currentUserFIO;

    // Отключаем пользователя от онлайн
    OnlineUsersManager::instance().setUserOffline(currentUserFIO);
    qDebug() << "✅ Пользователь отключен от онлайн:" << currentUserFIO;

    // Сохраняем базу данных
    if (db.isDirty()) {
        qDebug() << "💾 Сохранение базы данных перед выходом...";
        db.forceSave();
    }

    // Завершаем COM
    CoUninitialize();

    qDebug() << "✅ Программа завершена";

    return result;
}

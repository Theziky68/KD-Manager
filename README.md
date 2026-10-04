# KD-Manager

Программа для управления конструкторской документацией. Писал для себя и коллег, чтобы не терять чертежи и модели по папкам.

## Что умеет

- Хранит документы в структуре: изделие → сборка → деталь → спецификация
- Открывает файлы Компаса напрямую (.cdw, .frw, .spw, .m3d, .a3d)
- Работает локально (SQLite) или по сети (PostgreSQL) — можно работать командой
- Есть поиск по файлам
- Комментарии к документам, статусы (в работе, на проверке, утверждён)
- Уведомления об изменениях
- Тёмная и светлая темы
- Автоматические бэкапы каждый час

## Как собрать

Нужен Qt 6, CMake 3.16+, компилятор с C++17.

```bash
git clone https://github.com/Theziky68/KD-Manager.git
cd KD-Manager
mkdir build && cd build
cmake ..
cmake --build . --parallel
```

Потом запустить `KDManager.exe` (или `./KDManager` на Linux).

## Как настроить

При первом запуске спросит ФИО и предложит выбрать базу данных.

**Если работаете один** — выбирайте SQLite, укажите путь к файлу базы (например `kd.db` рядом с программой).

**Если работаете командой** — нужен PostgreSQL. На сервере создайте базу:

```sql
CREATE USER kd_user WITH PASSWORD 'пароль';
CREATE DATABASE kd_documents OWNER kd_user;
```

Потом в настройках программы укажите адрес сервера, порт, логин, пароль и имя базы.

## Структура

```
KDManager/
├── main.cpp
├── MainWindow.cpp/.h        — главное окно
├── DatabaseManager.cpp/.h   — работа с базой
├── DatabaseAbstraction.cpp/.h — запросы под SQLite и PostgreSQL
├── StartupPage.cpp/.h       — стартовая страница
├── ProductTreePage.cpp/.h   — дерево изделия
├── CurvedTreeWidget.cpp/.h  — визуализация дерева
├── FileCardWidget.cpp/.h    — карточки файлов
├── PreviewWidget.cpp/.h     — превью
├── CommentDialog.cpp/.h     — комментарии
├── NotificationManager.cpp/.h — уведомления
├── BackupManager.cpp/.h     — бэкапы
├── ThemeManager.cpp/.h      — темы
└── ... остальные файлы
```

## Что в планах

- Проверка комплектности по спецификации (чтобы находил, каких чертежей не хватает)
- Экспорт отчётов в PDF

## Лицензия

MIT. Делайте что хотите.
Принимаю предложения замечания и оскорбления. Только учусь


<img width="3440" height="1393" alt="main-screen" src="https://github.com/user-attachments/assets/71d89a33-f882-4e42-ad0f-bd2da8349b4a" />


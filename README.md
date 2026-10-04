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
<img width="3179" height="1387" alt="{D377037B-48BF-4B16-B998-A60D0267F6E8}" src="https://github.com/user-attachments/assets/966582c3-4710-49e0-bc17-32a716ea65db" />
<img width="3172" height="1384" alt="{A57F2F52-A1D2-441B-8C5A-FF65BED5CA66}" src="https://github.com/user-attachments/assets/74c56284-cb18-426c-93a8-275d0fe0b686" />
<img width="3171" height="1377" alt="{F289C6B7-E1F2-4F1F-9992-9FC53C2D01F0}" src="https://github.com/user-attachments/assets/707ef445-e8c4-4946-8b82-d46b3ba3187d" />
<img width="1192" height="676" alt="{BEB2B0E4-54E5-4D3B-9662-E011748E8316}" src="https://github.com/user-attachments/assets/7ce9be5f-bbd6-4b79-b3c1-0ef75785709a" />
<img width="897" height="616" alt="{A3A17189-9B51-49AC-ADC6-8FD28F070DA1}" src="https://github.com/user-attachments/assets/d9228936-5d56-4dcc-8fe6-068a240a8e36" />
<img width="689" height="570" alt="{30812B91-BA35-4BCF-806E-B7CC6C3DE025}" src="https://github.com/user-attachments/assets/6d785e48-d2fa-496c-ba3b-b3c36d2afff5" />
<img width="694" height="574" alt="{1FA57F53-7EF3-41AD-A725-CABE2766DB14}" src="https://github.com/user-attachments/assets/9b067538-18f0-4b0a-ab08-0cf4e53b793c" />
<img width="357" height="484" alt="{C3D6E240-150F-4ED9-B7E5-30A1F0A91CB9}" src="https://github.com/user-attachments/assets/4b603614-13fc-4157-b9ce-9185377817fa" />
<img width="956" height="664" alt="{BC6BB0DC-B0DB-4C69-823C-E27C923187A6}" src="https://github.com/user-attachments/assets/38112e7c-580d-4bad-9ed1-3be7589042ef" />











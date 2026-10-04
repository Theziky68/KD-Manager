cmake_minimum_required(VERSION 3.16)
project(KDManager VERSION 0.2 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===== ОПТИМИЗАЦИЯ ДЛЯ NINJA =====
if(MSVC)
    # Параллельная компиляция
    add_compile_options(/MP)

    # Debug - минимум оптимизаций для скорости
    string(APPEND CMAKE_CXX_FLAGS_DEBUG " /Od")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Sql Network Svg OpenGL OpenGLWidgets PrintSupport)

set(SOURCES
    main.cpp
    SettingsDialog.cpp
    MainWindow.cpp
    DatabaseManager.cpp
    DatabaseAbstraction.cpp
    DatabaseSetupDialog.cpp
    FolderData.cpp
    PreviewWidget.cpp
    SearchDialog.cpp
    TasksDialog.cpp
    NotificationManager.cpp
    ChangesPanel.cpp
    CommentDialog.cpp
    ThemeManager.cpp
    Model3DViewer.cpp
    StartupPage.cpp
    PinDialog.cpp
    BackupManager.cpp
    AddFilesDialog.cpp
    AboutDialog.cpp
    FileCardWidget.cpp
    FileCardContainer.cpp
    TreeItemWidget.cpp
    NotificationPanel.cpp
    UserConfig.cpp
    UserSetupDialog.cpp
    OnlineUsersManager.cpp
    OnlineUsersPanel.cpp
    InteractiveTreeView.cpp
    ProductTreePage.cpp
)

set(HEADERS
    SettingsDialog.h
    MainWindow.h
    DatabaseManager.h
    DatabaseAbstraction.h
    DatabaseSetupDialog.h
    FolderData.h
    PreviewWidget.h
    SearchDialog.h
    TasksDialog.h
    NotificationManager.h
    ChangesPanel.h
    CommentDialog.h
    DocumentStatus.h
    ThemeManager.h
    Model3DViewer.h
    NotificationData.h
    StartupPage.h
    PinDialog.h
    BackupManager.h
    AddFilesDialog.h
    AboutDialog.h
    FileCardWidget.h
    FileCardContainer.h
    TreeItemWidget.h
    NotificationPanel.h
    UserConfig.h
    UserSetupDialog.h
    OnlineUsersManager.h
    OnlineUsersPanel.h
    InteractiveTreeView.h
    ProductTreePage.h
)

set_source_files_properties(main.cpp PROPERTIES SKIP_AUTOMOC ON)

add_executable(untitled ${SOURCES} ${HEADERS})

target_link_libraries(untitled
    Qt6::Core
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
    Qt6::Svg
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::PrintSupport
)

target_include_directories(untitled PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
    target_link_libraries(untitled user32 ole32 shell32)
endif()
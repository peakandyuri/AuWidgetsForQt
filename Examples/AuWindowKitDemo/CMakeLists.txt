add_executable(AuWindowKitDemo
    main.cpp
    MainWindow.cpp
    demos/DialogWindows.cpp
    demos/MyWin.cpp
    demos/MyWinContent.ui
    AuWindowKitDemo.qrc
    MainWindow.h
    demos/DialogWindows.h
    demos/MyWin.h
)

target_include_directories(AuWindowKitDemo PRIVATE
    demos
)

target_link_libraries(AuWindowKitDemo PRIVATE
    AuWindowKit
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
)

au_copy_dlls(AuWindowKitDemo
    "${CMAKE_SOURCE_DIR}/AuWindowKit/bin/AuWindowKit.dll"
)

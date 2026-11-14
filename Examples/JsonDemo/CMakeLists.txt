# JsonDemo - AuJson 速度测试示例（录入 JSON → 自动解析成树 → 显示解析耗时）
add_executable(JsonDemo
    main.cpp
    MainWindow.cpp
    MainWindow.h
)

target_link_libraries(JsonDemo PRIVATE
    AuJson
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
)

au_copy_dlls(JsonDemo
    "${CMAKE_SOURCE_DIR}/AuJson/bin/AuJson.dll"
)

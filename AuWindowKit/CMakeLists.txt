add_library(AuWindowKit SHARED
    widget/AuWindow.cpp
    widget/AuWindowWin.cpp
    widget/AuWindowX11.cpp
    widget/AuWindowFallback.cpp
    widget/AuTitleBar.cpp
    widget/AuBorderShadow.cpp
    style/AuWindowStyle.cpp
    include/AuWindowKit_global.h
    include/AuWindow.h
    include/AuTitleBar.h
    include/AuWindowStyle.h
    widget/AuWindow_p.h
    widget/AuBorderShadow.h
)

target_include_directories(AuWindowKit
    PUBLIC  include
    PRIVATE widget style
)

target_link_libraries(AuWindowKit PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
)

# AuWindowX11.cpp 引用私有头 <qpa/qwindowsysteminterface_p.h>（对应 qmake 的 gui-private）
if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(AuWindowKit PRIVATE Qt6::GuiPrivate)
else()
    target_include_directories(AuWindowKit PRIVATE ${Qt${QT_VERSION_MAJOR}Gui_PRIVATE_INCLUDE_DIRS})
endif()

# Windows：圆角区域 GDI 函数（CreateRoundRectRgn / CombineRgn 等）
if(WIN32)
    target_link_libraries(AuWindowKit PRIVATE gdi32)
endif()

# X11：_NET_WM_MOVERESIZE 原生拖动 / 缩放 + 混成检测（直接使用 Xlib）
if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_include_directories(AuWindowKit PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(AuWindowKit PRIVATE ${X11_LIBRARIES})
endif()

set_target_properties(AuWindowKit PROPERTIES DEFINE_SYMBOL AUWINDOWKIT_LIBRARY)

au_target_output_dirs(AuWindowKit)

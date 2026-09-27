#ifndef AUDATEPICKER_GLOBAL_H
#define AUDATEPICKER_GLOBAL_H

#include <QtCore/qglobal.h>

// 库导出宏：编译 AuDatePicker 库时导出符号，使用方自动导入。
#if defined(AUDATEPICKER_LIBRARY)
#  define AUDATEPICKER_EXPORT Q_DECL_EXPORT
#else
#  define AUDATEPICKER_EXPORT Q_DECL_IMPORT
#endif

#endif // AUDATEPICKER_GLOBAL_H

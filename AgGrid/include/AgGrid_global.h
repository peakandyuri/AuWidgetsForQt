#ifndef AGGRID_GLOBAL_H
#define AGGRID_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(AGGRID_LIBRARY)
#  define AGGRID_EXPORT Q_DECL_EXPORT
#else
#  define AGGRID_EXPORT Q_DECL_IMPORT
#endif

#endif // AGGRID_GLOBAL_H

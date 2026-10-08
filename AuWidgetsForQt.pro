TEMPLATE = subdirs

SUBDIRS += \
    AuCommon \
    AuDatePicker \
    AuGrid \
    AuPlot \
    AuPortableDocEditor \
    AuSelect \
    AuSnipping \
    AuWindowKit \
    Example \
    FeatureDemo \
    AuWindowKitDemo

AuGrid.depends += AuCommon
AuSelect.depends += AuCommon
AuDatePicker.depends += AuCommon
Example.depends += AuGrid AuSelect AuDatePicker AuCommon
FeatureDemo.depends += AuGrid AuSelect AuDatePicker AuCommon AuPlot
AuWindowKitDemo.depends += AuWindowKit

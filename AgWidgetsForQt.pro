TEMPLATE = subdirs

SUBDIRS += \
    AgCommon \
    AgGrid \
    AgSelect \
    AgDatePicker \
    AgPlot \
    AgWindowKit \
    Example \
    FeatureDemo \
    AgWindowKitDemo

AgGrid.depends += AgCommon
AgSelect.depends += AgCommon
AgDatePicker.depends += AgCommon
Example.depends += AgGrid AgSelect AgDatePicker AgCommon
FeatureDemo.depends += AgGrid AgSelect AgDatePicker AgCommon AgPlot
AgWindowKitDemo.depends += AgWindowKit

TEMPLATE = subdirs

SUBDIRS += \
    AgCommon \
    AgGrid \
    AgSelect \
    AgDatePicker \
    Example

AgGrid.depends += AgCommon
AgSelect.depends += AgCommon
Example.depends += AgGrid AgSelect AgCommon

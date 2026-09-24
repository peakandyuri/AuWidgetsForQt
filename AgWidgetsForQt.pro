TEMPLATE = subdirs

SUBDIRS += \
    AgCommon \
    AgGrid \
    AgSelect \
    AgDatePicker \
    Example

AgGrid.depends += AgCommon
AgSelect.depends += AgCommon
AgDatePicker.depends += AgCommon
Example.depends += AgGrid AgSelect AgDatePicker AgCommon

TEMPLATE = subdirs

SUBDIRS += \
    AgGrid \
    AgSelect \
    AgDatePicker \
    Example

Example.depends += AgGrid

TEMPLATE = subdirs

SUBDIRS += \
    AgGrid \
    Example

Example.depends += AgGrid


include(../../interfaces/interfaces.pri)

PLUGIN_NAME 	= notify

QT              += widgets svg
TEMPLATE         = lib
CONFIG          += plugin c++11 link_pkgconfig
PKGCONFIG += dtkbase dtkwidget gtk+-2.0
TARGET          = $$qtLibraryTarget($$PLUGIN_NAME)
DESTDIR          = $$_PRO_FILE_PWD_/../
DISTFILES       += $$PLUGIN_NAME.json

HEADERS += \
    notifyplugin.h \
    viewer.h \
    notifymanager.h \
    numbutton.h \
    appicon.h \
    datasourcethread.h

SOURCES += \
    notifyplugin.cpp \
    viewer.cpp \
    notifymanager.cpp \
    numbutton.cpp \
    appicon.cpp \
    datasourcethread.cpp

target.path = $${PREFIX}/lib/dde-control-center/plugins/
INSTALLS += target

RESOURCES += \
    images.qrc

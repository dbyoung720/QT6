TEMPLATE = app
TARGET = coffee
QT += quick insighttracker insighttrackerqml

CONFIG += c++11

SOURCES += main.cpp

RESOURCES += qml.qrc

OTHER_FILES += qtinsight.conf

target.path = $$[QT_INSTALL_EXAMPLES]/insighttracker/coffee
INSTALLS += target

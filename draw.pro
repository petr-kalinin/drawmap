TEMPLATE = app
DEPENDPATH += .
INCLUDEPATH += .
INCLUDEPATH += ../libosmium/include
LIBS += -lz -lproj

QMAKE_CXXFLAGS += -std=c++11

# Input
SOURCES += draw.cpp

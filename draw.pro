TEMPLATE = app
DEPENDPATH += .
INCLUDEPATH += .
INCLUDEPATH += ../libosmium/include
LIBS += -lz -lproj -lopencv_highgui -lopencv_core -lopencv_imgproc

QMAKE_CXXFLAGS += -std=c++11 -g

# Input
SOURCES += draw.cpp

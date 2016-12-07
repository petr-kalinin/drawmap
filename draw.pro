TEMPLATE = app
DEPENDPATH += .
INCLUDEPATH += .
INCLUDEPATH += ../libosmium/include
LIBS += -lz -lproj -lopencv_highgui -lopencv_core -lopencv_imgproc

QMAKE_CXXFLAGS += -std=c++14 -g

# Input
SOURCES += src/common.cpp src/draw.cpp src/osm_main.cpp src/osm_roads.cpp src/srtm.cpp src/osm_rail.cpp \
    src/osm_places.cpp src/osm_rivers.cpp

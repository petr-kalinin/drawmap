#pragma once

#include "common.h"
#include "osm_common.h"

#include <osmium/handler.hpp>

#include <QImage>
#include <QPainter>

#include <memory>

enum class RoadType {MAIN, SIDE};

class OsmRoadsHandler : public BaseHandler {
public:
    OsmRoadsHandler(const Projector& proj_, const MinMax& minmax_, int imageSize);
    
    virtual void way(const osmium::Way &way);
    
    virtual void finalize();
    
    QImage getImage() const;

    const QPainterPath& getUnitedPath() const;
    
    void setPlacesPath(const QPainterPath& path);
    
private:
    struct RoadPath {
        QPainterPath path;
        RoadType type;
        int width;
    };
    
    double scale;
    QImage image;
    QPainterPath unitedPath, mainPath, sidePath;
    std::vector<RoadPath> paths;
    const QPainterPath* placesPath;
    const Projector& proj;
    const MinMax& minmax;
}; 


#pragma once

#include "common.h"
#include "osm_common.h"

#include <osmium/handler.hpp>

#include <QImage>
#include <QPainter>

class OsmRoadsHandler : public BaseHandler {
public:
    OsmRoadsHandler(const Projector& proj_, const MinMax& minmax_, int imageSize);
    
    virtual void way(const osmium::Way &way);
    
    virtual void finalize();
    
    QImage getImage() const;

    const QPainterPath& getUnitedPath() const;
    
    void setPlacesPath(const QPainterPath& path);
    
private:
    double scale;
    QImage image;
    QPainterPath mainPath, sidePath, unitedPath;
    const QPainterPath* placesPath;
    const Projector& proj;
    const MinMax& minmax;
}; 


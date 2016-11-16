#pragma once

#include "common.h"
#include "osm_common.h"

#include <osmium/handler.hpp>

#include <QImage>
#include <QPainter>

class OsmRailHandler : public BaseHandler {
public:
    OsmRailHandler(const Projector& proj_, const MinMax& minmax_, int imageSize);
    
    virtual void way(const osmium::Way &way);
    
    QImage getImage() const;
    
private:
    double scale;
    QImage imageFill, imageOutline;
    QPainter painterFill, painterOutline;
    const Projector& proj;
    const MinMax& minmax;
}; 


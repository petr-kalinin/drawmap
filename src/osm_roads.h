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
    
    QImage getImage() const;
    
private:
    double scale;
    QImage imageFillMain, imageFillSide, imageOutline;
    QPainter painterFillMain, painterFillSide, painterOutline;
    const Projector& proj;
    const MinMax& minmax;
}; 


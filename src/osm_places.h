#pragma once

#include "common.h"
#include "osm_common.h"

#include <osmium/handler.hpp>

#include <QImage>
#include <QPainter>

class OsmPlacesHandler : public BaseHandler {
public:
    OsmPlacesHandler(const Projector& proj_, const MinMax& minmax_, int imageSize);
    
    virtual void area(const osmium::Area &area);
    
    QImage getImage() const;
    
private:
    bool needArea(const osmium::Area &area) const;
    
    double scale;
    QImage imageFill, imageOutline;
    QPainter painterFill, painterOutline;
    const Projector& proj;
    const MinMax& minmax;
}; 


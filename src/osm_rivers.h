#pragma once

#include "common.h"
#include "osm_common.h"

#include <osmium/handler.hpp>

#include <QImage>
#include <QPainter>

class OsmRiversHandler : public BaseHandler {
public:
    OsmRiversHandler(const Projector& proj_, const MinMax& minmax_, int imageSize);
    
    virtual void area(const osmium::Area &area);
    virtual void way(const osmium::Way &way);
    
    virtual void finalize();
    
    QImage getImage() const;
    
private:
    template<class Object>
    bool needObject(const Object &object) const;
    
    QPainterPath paths;
    QPainterPath areas;
    double scale;
    QImage image;
    const Projector& proj;
    const MinMax& minmax;
}; 


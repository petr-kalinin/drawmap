#pragma once

#include "common.h"
#include "osm_common.h"

#include <osmium/handler.hpp>

#include <QImage>
#include <QPainter>

class OsmForestsHandler : public BaseHandler {
public:
    OsmForestsHandler(const Projector& proj_, const MinMax& minmax_, int imageSize);
    
    virtual void area(const osmium::Area &area);
    
    virtual void finalize();
    
    QImage getImage() const;
    
    const QPainterPath& getAreas() const;
    
private:
    template<class Object>
    bool needObject(const Object &object) const;
    
    QPainterPath areas;
    double scale;
    QImage image;
    const Projector& proj;
    const MinMax& minmax;
}; 


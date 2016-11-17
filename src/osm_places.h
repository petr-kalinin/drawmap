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
    
    virtual void finalize();
    
    QImage getImage() const;
    
    const QPainterPath& getUnitedPath() const;
    
    void setRoadsPath(const QPainterPath& path);
    
private:
    bool needArea(const osmium::Area &area) const;
    
    double scale;
    QImage image;
    QPainterPath unitedPath;
    const QPainterPath* roadsPath;
    const Projector& proj;
    const MinMax& minmax;
}; 


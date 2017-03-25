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
    void setRailPath(const QPainterPath& path);
    void setForestAreas(const QPainterPath& path);
    
private:
    bool needArea(const osmium::Area &area) const;
    QPolygonF simplifyPolygon(const QPolygonF& polygon) const;
    
    double scale;
    QImage image;
    QPainterPath unitedPath;
    std::vector<QPainterPath> paths;
    const QPainterPath* roadsPath;
    const QPainterPath* railPath;
    const QPainterPath* forestAreas;
    const Projector& proj;
    const MinMax& minmax;
}; 


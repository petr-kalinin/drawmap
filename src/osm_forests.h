#pragma once

#include "common.h"
#include "osm_common.h"

#include <osmium/handler.hpp>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include <QImage>
#include <QPainter>

class OsmForestsHandler : public BaseHandler {
public:
    OsmForestsHandler(const Projector& proj_, const MinMax& minmax_, int imageSize, int xTile_, int yTile);
    
    virtual void area(const osmium::Area &area);
    
    virtual void finalize();
    
    QImage getImage() const;
    
    const QPainterPath& getAreas() const;
    
    void setHeights(cv::Mat mat);
    
private:
    template<class Object>
    bool needObject(const Object &object) const;
    
    QPainterPath areas;
    double scale;
    QImage image;
    const Projector& proj;
    const MinMax& minmax;
    cv::Mat heights;
    int xTile, yTile;
}; 


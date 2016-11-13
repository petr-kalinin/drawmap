#include "common.h"

#include <QPainter>

Projector::Projector() :
    latlonProj(pj_init_plus("+proj=latlong +datum=WGS84")),
    resultProj(pj_init_plus("+init=epsg:3857"))
{}
    
point Projector::transform(point p) const {
    p.x *= M_PI/180;
    p.y *= M_PI/180;
    pj_transform(latlonProj, resultProj, 1, 1, &p.x, &p.y, NULL);
    return p;
}
    
point Projector::invertTransform(point p) const {
    pj_transform(resultProj, latlonProj, 1, 1, &p.x, &p.y, NULL);
    p.x /= M_PI/180;
    p.y /= M_PI/180;
    return p;
}

QImage combine(const QImage& image1, const QImage& image2) {
    QImage result(image1);
    QPainter painter(&result);
    painter.drawImage(0, 0, image2);
    
    return result;
}

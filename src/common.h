#pragma once

#include <proj_api.h>
#include <cmath>
#include <QImage>

struct point {
    double x,y;
};

class Projector {
public:
    Projector();
    
    point transform(point p) const;
    
    point invertTransform(point p) const;
    
private:
    projPJ latlonProj;
    projPJ resultProj;
};

struct MinMax {
    double maxx, maxy, minx, miny;
};

QImage combine(const QImage& image1, const QImage& image2);

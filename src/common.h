#pragma once

#include <proj_api.h>
#include <cmath>

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


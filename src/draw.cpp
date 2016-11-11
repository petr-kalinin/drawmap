#include "srtm.h"

#include <QImage>

#include <iostream>

const int IMAGE_SIZE = 4000;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " OSMFILE\n";
        exit(1);
    }

    Projector proj;
    MinMax minmax;
    
    /*
    minmax.minx = 4550000; // 4860000
    minmax.maxx = 5200000; // 4880000
    minmax.miny = 7300000; // 7555000
    */
    
    /*
    point center = proj.transform({41.720581, 57.858635});
    minmax.minx = center.x - 100000;
    minmax.maxx = center.x + 100000;
    minmax.miny = center.y - 100000;
    */
    
    
    point center = proj.transform({43.739319, 56.162759});
    minmax.minx = center.x - 10000;
    minmax.maxx = center.x + 10000;
    minmax.miny = center.y - 10000;
    
    
    
    /*
    point center = proj.transform({37.6, 55.8});
    minmax.minx = center.x - 1000000;
    minmax.maxx = center.x + 1000000;
    minmax.miny = center.y - 1000000;
    */
    
    minmax.maxy = minmax.miny + (minmax.maxx - minmax.minx); 
    
    SRTMtoCV srtm(proj, minmax, IMAGE_SIZE);
    
    cvPaint::paint(srtm.getCvHeights()).save("test-cv.png");
    cvPaint::paint(srtm.getXGrad()).save("test-xgrad.png");
    cvPaint::paint(srtm.getYGrad()).save("test-ygrad.png");
    cvPaint::paintGrads(srtm.getXGrad(), srtm.getYGrad()).save("test-grads.png");
    
    return 0;
}


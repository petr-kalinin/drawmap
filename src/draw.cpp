#include "srtm.h"

#include "osm_roads.h"
#include "osm_rail.h"
#include "osm_places.h"
#include "osm_main.h"

#include <QImage>

#include <iostream>

const int IMAGE_SIZE = 1200;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " OSMFILE\n";
        exit(1);
    }

    Projector proj;
    MinMax minmax;
    /*
     * Nizhobl: x in [41.7 .. 47.8]  -> [4642000 .. 5321000]  dx=679000 em (effective meters)
     *          y in [54.4 .. 58.1]  -> [7231000 .. 7952000]  dy=721000 em
     * 
     * assume page size is A-1 (1189 x 1682)
     * x-scale is 517 em / mm
     * y-scale is 430 em / mm
     * so assuming scale is 500 em / mm
     * thus 10cm x 10cm square is 50000 x 50000 em
     * 5cm x 5cm is 25000 x 25000
     * 
     * for 600dpi for 10x10cm image size should be 2400 x 2400
     * 5x5cm -- 1200x1200
     */
    
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
    minmax.minx = center.x - 25000/2;
    minmax.maxx = center.x + 25000/2;
    minmax.miny = center.y - 25000/2;
    
    
    
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
    
    OsmRoadsHandler roads(proj, minmax, IMAGE_SIZE);
    OsmRailHandler rail(proj, minmax, IMAGE_SIZE);
    OsmPlacesHandler places(proj, minmax, IMAGE_SIZE);

    OsmDrawer osm;
    osm.addHandler(&roads);
    osm.addHandler(&rail);
    osm.addHandler(&places);
    
    osm.dispatch(argv[1]);
    
    auto hills = cvPaint::paintGrads(srtm.getXGrad(), srtm.getYGrad());
    
    roads.getImage().save("roads.png");
    
    combine(hills, combine(places.getImage(), combine(roads.getImage(), rail.getImage()))).save("final.png");
    
    return 0;
}


#include "srtm.h"

#include "osm_roads.h"
#include "osm_rail.h"
#include "osm_places.h"
#include "osm_rivers.h"
#include "osm_forests.h"
#include "osm_main.h"

#include <QImage>

#include <iostream>
#include <thread>

const int IMAGE_SIZE = 1200;
const int TILE_SOURCE_SIZE = 25000;

void drawTile(QImage* result, const std::string osmFile, const Projector proj, MinMax minmax, int xTile, int yTile) {
    std::cout << "tile " << minmax.minx << " " << minmax.maxx << "   " << minmax.miny << " " << minmax.maxy << std::endl;
    
    SRTMtoCV srtm(proj, minmax, IMAGE_SIZE);
    
    cvPaint::paint(srtm.getCvHeights()).save("test-cv.png");
    cvPaint::paint(srtm.getXGrad()).save("test-xgrad.png");
    cvPaint::paint(srtm.getYGrad()).save("test-ygrad.png");
    cvPaint::paintGrads(srtm.getXGrad(), srtm.getYGrad()).save("test-grads.png");
    
    OsmRoadsHandler roads(proj, minmax, IMAGE_SIZE);
    OsmRailHandler rail(proj, minmax, IMAGE_SIZE);
    OsmPlacesHandler places(proj, minmax, IMAGE_SIZE);
    OsmRiversHandler rivers(proj, minmax, IMAGE_SIZE);
    
    OsmForestsHandler forests(proj, minmax, IMAGE_SIZE, xTile, yTile);
    
    forests.setHeights(srtm.getCvHeights());
    
    roads.setPlacesPath(places.getUnitedPath());
    
    places.setRoadsPath(roads.getUnitedPath());
    places.setRailPath(rail.getUnitedPath());
    places.setForestAreas(forests.getAreas());

    OsmDrawer osm;
    osm.addHandler(&roads);
    osm.addHandler(&rail);
    osm.addHandler(&places);
    osm.addHandler(&rivers);
    osm.addHandler(&forests);
    
    osm.dispatch(osmFile);
    
    auto hills = cvPaint::paintGrads(srtm.getXGrad(), srtm.getYGrad());
    
    
    //roads.getImage().save("roads.png");
    //places.getImage().save("places.png");
    
    *result = hills;
    // *result = rivers.getImage();
    *result = combine(*result, forests.getImage());
    *result = combine(*result, rivers.getImage());
    *result = combine(*result, roads.getImage());
    *result = combine(*result, rail.getImage());
    *result = combine(*result, places.getImage());
    //std::cout << result << " result.width=" << result->width() << std::endl;
    
    //*result = forests.getImage();
}

int main(int argc, char* argv[]) {
    cv::setNumThreads(0);
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
    minmax.minx = center.x - TILE_SOURCE_SIZE/2;
    minmax.maxx = center.x + TILE_SOURCE_SIZE/2;
    minmax.miny = center.y - TILE_SOURCE_SIZE/2;
    
    
    
    /*
    point center = proj.transform({37.6, 55.8});
    minmax.minx = center.x - 1000000;
    minmax.maxx = center.x + 1000000;
    minmax.miny = center.y - 1000000;
    */
    
    minmax.maxy = minmax.miny + (minmax.maxx - minmax.minx); 
    
    int TILES = 10;
    int OFFSET = TILES/2;
    
    QImage result(IMAGE_SIZE*TILES, IMAGE_SIZE*TILES, QImage::Format_ARGB32);
    result.fill({255, 255, 255, 0});
    QPainter painter(&result);

    for (int x=0; x<TILES; x++) {
        std::vector<std::unique_ptr<std::thread>> threads;
        std::vector<QImage> images(TILES);
        for (int y=0; y<TILES; y++) {
            MinMax curMinMax(minmax);
            curMinMax.minx += (x-OFFSET)*TILE_SOURCE_SIZE;
            curMinMax.maxx += (x-OFFSET)*TILE_SOURCE_SIZE;
            curMinMax.miny += (OFFSET-y)*TILE_SOURCE_SIZE;
            curMinMax.maxy += (OFFSET-y)*TILE_SOURCE_SIZE;
            threads.emplace_back(new std::thread(drawTile, &(images[y]), argv[1], proj, curMinMax, x, y));
            //threads[y]->join();
            //drawTile(&(images[y]), argv[1], proj, curMinMax);
        }
        for (int y=0; y<TILES; y++) {
            std::cout << "y=" << y << std::endl;
            threads[y]->join();
            std::cout << "y=" << y << std::endl;
            std::ostringstream str;
            str << "img" << x << y;
            images[y].save(std::string(str.str() + ".png").c_str());
            std::cout << &(images[y]) << "y=" << y << " " << str.str() << " " << images[y].width() << std::endl;
            painter.drawImage(x*IMAGE_SIZE, y*IMAGE_SIZE, images[y]);
        }
    }
    
    result.save("final.png");

    
    return 0;
}

#include <QImage>
#include <QPainter>

#include <proj_api.h>

#include <iostream>
#include <cmath>
#include <cstdlib>
#include <fstream>

#include <getopt.h>

#include <osmium/area/assembler.hpp>
#include <osmium/area/multipolygon_collector.hpp>
#include <osmium/dynamic_handler.hpp>
#include <osmium/geom/wkt.hpp>
#include <osmium/handler/dump.hpp>
#include <osmium/handler/node_locations_for_ways.hpp>
#include <osmium/index/map/dummy.hpp>
#include <osmium/index/map/sparse_mem_array.hpp>
#include <osmium/io/pbf_input.hpp>
#include <osmium/visitor.hpp>

typedef osmium::index::map::Dummy<osmium::unsigned_object_id_type, osmium::Location> index_neg_type;
typedef osmium::index::map::SparseMemArray<osmium::unsigned_object_id_type, osmium::Location> index_pos_type;
typedef osmium::handler::NodeLocationsForWays<index_pos_type, index_neg_type> location_handler_type;

struct point {
    double x,y;
};

class Projector {
public:
    Projector() :
        latlonProj(pj_init_plus("+proj=latlong +datum=WGS84")),
        resultProj(pj_init_plus("+init=epsg:3857"))
    {}
    
    point transform(point p) const {
        p.x *= M_PI/180;
        p.y *= M_PI/180;
        pj_transform(latlonProj, resultProj, 1, 1, &p.x, &p.y, NULL);
        return p;
    }
    
    point invertTransform(point p) const {
        pj_transform(resultProj, latlonProj, 1, 1, &p.x, &p.y, NULL);
        p.x /= M_PI/180;
        p.y /= M_PI/180;
        return p;
    }
    
private:
    projPJ latlonProj;
    projPJ resultProj;
};

class MaxMinHandler : public osmium::handler::Handler {
public:
    MaxMinHandler(const Projector& proj_) :
        maxx(-1e10),
        maxy(-1e10),
        minx(1e10),
        miny(1e10),
        proj(proj_)
    {}
    
    void node(const osmium::Node& node) {
        point p = proj.transform({node.location().lon(), node.location().lat()});
        maxx = std::max(maxx, p.x);
        maxy = std::max(maxy, p.y);
        minx = std::min(minx, p.x);
        miny = std::min(miny, p.y);
    }
    
    double maxx, maxy, minx, miny;
    
private:
    const Projector& proj;
};

class Drawer : public osmium::handler::Handler {
public:
    Drawer(const Projector& proj_, const MaxMinHandler& minmax_) : 
        scale(-1),
        image(4000, 4000, QImage::Format_ARGB32),
        painter(&image),
        proj(proj_),
        minmax(minmax_)
    {
        image.fill({255, 255, 255, 0});
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    }
    
    void way(const osmium::Way& way)  {
        if (!way.get_value_by_key("highway"))
            return;
        if (scale < 0) {
            double scaleX = image.width() / (minmax.maxx - minmax.minx);
            double scaleY = image.height() / (minmax.maxy - minmax.miny);
            scale = std::min(scaleX, scaleY);
            std::cout << "scale=" << scale << std::endl;
        }
        QPainterPath path;
        bool first = true;
        for (const auto& node: way.nodes()) {
            if (!node.location())
                continue;
            point p = proj.transform({node.lon(), node.lat()});
            double x = scale * (p.x-minmax.minx);
            double y = scale * (minmax.maxy-p.y);
            if (first) {
                path.moveTo(x, y);
                first = false;
            } else {
                path.lineTo(x, y);
            }
        }
        QPen pen({255, 0, 0});
        pen.setWidth(2);
        painter.setPen(pen);
        
        painter.drawPath(path);
    }
    
    QImage& getImage() {
        return image;
    }
    
private:
    double scale;
    QImage image;
    QPainter painter;
    const Projector& proj;
    const MaxMinHandler& minmax;
}; 

class SRTM : public osmium::handler::Handler {
public:
    SRTM(const Projector& proj_, const MaxMinHandler& minmax_) : 
        image(4000, 4000, QImage::Format_ARGB32),
        painter(&image),
        proj(proj_),
        minmax(minmax_)
    {
        image.fill({255, 255, 255, 255});
        paint();
    }
    
    QImage& getImage() {
        return image;
    }

private:
    static const int SRTMSize = 3601;
    typedef std::vector<std::vector<int16_t>> Heights;
    
    QImage image;
    QPainter painter;
    const Projector& proj;
    const MaxMinHandler minmax;
    std::map<std::pair<int, int>, Heights> heights;
    
    Heights loadHeights(std::string filename) {
        std::ifstream file(filename, std::ios::in|std::ios::binary);
        if (!file) {
            throw std::runtime_error("Can't open file " + filename);
        }

        unsigned char buffer[2];
        Heights heights;
        heights.resize(SRTMSize);
        for (int i = 0; i < SRTMSize; ++i) {
            heights[i].resize(SRTMSize);
        }
        for (int i = 0; i < SRTMSize; ++i) {
            for (int j = 0; j < SRTMSize; ++j) {
                if (!file.read(reinterpret_cast<char*>(buffer), sizeof(buffer))) {
                    throw std::runtime_error("Can't read data from file " + filename);
                }
                heights[j][i] = (buffer[0] << 8) | buffer[1];
            }
        }
        /*
        QImage image(SRTMSize, SRTMSize, QImage::Format_ARGB32);
        for (int x=0; x<image.width(); x++)
            for (int y=0; y<image.height(); y++)
                image.setPixel(x, y, heightToColor(heights[x][y]));
        image.save((filename + ".png").c_str());
        */
        return heights;
    }
    
    Heights loadHeights(int x, int y) {
        char cx='E', cy='N';
        if (x < 0) {
            x = -x;
            cx = 'W';
        }
        if (y < 0) {
            y = -y;
            cy = 'S';
        }
        char filename[8];
        snprintf(filename, 8, "%c%02d%c%03d", cy, y, cx, x);
        
        system((std::string("./download_srtm.sh ") + filename).c_str());
        
        return loadHeights(std::string("srtm/") + filename + ".hgt");
    }
    
    const Heights& getHeights(int x, int y) {
        if (heights.count({x,y}) == 0)
            heights[{x,y}] = loadHeights(x,y);
        return heights[{x,y}];
    }
    
    int16_t getHeight(double x, double y) {
        int xx = std::floor(x);
        int yy = std::floor(y);
        const Heights& thisHeights = getHeights(xx, yy);
        int fx = (x - xx) * SRTMSize;
        int fy = (1 - (y - yy)) * SRTMSize;
        //std::cout << "getHeight(" << x << " " << y << "   " << xx << " " << yy << "    " << fx << " " << fy << std::endl;
        return thisHeights[fx][fy];
    }
    
    QRgb heightToColor(int16_t height) {
        if (height < 50) height = 50;
        if (height > 200) height = 200;
        height = 1.0* (height - 50) / (200 - 50) * 255;
        QColor color({height, height, height});
        return color.rgba();
    }
    
    void paint() {
        for (int x=0; x<image.width(); x++) {
            for (int y=0; y<image.height(); y++) {
                double rx = (minmax.minx + 1.0*x/image.width()*(minmax.maxx-minmax.minx));
                double ry = (minmax.maxy - 1.0*y/image.height()*(minmax.maxy-minmax.miny));
                point p = proj.invertTransform({rx, ry});
                //std::cout << x << " " << y << "    " << rx << " " << ry << std::endl;
                int height = getHeight(p.x, p.y);
                image.setPixel(x, y, heightToColor(height));
             }
        }
    }
};

QImage combine(const QImage& image1, const QImage& image2) {
    QImage result(image1);
    QPainter painter(&result);
    painter.drawImage(0, 0, image2);
    
    return result;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " OSMFILE\n";
        exit(1);
    }

    Projector proj;
    MaxMinHandler minmax(proj);
    
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
    
    
    minmax.maxy = minmax.miny + (minmax.maxx - minmax.minx); 
    
    SRTM srtm(proj, minmax);
    Drawer drawer(proj, minmax);
    
    for (int i=1; i<argc; i++) {
        std::cout << argv[i] << std::endl;
        //osmium::handler::Dump handler(std::cout);
        osmium::io::File infile(argv[i]);

        osmium::area::Assembler::config_type assembler_config;
        osmium::area::MultipolygonCollector<osmium::area::Assembler> collector(assembler_config);

        std::cerr << "Pass 1...\n";
        osmium::io::Reader reader1(infile);
        collector.read_relations(reader1);
        reader1.close();
        std::cerr << "Pass 1 done\n";


        index_pos_type index_pos;
        index_neg_type index_neg;
        location_handler_type location_handler(index_pos, index_neg);
        location_handler.ignore_errors(); // XXX

        std::cerr << "Pass 2...\n";
        osmium::io::Reader reader2(infile);
        osmium::apply(reader2, location_handler,  drawer, collector.handler([&drawer](osmium::memory::Buffer&& buffer) {
            osmium::apply(buffer, drawer);
        }));
        reader2.close();
        std::cerr << "Pass 2 done\n";
    }
    
    combine(srtm.getImage(), drawer.getImage()).save("test.png");
    
    return 0;
}


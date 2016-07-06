#include <QImage>
#include <QPainter>

#include <proj_api.h>

#include <iostream>
#include <cmath>

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
        image(2000, 2000, QImage::Format_ARGB32),
        painter(&image),
        proj(proj_),
        minmax(minmax_)
    {
        image.fill({255, 255, 255});
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
        painter.setPen({0, 0, 0});
        painter.drawPath(path);
    }
    
    void finalize() {
        image.save("test.png");
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
    Drawer(const Projector& proj_, const MaxMinHandler& minmax_) : 
        image(2000, 2000, QImage::Format_ARGB32),
        painter(&image),
        proj(proj_),
        minmax(minmax_)
    {
        image.fill({255, 255, 255});
        downloadData();
        paint();
    }
    
private:
    static const int SRTM_size = 3001;
    typedef int heights[SRTM_size][SRTM_size];
    std::map<std::pair<int, int>, heights> data;
    
    void downloadData() {
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " OSMFILE\n";
        exit(1);
    }

    Projector proj;
    MaxMinHandler minmax(proj);
    minmax.minx = 4860000; // 4860000
    minmax.maxx = 4880000; // 4880000
    minmax.miny = 7580000; // 7555000
    minmax.maxy = minmax.miny + (minmax.maxx - minmax.minx); 
    //osmium::handler::Dump handler(std::cout);
    osmium::io::File infile(argv[1]);

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
    Drawer drawer(proj, minmax);

    std::cerr << "Pass 2...\n";
    osmium::io::Reader reader2(infile);
    osmium::apply(reader2, location_handler,  drawer, collector.handler([&drawer](osmium::memory::Buffer&& buffer) {
        osmium::apply(buffer, drawer);
    }));
    reader2.close();
    std::cerr << "Pass 2 done\n";
    
    drawer.finalize();
    
    return 0;
}


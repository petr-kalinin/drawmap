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

const int IMAGE_SIZE = 4000;
std::vector<int> dx{-1,-1,-1, 0, 1, 1, 1, 0};
std::vector<int> dy{-1, 0, 1, 1, 1, 0, -1,-1};

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
        image(IMAGE_SIZE, IMAGE_SIZE, QImage::Format_ARGB32),
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
        image(IMAGE_SIZE, IMAGE_SIZE, QImage::Format_ARGB32),
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

    typedef std::vector<std::vector<int32_t>> Heights;

    Heights getHeightsForImage() {
        Heights result;
        result.resize(image.width());
        for (int x=0; x<image.width(); x++) {
            result[x].resize(image.height());
            for (int y=0; y<image.height(); y++) {
                double rx = (minmax.minx + 1.0*x/image.width()*(minmax.maxx-minmax.minx));
                double ry = (minmax.maxy - 1.0*y/image.height()*(minmax.maxy-minmax.miny));
                point p = proj.invertTransform({rx, ry});
                result[x][y] = getHeight(p.x, p.y);
             }
        }
        return result;
    }
    
private:
    static const int SRTMSize = 3601;
    
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
        Heights heights = getHeightsForImage();
        for (int x=0; x<image.width(); x++) {
            for (int y=0; y<image.height(); y++) {
                image.setPixel(x, y, heightToColor(heights[x][y]));
             }
        }
    }

};

class RiverBasins {
public:
    RiverBasins(const SRTM::Heights heights_) : 
        image(IMAGE_SIZE, IMAGE_SIZE, QImage::Format_ARGB32),
        painter(&image),
        heights(heights_)
    {
        image.fill({255, 255, 255, 255});
        paint();
    }
    
    QImage& getImage() {
        return image;
    }
    
private:
    struct point {int x; int y;};
     
    QImage image;
    QPainter painter;
    SRTM::Heights heights;
    std::vector<std::vector<point>> representative, parent;
    std::vector<std::vector<std::vector<point>>> childs;
    std::vector<std::vector<int>> area;
    std::vector<point> roots;
    std::vector<std::vector<bool>> was_on_hills;
    
    void reset_hills() {
        was_on_hills.resize(image.width());
        for (int x=0; x<image.width(); x++) {
            was_on_hills[x] = std::vector<bool>(image.height(), false);
        }
    }
    
    point go_over_hills(int x, int y, int base_height, int x0, int y0) {
        if (was_on_hills[x][y]) return {-1, -1};
        was_on_hills[x][y] = true;
        if (heights[x][y] > base_height + 10) {
            return {-1, -1};
        }
        if (abs(x-x0)+abs(y-y0) > 10)
            return {-1, -1};
        if (heights[x][y] < base_height) {
            return {x, y};
        }
        point result = {x, y};
        int best_height = heights[x][y];
        for (int k=0; k<dx.size(); k++) {
            int xx = x+dx[k];
            int yy = y+dy[k];
            if ((xx < 0) || (xx >= image.width()))
                continue;
            if ((yy < 0) || (yy >= image.height()))
                continue;
            point cur_point = go_over_hills(xx, yy, base_height, x0, y0);
            if (cur_point.x == -1)
                continue;
            int cur_height = heights[cur_point.x][cur_point.y];
            if (cur_height < best_height) {
                result = cur_point;
                best_height = cur_height;
            }
        }
        return result;
    }
    
    point find_exit(int x, int y, point repr) {
        if (representative[x][y].x >= 0) {
            return {-1, -1};
        }
        representative[x][y] = repr;
        point result = {-1,-1};
        int best_height = heights[x][y];
        for (int k=0; k<dx.size(); k++) {
            int xx = x+dx[k];
            int yy = y+dy[k];
            if ((xx < 0) || (xx >= image.width()))
                continue;
            if ((yy < 0) || (yy >= image.height()))
                continue;
            point cur_point{xx,yy};
            if (heights[xx][yy] == heights[x][y]) {
                cur_point = find_exit(xx, yy, repr);
            } else if (heights[xx][yy] > heights[x][y]) {
                continue;
            }
            if (cur_point.x == -1)
                continue;
            int cur_height = heights[cur_point.x][cur_point.y];
            if (cur_height < best_height) {
                result = cur_point;
                best_height = cur_height;
            }
        }
        if (result.x >= 0)
            return result;
        reset_hills();
        for (int k=0; k<dx.size(); k++) {
            int xx = x+dx[k];
            int yy = y+dy[k];
            if ((xx < 0) || (xx >= image.width()))
                continue;
            if ((yy < 0) || (yy >= image.height()))
                continue;
            point cur_point{xx,yy};
            if (heights[xx][yy] > heights[x][y]) {
                cur_point = go_over_hills(xx, yy, heights[x][y], x, y);
            } else continue;
            if (cur_point.x == -1)
                continue;
            int cur_height = heights[cur_point.x][cur_point.y];
            if (cur_height < best_height) {
                result = cur_point;
                best_height = cur_height;
            }
        }
        return result;
    }
    
    void mark(int x, int y) {
        if (representative[x][y].x >= 0)
            return;
        point p = find_exit(x,y,{x,y});
        parent[x][y] = p;
        if (p.x >= 0) {
            mark(p.x, p.y);
            p = representative[p.x][p.y];
            parent[x][y] = p;
            childs[p.x][p.y].push_back({x,y});
        } else
            roots.push_back({x,y});
    }
    
    QRgb color(double x, int height) {
        QColor color({height*x, height*(1-x), 0});
        return color.rgba();
    }
    
    void set_area(int x, int y) {
        area[x][y] = 1;
        for (int i=0; i<childs[x][y].size(); i++) {
            set_area(childs[x][y][i].x, childs[x][y][i].y);
            area[x][y] += area[childs[x][y][i].x][childs[x][y][i].y];
            if (x==0 && y==0) {
                //std::cout << "area +=" << area[childs[x][y][i].first][childs[x][y][i].second] << " = " << area[x][y] << std::endl;
            }
        }
    }
    
    void paint_from_v(int x, int y, double l, double r) {
        //image.setPixel(x, y, color((l+r)/2, heights[x][y]));
        //image.setPixel(x, y, color(1.0*area[x][y] / IMAGE_SIZE / IMAGE_SIZE));
        static const int MIN_AREA = IMAGE_SIZE * IMAGE_SIZE / 200;
        if (area[x][y] > MIN_AREA)
            image.setPixel(x, y, color(1.0*MIN_AREA/area[x][y], 255));
        if (childs[x][y].size() == 0)
            return;
        double step = (r-l) / area[x][y];
        double pos = l;
        for (int i=0; i<childs[x][y].size(); i++) {
            double new_pos = pos + area[childs[x][y][i].x][childs[x][y][i].y] * step;
            paint_from_v(childs[x][y][i].x, childs[x][y][i].y, pos, new_pos);
            pos = new_pos;
        }
    }
    
    void paint_equal(int x,int y) {
        image.setPixel(x, y, image.pixel(representative[x][y].x, representative[x][y].y));
    }
    
    void paint() {
        representative.resize(image.width());
        parent.resize(image.width());
        childs.resize(image.width());
        for (int x=0; x<image.width(); x++) {
            representative[x].resize(image.height(), {-1, -1});
            parent[x].resize(image.height());
            childs[x].resize(image.height());
        }
        for (int x=0; x<image.width(); x++)
            for (int y=0; y<image.height(); y++) {
                mark(x,y);
            }
        double step = 1.0/roots.size();
        for (int i=0; i<roots.size(); i++) {
            image.setPixel(roots[i].x, roots[i].y, color(step*i, 255));
            //paint_from_v(roots[i].x, roots[i].y, step*i, step*(i+1));
        }
        for (int x=0; x<image.width(); x++)
            for (int y=0; y<image.height(); y++) {
                //paint_equal(x,y);
            }
        /*
        for (int y=0; y<image.height(); y++) {
            for (int x=0; x<image.width(); x++) {
                std::cout << representative[x][y].x << representative[x][y].y << " ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
        for (int y=0; y<image.height(); y++) {
            for (int x=0; x<image.width(); x++) {
                std::cout << parent[x][y].x << parent[x][y].y << " ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
        for (int y=0; y<image.height(); y++) {
            for (int x=0; x<image.width(); x++) {
                for (auto p: childs[x][y]) {
                    std::cout << p.x << p.y;
                }
                std::cout << ". ";
            }
            std::cout << std::endl;
        }
        */
    }
};

class RiverBasins2 {
public:
    RiverBasins2(const SRTM::Heights heights_) : 
        image(IMAGE_SIZE, IMAGE_SIZE, QImage::Format_ARGB32),
        painter(&image),
        heights(heights_)
    {
        image.fill({255, 255, 255, 255});
        
        /*
        for (int i=0; i<image.width(); i++) {
            heights[i][0] = -INF/2;
            heights[i][image.height()-1] = -INF/2;
        }
        for (int i=0; i<image.height(); i++) {
            heights[0][i] = -INF/2;
            heights[image.width()-1][i] = -INF/2;
        }*/
        
        heights[image.width() - 1][image.height()/2] = -INF/2;
        paint();
    }
    
    QImage& getImage() {
        return image;
    }
    
private:
    typedef std::pair<int, int> point;
     
    QImage image;
    QPainter painter;
    SRTM::Heights heights;
    std::vector<std::vector<point>> parent;
    static std::vector<std::vector<int>> area;
    std::vector<std::vector<std::vector<point>>> childs;
    static const int INF = 100000;
    point root;
    static const int MIN_AREA_FOR_RIVER = IMAGE_SIZE * IMAGE_SIZE / 500;
    
    void build_tree() {
        std::set<std::pair<int, point>> q;
        std::vector<std::vector<int>> d;
        d.resize(image.width(), std::vector<int>(image.height(), INF));
        /*
        for (int y=0; y<image.height(); y++) {
            for (int x=0; x<image.width(); x++)
                std::cout << heights[x][y] << " ";
            std::cout << std::endl;
        }
        std::cout << std::endl;
        */
        root = {-1,-1};
        for (int x=0; x<image.width(); x++)
            for (int y=0; y<image.height(); y++)
                if (heights[x][y] == -INF/2) {
                    //std::cout << "found root " << x << y << std::endl;
                    q.insert({-INF/2, {x, y}});
                    d[x][y] = -INF/2;
                    if (root.first >= 0) {
                        parent[x][y] = root;
                    } else {
                        root = {x,y};
                    }
                }
        while (!q.empty()) {
            auto x = *q.begin();
            q.erase(q.begin());
            point p = x.second;
            if (d[p.first][p.second] == -INF) continue;
            d[p.first][p.second] = -INF;
            point par = parent[p.first][p.second];
            //std::cout << "Popped " << p.first << p.second << " " << x.first << " " << par.first << par.second << std::endl;
            if (par.first >= 0)
                childs[par.first][par.second].push_back(p);
            for (int k=0; k<dx.size(); k++) {
                int xx = p.first+dx[k];
                int yy = p.second+dy[k];
                if ((xx < 0) || (xx >= image.width()))
                    continue;
                if ((yy < 0) || (yy >= image.height()))
                    continue;
                //std::cout << "Try put " << xx << " " << yy << std::endl;
                if (d[xx][yy] == -INF) continue;
                //int w = heights[p.first][p.second] - heights[xx][yy];
                int w = heights[xx][yy];
                //std::cout << "was " << d[xx][yy] << " can be " << w << std::endl;
                if (d[xx][yy] > w) {
                    d[xx][yy] = w;
                    parent[xx][yy] = p;
                    q.insert({w, {xx, yy}});
                }
            }
        }
    }
    
    QRgb color(double x, int height) {
        QColor color({height*x, height*(1-x), 0});
        return color.rgba();
    }

    QRgb riverColor(int area) {
        double coeff = 0.3 + 0.7*std::log(1.0*area/MIN_AREA_FOR_RIVER) / std::log(1.0*IMAGE_SIZE*IMAGE_SIZE/MIN_AREA_FOR_RIVER);
        QColor color({0, 0, 255*coeff});
        return color.rgba();
    }

    void set_area(int x, int y) {
        area[x][y] = 1;
        for (int i=0; i<childs[x][y].size(); i++) {
            set_area(childs[x][y][i].first, childs[x][y][i].second);
            area[x][y] += area[childs[x][y][i].first][childs[x][y][i].second];
            if (x==0 && y==0) {
                //std::cout << "area +=" << area[childs[x][y][i].first][childs[x][y][i].second] << " = " << area[x][y] << std::endl;
            }
        }
    }
    
    static bool compare_area(point a, point b) {
        return area[a.first][a.second] > area[b.first][b.second];
    }
    
    void paint_from_v(int x, int y, double l, double r) {
        image.setPixel(x, y, color((l+r)/2, heights[x][y]));
        //image.setPixel(x, y, color(1.0*area[x][y] / IMAGE_SIZE / IMAGE_SIZE));
        if (childs[x][y].size() == 0)
            return;
        sort(childs[x][y].begin(), childs[x][y].end(), compare_area);
        double step = (r-l) / area[x][y];
        double pos = l;
        for (int i=0; i<childs[x][y].size(); i++) {
            double new_pos = pos + area[childs[x][y][i].first][childs[x][y][i].second] * step;
            paint_from_v(childs[x][y][i].first, childs[x][y][i].second, pos, new_pos);
            pos = new_pos;
        }
    }
    
    void paint_rivers() {
        for (int x=0; x<image.width(); x++)
            for (int y=0; y<image.height(); y++) {
                if (area[x][y] > MIN_AREA_FOR_RIVER) {
                    auto color = riverColor(area[x][y]);
                    image.setPixel(x, y, color);
                    image.setPixel(x+1, y, color);
                    image.setPixel(x, y+1, color);
                    image.setPixel(x+1, y+1, color);
                }
            }
    }

    void paint() {
        parent.resize(image.width());
        childs.resize(image.width());
        area.resize(image.width());
        for (int x=0; x<image.width(); x++) {
            parent[x].resize(image.height(), {-1,-1});
            childs[x].resize(image.height(), {});
            area[x].resize(image.height(), 0);
        }
        build_tree();
        set_area(root.first, root.second);
        paint_from_v(root.first, root.second, 0, 1);
        paint_rivers();
        /*
        for (int y=0; y<image.height(); y++) {
            for (int x=0; x<image.width(); x++) {
                std::cout << parent[x][y].first << parent[x][y].second << " ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
        for (int y=0; y<image.height(); y++) {
            for (int x=0; x<image.width(); x++) {
                std::cout << area[x][y] << " ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
        for (int y=0; y<image.height(); y++) {
            for (int x=0; x<image.width(); x++) {
                for (auto p: childs[x][y]) {
                    std::cout << p.first << p.second;
                }
                std::cout << ". ";
            }
            std::cout << std::endl;
        }
        */
    }
};

std::vector<std::vector<int>> RiverBasins2::area {};

QImage combine(const QImage& image1, const QImage& image2) {
    QImage result(image1);
    QPainter painter(&result);
    painter.drawImage(0, 0, image2);
    
    return result;
}

int main(int argc, char* argv[]) {
    /*for (int i=-3; i<=3; i++)
        for (int j=-3; j<=3; j++) {
            dx.push_back(i);
            dy.push_back(j);
        }*/
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " OSMFILE\n";
        exit(1);
    }

    Projector proj;
    MaxMinHandler minmax(proj);
    
    
    minmax.minx = 4550000; // 4860000
    minmax.maxx = 5200000; // 4880000
    minmax.miny = 7300000; // 7555000
    
    
    /*
    point center = proj.transform({41.720581, 57.858635});
    minmax.minx = center.x - 100000;
    minmax.maxx = center.x + 100000;
    minmax.miny = center.y - 100000;
    */
    
    /*
    point center = proj.transform({43.739319, 56.162759});
    minmax.minx = center.x - 10000;
    minmax.maxx = center.x + 10000;
    minmax.miny = center.y - 10000;
    */
    
    
    minmax.maxy = minmax.miny + (minmax.maxx - minmax.minx); 
    
    SRTM srtm(proj, minmax);
    RiverBasins2 basins(srtm.getHeightsForImage());
    
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
    
    //combine(basins.getImage(), drawer.getImage()).save("test.png");
    basins.getImage().save("test.png");
    srtm.getImage().save("test-1.png");
    
    return 0;
}


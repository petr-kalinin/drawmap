#include "osm_rivers.h"
#include "common.h"

#include <osmium/osm/area.hpp>
#include <osmium/osm/way.hpp>
#include <QPainterPathStroker>
#include <Qt>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include <set>
#include <map>
#include <cmath>
#include <random>
#include <queue>

namespace {
    std::map<std::string, std::set<std::string>> TAGS_TO_INCLUDE{
        {"waterway", {"river", "riverbank", "canal"}},
        {"natural", {"water"}},
        {"landuse", {"reservoir"}},
    };
    
    const QColor BASE_COLOR(0, 102, 255);
}

OsmRiversHandler::OsmRiversHandler(const Projector& proj_, const MinMax& minmax_, int imageSize) : 
    image(imageSize, imageSize, QImage::Format_ARGB32),
    proj(proj_),
    minmax(minmax_)
{
    image.fill({255, 255, 255, 0});
    double scaleX = image.width() / (minmax.maxx - minmax.minx);
    double scaleY = image.height() / (minmax.maxy - minmax.miny);
    scale = std::min(scaleX, scaleY);
}

template<class Object>
bool OsmRiversHandler::needObject(const Object& object) const {
    for (const auto& tag: TAGS_TO_INCLUDE) {
        if (object.get_value_by_key(tag.first.c_str())) {
            std::string type = object.get_value_by_key(tag.first.c_str());
            if (tag.second.count(type) != 0)
                return true;
        }
    }
    return false;
}

void OsmRiversHandler::area(const osmium::Area& area)  {
    if (!needObject(area)) return;
    for (const auto& ring: area.outer_rings()) {
        QPainterPath path;
        bool first = true;
        for (const auto& node: ring) {
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
        if (path.intersects(QRectF(0, 0, image.width(), image.height()))) {
            areas += path;
        }
    }
}

void OsmRiversHandler::way(const osmium::Way& way)  {
    if (!needObject(way)) return;
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
    if (path.intersects(QRectF(0, 0, image.width(), image.height()))) {
        paths.addPath(path);
    }
}

QImage OsmRiversHandler::paintAreas() const {
    static const QColor WHITE{255, 255, 255, 0};
    static const int PAINT_IMAGE_SIZE = 400;
    QImage source(PAINT_IMAGE_SIZE, PAINT_IMAGE_SIZE, QImage::Format_ARGB32);
    source.fill(WHITE);
    QPainter painter(&source);
    double scale = 1.0*PAINT_IMAGE_SIZE / image.width();
    painter.setTransform(QTransform::fromScale(scale, scale));
    painter.fillPath(areas, BASE_COLOR);
    painter.setPen(QPen(BASE_COLOR, 3/scale/scale));
    painter.drawPath(paths);
    painter.drawPath(areas);

    QImage result = source;

    std::queue<std::pair<int, int>> q;
    std::vector<std::vector<double>> dist(result.width(), std::vector<double>(result.height(), -1));
    for (int x = 0; x < result.width(); x++) {
        for (int y = 0; y < result.height(); y++) {
            //std::cout << (result.pixel(x, y) & 0xffffff) << " " << (WHITE.rgba() & 0xffffff) << std::endl;
            if ((result.pixel(x, y)& 0xffffff) == (WHITE.rgba()& 0xffffff)) {
                q.emplace(x, y);
                dist[x][y] = 0;
            }
        }
    }
    static const std::vector<int> dx{-1,1,0,0};
    static const std::vector<int> dy{0,0,-1,1};
    std::random_device random;
    std::default_random_engine engine(random());
    std::uniform_real_distribution<double> uniform_dist(-0.2, 0.2);
    while (!q.empty()) {
        auto cur = q.front();
        q.pop();
        for (int k = 0; k < 4; k++) {
            int nx = cur.first + dx[k];
            int ny = cur.second + dy[k];
            if (nx < 0 || nx>=result.width() || ny<0 || ny>=result.height())
                    continue;
            if (dist[nx][ny] == -1) {
                dist[nx][ny] = dist[cur.first][cur.second] + 1 + uniform_dist(engine);
                q.emplace(nx, ny);
            }
        }
    }
    //std::uniform_real_distribution<double> h_distr(-1, 1);
    std::normal_distribution<> h_distr(0, 0.7);
    std::vector<std::vector<double>> h(result.width(), std::vector<double>(result.height(), 0));
    for (int x = 0; x < result.width(); x++) {
        for (int y = 0; y < result.height(); y++) {
            if ((result.pixel(x, y)& 0xffffff) != (WHITE.rgba()& 0xffffff)) {
                h[x][y] = h_distr(engine);
            }
        }
    }
    std::vector<std::vector<double>> hSmoothed(result.width(), std::vector<double>(result.height(), 0));
    for (int x = 0; x < result.width(); x++) {
        for (int y = 0; y < result.height(); y++) {
            if ((result.pixel(x, y)& 0xffffff) != (WHITE.rgba()& 0xffffff)) {
                hSmoothed[x][y] = 0;
                double sw = 0;
                static const double D1 = 40;
                static const double Ddist = 10;
                for (int dx = -10; dx <= 10; dx++) {
                    for (int dy = -10; dy <= 10; dy++) {
                        if (x+dx < 0 || x+dx>=result.width() || y+dy<0 || y+dy>=result.height())
                            continue;
                        double diffDist = dist[x][y]-dist[x+dx][y+dy];
                        double weight = exp(-(dx*dx+dy*dy)/D1)*exp(-(diffDist*diffDist) / Ddist);
                        hSmoothed[x][y] += h[x+dx][y+dy] * weight;
                        //hSmoothed[x][y] = (dist[x][y]/20) - int(dist[x][y]/20);
                        sw += weight;
                        //std::cout << "add " << h[x+dx][y+dy] * weight << " w " << weight << std::endl;
                    }
                }
                //std::cout << "result " << hSmoothed[x][y] << " " << sw << std::endl;
                hSmoothed[x][y] /= sw;
            }
        }
    }
    for (int x = 0; x < result.width(); x++) {
        for (int y = 0; y < result.height(); y++) {
            if ((result.pixel(x, y)& 0xffffff) != (WHITE.rgba()& 0xffffff)) {
                //std::cout << hSmoothed[x][y] << " " << h[x][y] << std::endl;
                QColor pointColor;
                double h, s, v;
                BASE_COLOR.getHsvF(&h, &s, &v);
                double factor = (dist[x][y]) / (5);
                if (factor > 1) factor = 1;
                double corr = (hSmoothed[x][y] * 3 * factor + 1) / 2;
                if (corr > 1) corr = 1;
                if (corr < 0) corr = 0;
                pointColor.setHsvF(h, s * corr, v);
                result.setPixel(x, y, pointColor.rgb());
            }
        }
    }
    result = result.scaled(image.width(), image.height());

    QImage sourceFull(image.width(), image.height(), QImage::Format_ARGB32);
    sourceFull.fill(WHITE);
    QPainter painterFull(&sourceFull);
    painterFull.setRenderHint(QPainter::Antialiasing, true);
    painterFull.setRenderHint(QPainter::TextAntialiasing, true);
    painterFull.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painterFull.fillPath(areas, BASE_COLOR);
    painterFull.setPen(QPen(BASE_COLOR, 1));
    painterFull.drawPath(paths);
    
    for (int x = 0; x < result.width(); x++) {
        for (int y = 0; y < result.height(); y++) {
            QColor res = result.pixel(x, y);
            int alpha = qAlpha(sourceFull.pixel(x, y));
            //std::cout << "alpha=" << alpha << "  " << sourceFull.pixel(x, y) << std::endl;
            res.setAlpha(alpha);
            result.setPixel(x, y, res.rgba());
        }
    }
    result.save("riverAreas.png");
    sourceFull.save("riverAreasFull.png");
    return result;
}

void OsmRiversHandler::finalize()
{
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    painter.setPen(QPen(BASE_COLOR, 2));
    painter.drawPath(paths);

    painter.fillPath(areas, BASE_COLOR);
    //painter.drawPath(areas);
    
    painter.drawImage(0, 0, paintAreas());
}

QImage OsmRiversHandler::getImage() const {
    return image;
}

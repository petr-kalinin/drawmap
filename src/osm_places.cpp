#include "osm_places.h"
#include "common.h"

#include <osmium/osm/area.hpp>
#include <QPainterPathStroker>
#include <Qt>

#include <set>
#include <cmath>

namespace {
    std::set<std::string> PLACES_TO_INCLUDE{"city", "town", "village", "hamlet", "allotments"};
    int VERTICAL_SHIFT = 15;
}

OsmPlacesHandler::OsmPlacesHandler(const Projector& proj_, const MinMax& minmax_, int imageSize) : 
    image(imageSize, imageSize, QImage::Format_ARGB32),
    proj(proj_),
    minmax(minmax_)
{
    image.fill({255, 255, 255, 0});
    double scaleX = image.width() / (minmax.maxx - minmax.minx);
    double scaleY = image.height() / (minmax.maxy - minmax.miny);
    scale = std::min(scaleX, scaleY);
}

bool OsmPlacesHandler::needArea(const osmium::Area& area) const {
    if ((area.get_value_by_key("landuse")) && (area.get_value_by_key("landuse") == std::string("allotments")))
        return true;
    if (!area.get_value_by_key("place"))
        return false;
    std::string type = area.get_value_by_key("place");
    return (PLACES_TO_INCLUDE.count(type) != 0);
}

void OsmPlacesHandler::area(const osmium::Area& area)  {
    if (!needArea(area)) return;
    if (area.num_rings().first == 0) {
        std::cerr << "Area with zero outer rings" << std::endl;
        return;
    }
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
            unitedPath += path;
            paths.push_back(path);
        }
    }
}

const QPainterPath& OsmPlacesHandler::getUnitedPath() const {
    return unitedPath;
}

void OsmPlacesHandler::setRoadsPath(const QPainterPath& path) {
    roadsPath = &path;
}

QPolygonF OsmPlacesHandler::simplifyPolygon(const QPolygonF& polygon) const {
    static const int DIST_THRESHOLD = 4;
    QPolygonF newPolygon;
    QPointF lastPoint(-1e10, -1e10);
    for (const auto& point: polygon) {
        double dist = std::hypot(point.x() - lastPoint.x(), point.y() - lastPoint.y());
        if (dist > DIST_THRESHOLD) {
            newPolygon << point;
            lastPoint = point;
        }
    }
    return newPolygon;
}

namespace {
    
typedef std::pair<QPainterPath, float> SidePath;
    
void addSidePaths(const QPolygonF& polygon, std::vector<SidePath>& paths) {
    QPointF lastPoint;
    for (const auto& point: polygon) {
        if (!lastPoint.isNull()) {
            QPainterPath path;
            path.moveTo(lastPoint);
            path.lineTo(lastPoint - QPointF(0, VERTICAL_SHIFT));
            path.lineTo(point - QPointF(0, VERTICAL_SHIFT));
            path.lineTo(point);
            path.lineTo(lastPoint);
            paths.emplace_back(path, (lastPoint.y() + point.y())/2.0);
        }
        lastPoint = point;
    }
}

}

void OsmPlacesHandler::finalize()
{
    QPainterPath roadsPathSimplified = roadsPath->simplified();
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    //painter.fillPath(unitedPath - roadsPathSimplified, QColor(255, 0, 0));
    //painter.fillPath(roadsPathSimplified, QColor(255, 0, 0));
    
    painter.setPen(QPen(QColor(0,0,0), 2));
    std::vector<SidePath> topPaths;
    std::vector<SidePath> sidePaths;
    for (auto& path: paths) {
        path -= roadsPathSimplified;
        path = path.simplified();
        auto polygons = path.toSubpathPolygons(QTransform());
        for (auto polygon: polygons) {
            addSidePaths(polygon, sidePaths);
        }
        QPainterPath newPath = path;
        newPath.translate(0, -VERTICAL_SHIFT);
        topPaths.emplace_back(newPath, -1);
    }
    std::sort(sidePaths.begin(), sidePaths.end(), [](const SidePath& a, const SidePath& b) { return a.second < b.second; });
    for (const auto paths: {&sidePaths, &topPaths}) {
        for (const auto& path: *paths) {
            painter.fillPath(path.first, QColor(128, 128, 128));
            painter.drawPath(path.first);
        }
    }
}

QImage OsmPlacesHandler::getImage() const {
    return image;
}
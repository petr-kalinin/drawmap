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
    
    //const QColor BASE_COLOR(0, 102, 255);
    const QColor BASE_COLOR(0, 51, 128);
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

QPolygonF simplifyPolygon(const QPolygonF& polygon, double threshold) {
    QPolygonF newPolygon;
    QPointF lastPoint(-1e10, -1e10);
    for (const auto& point: polygon) {
        double dist = std::hypot(point.x() - lastPoint.x(), point.y() - lastPoint.y());
        if (dist > threshold) {
            newPolygon << point;
            lastPoint = point;
        }
    }
    return newPolygon;
}

QPainterPath simplifyPath(const QPainterPath& path, double threshold) {
    QPainterPath result;
    QPointF lastAddedPoint(-1e10, -1e10);
    QPointF lastPoint(-1e10, -1e10);
    for (size_t i = 0; i < path.elementCount(); i++) {
        auto element = path.elementAt(i);
        if (element.isMoveTo()) {
            if (lastPoint.x() > -1e-9) {
                result.lineTo(lastPoint);
            }
            result.moveTo(element.x, element.y);
            lastAddedPoint = QPointF(element.x, element.y);
        } else {
            double dist = std::hypot(element.x - lastAddedPoint.x(), element.y - lastAddedPoint.y());
            if (dist > threshold) {
                result.lineTo(element.x, element.y);
                lastPoint = QPointF(element.x, element.y);
            }
        }
        lastPoint = QPointF(element.x, element.y);
    }
    result.lineTo(lastPoint);
    return result;
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

void OsmRiversHandler::finalize()
{
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    painter.setPen(QPen(BASE_COLOR, 2));
    painter.drawPath(paths);

    auto simplifiedAreas = simplifyPath(areas.simplified(), 3);
    
    painter.fillPath(simplifiedAreas, BASE_COLOR);
    for (int i = 4; i >= 1; i--) {
        QPainterPathStroker stroker;
        stroker.setWidth(15 * i);
        stroker.setJoinStyle(Qt::PenJoinStyle::RoundJoin);
        stroker.setCapStyle(Qt::PenCapStyle::RoundCap);

        QPainterPath outline = stroker.createStroke(simplifiedAreas);
        //outline = outline & simplifiedAreas;
        painter.fillPath(outline, BASE_COLOR.lighter(100+25*(5-i)));
    }

    painter.setPen(QPen(BASE_COLOR, 2));
    painter.drawPath(simplifiedAreas);

    
    QImage imageBase = image.copy();
    imageBase.fill({255, 255, 255, 0});

    QPainter painterBase(&imageBase);
    painterBase.setRenderHint(QPainter::Antialiasing, true);
    painterBase.setRenderHint(QPainter::TextAntialiasing, true);
    painterBase.setRenderHint(QPainter::SmoothPixmapTransform, true);

    painterBase.setPen(QPen(BASE_COLOR, 2));
    painterBase.drawPath(paths);
    painterBase.fillPath(simplifiedAreas, BASE_COLOR);
    painterBase.drawPath(simplifiedAreas);
    
    imageBase.save("riversBase.png");
    
    for (int x = 0; x < image.width(); x++)
        for (int y = 0; y < image.height(); y++) {
            QColor colorBase = QColor::fromRgba(imageBase.pixel(x, y));
            QColor color = image.pixel(x, y);
            color.setAlpha(colorBase.alpha());
            image.setPixel(x, y, color.rgba());
        }
    
    image.save("rivers.png");
}

QImage OsmRiversHandler::getImage() const {
    return image;
}

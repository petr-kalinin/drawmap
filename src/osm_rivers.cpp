#include "osm_rivers.h"
#include "common.h"

#include <osmium/osm/area.hpp>
#include <osmium/osm/way.hpp>
#include <QPainterPathStroker>
#include <Qt>

#include <set>
#include <map>
#include <cmath>

namespace {
    std::map<std::string, std::set<std::string>> TAGS_TO_INCLUDE{
        {"waterway", {"river", "riverbank", "canal"}},
        {"natural", {"water"}},
        {"landuse", {"reservoir"}},
    };
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
            areas.push_back(path);
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
        paths.push_back(path);
    }
}

void OsmRiversHandler::finalize()
{
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    //painter.fillPath(unitedPath - roadsPathSimplified, QColor(255, 0, 0));
    //painter.fillPath(roadsPathSimplified, QColor(255, 0, 0));
    
    painter.setPen(QPen(QColor(0, 102, 255), 2));
    for (auto& path: paths) {
        painter.drawPath(path);
    }
    for (auto& area: areas) {
        painter.fillPath(area, QColor(0, 102, 255));
        painter.drawPath(area);
    }
}

QImage OsmRiversHandler::getImage() const {
    return image;
}
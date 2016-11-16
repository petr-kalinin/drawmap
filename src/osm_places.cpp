#include "osm_places.h"
#include "common.h"

#include <osmium/osm/area.hpp>
#include <QPainterPathStroker>
#include <Qt>
#include <set>

namespace {
    std::set<std::string> PLACES_TO_INCLUDE{"city", "town", "village", "hamlet", "allotments"};
}

OsmPlacesHandler::OsmPlacesHandler(const Projector& proj_, const MinMax& minmax_, int imageSize) : 
    imageFill(imageSize, imageSize, QImage::Format_ARGB32),
    imageOutline(imageSize, imageSize, QImage::Format_ARGB32),
    painterFill(&imageFill),
    painterOutline(&imageOutline),
    proj(proj_),
    minmax(minmax_)
{
    imageFill.fill({255, 255, 255, 0});
    imageOutline.fill({255, 255, 255, 0});
    for (auto painter: {&painterFill, &painterOutline}) {
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    }
    double scaleX = imageFill.width() / (minmax.maxx - minmax.minx);
    double scaleY = imageFill.height() / (minmax.maxy - minmax.miny);
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
    if (area.num_rings().first == 1) {
        //return;
        std::cerr << "Area with >1 outer rings" << std::endl;
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
        painterOutline.setPen(QPen(QColor(0,0,0)));
        painterOutline.drawPath(path);
        painterFill.fillPath(path, QColor(128, 128, 128));
    }
}

QImage OsmPlacesHandler::getImage() const {
    return combine(imageOutline, imageFill);
}
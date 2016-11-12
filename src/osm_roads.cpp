#include "osm_roads.h"

#include <osmium/osm/way.hpp>

OsmRoadsHandler::OsmRoadsHandler(const Projector& proj_, const MinMax& minmax_, int imageSize) : 
    image(imageSize, imageSize, QImage::Format_ARGB32),
    painter(&image),
    proj(proj_),
    minmax(minmax_)
{
    image.fill({255, 255, 255});
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    double scaleX = image.width() / (minmax.maxx - minmax.minx);
    double scaleY = image.height() / (minmax.maxy - minmax.miny);
    scale = std::min(scaleX, scaleY);
}

void OsmRoadsHandler::way(const osmium::Way& way)  {
    if (!way.get_value_by_key("highway"))
        return;
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

const QImage& OsmRoadsHandler::getImage() const {
    return image;
}
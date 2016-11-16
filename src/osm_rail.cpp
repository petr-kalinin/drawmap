#include "osm_rail.h"
#include "common.h"

#include <osmium/osm/way.hpp>
#include <QPainterPathStroker>
#include <Qt>
#include <map>

OsmRailHandler::OsmRailHandler(const Projector& proj_, const MinMax& minmax_, int imageSize) : 
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

void OsmRailHandler::way(const osmium::Way& way)  {
    if (!way.get_value_by_key("railway"))
        return;
    std::string type = way.get_value_by_key("railway");
    if (type != "rail")
        return;
    if (way.get_value_by_key("service"))
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
    
    QPainterPathStroker stroker;
    stroker.setWidth(12);
    stroker.setJoinStyle(Qt::PenJoinStyle::RoundJoin);
    stroker.setCapStyle(Qt::PenCapStyle::FlatCap);
    
    QPainterPath strokeOutline = stroker.createStroke(path);
    
    stroker.setCapStyle(Qt::PenCapStyle::SquareCap);
    QPainterPath strokeFillBlack = stroker.createStroke(path);
    
    stroker.setDashPattern({4, 4});
    stroker.setCapStyle(Qt::PenCapStyle::FlatCap);
    QPainterPath strokeFillWhite = stroker.createStroke(path);
    
    painterOutline.setPen(QPen(QColor(0, 0, 0), 2));
    painterOutline.drawPath(strokeOutline);
    
    painterFill.fillPath(strokeFillBlack, QColor(0, 0, 0));
    painterFill.fillPath(strokeFillWhite, QColor(255, 255, 255));
}

QImage OsmRailHandler::getImage() const {
    return combine(imageOutline, imageFill);
}
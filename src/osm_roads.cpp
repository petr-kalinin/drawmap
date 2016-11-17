#include "osm_roads.h"
#include "common.h"

#include <osmium/osm/way.hpp>
#include <QPainterPathStroker>
#include <Qt>
#include <map>

namespace {
    enum class RoadType {MAIN, SIDE};
    
    struct RoadOptions {
        int width;
        RoadType type;
    };
    
    QColor mainRoadFill{204, 92, 17};
    QColor mainRoadOutline{153, 69, 13};
    QColor sideRoadFill{255, 255, 255};
    QColor sideRoadOutline{128, 128, 128};
    
    int BASE_WIDTH = 8;
    
    std::map<std::string, RoadOptions> options {
        {"motorway", {BASE_WIDTH*3, RoadType::MAIN}},
        {"motorway_link", {BASE_WIDTH, RoadType::MAIN}},
        {"trunk", {BASE_WIDTH*3, RoadType::MAIN}},
        {"trunk_link", {BASE_WIDTH, RoadType::MAIN}},
        {"primary", {BASE_WIDTH*2, RoadType::MAIN}},
        {"primary_link", {BASE_WIDTH, RoadType::MAIN}},
        {"secondary", {BASE_WIDTH*2, RoadType::MAIN}},
        {"secondary_link", {BASE_WIDTH, RoadType::MAIN}},
        {"tertiary", {BASE_WIDTH*2, RoadType::SIDE}},
        {"tertiary_link", {BASE_WIDTH, RoadType::SIDE}},
        {"unclassified", {BASE_WIDTH, RoadType::SIDE}}//,
        //{"residential", {BASE_WIDTH, RoadType::SIDE}},
        //{"service", {BASE_WIDTH, RoadType::SIDE}}
    };
}

OsmRoadsHandler::OsmRoadsHandler(const Projector& proj_, const MinMax& minmax_, int imageSize) : 
    image(imageSize, imageSize, QImage::Format_ARGB32),
    proj(proj_),
    minmax(minmax_)
{
    image.fill({255, 255, 255, 0});
    double scaleX = image.width() / (minmax.maxx - minmax.minx);
    double scaleY = image.height() / (minmax.maxy - minmax.miny);
    scale = std::min(scaleX, scaleY);
}

void OsmRoadsHandler::finalize()
{
    sidePath -= *placesPath;
    mainPath -= *placesPath;
    
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    painter.setPen(QPen(sideRoadOutline, 4));
    painter.drawPath(sidePath);
    painter.setPen(QPen(mainRoadOutline, 4));
    painter.drawPath(mainPath);
    
    painter.fillPath(sidePath, sideRoadFill);
    painter.fillPath(mainPath, mainRoadFill);
}

void OsmRoadsHandler::way(const osmium::Way& way)  {
    if (!way.get_value_by_key("highway"))
        return;
    std::string type = way.get_value_by_key("highway");
    auto option = options.find(type);
    if (option == options.end())
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
    stroker.setWidth(option->second.width);
    stroker.setJoinStyle(Qt::PenJoinStyle::RoundJoin);
    stroker.setCapStyle(Qt::PenCapStyle::RoundCap);
    QPainterPath strokeOutline = stroker.createStroke(path);
    
    static int nInside = 0; 
    if (strokeOutline.intersects(QRectF(0, 0, image.width(), image.height()))) {
        nInside++;
        if (nInside % 100 == 0) std::cout << nInside << std::endl;
        if (option->second.type == RoadType::MAIN)
            mainPath += strokeOutline;
        else
            sidePath += strokeOutline;
        unitedPath += strokeOutline;
    }
}

const QPainterPath& OsmRoadsHandler::getUnitedPath() const {
    return unitedPath;
}

void OsmRoadsHandler::setPlacesPath(const QPainterPath& path) {
    placesPath = &path;
}

QImage OsmRoadsHandler::getImage() const {
    return image;
}
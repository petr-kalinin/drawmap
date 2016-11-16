#include "osm_roads.h"
#include "common.h"

#include <osmium/osm/way.hpp>
#include <QPainterPathStroker>
#include <Qt>
#include <map>

namespace {
    enum class RoadType {MAIN, SIDE};
    
    struct RoadOptions {
        QColor fillColor;
        QColor borderColor;
        int width;
        RoadType type;
    };
    
    QColor mainRoadFill{204, 92, 17};
    QColor mainRoadOutline{153, 69, 13};
    QColor sideRoadFill{255, 255, 255};
    QColor sideRoadOutline{128, 128, 128};
    
    int BASE_WIDTH = 8;
    
    std::map<std::string, RoadOptions> options {
        {"motorway", {mainRoadFill, mainRoadOutline, BASE_WIDTH*3, RoadType::MAIN}},
        {"motorway_link", {mainRoadFill, mainRoadOutline, BASE_WIDTH, RoadType::MAIN}},
        {"trunk", {mainRoadFill, mainRoadOutline, BASE_WIDTH*3, RoadType::MAIN}},
        {"trunk_link", {mainRoadFill, mainRoadOutline, BASE_WIDTH, RoadType::MAIN}},
        {"primary", {mainRoadFill, mainRoadOutline, BASE_WIDTH*2, RoadType::MAIN}},
        {"primary_link", {mainRoadFill, mainRoadOutline, BASE_WIDTH, RoadType::MAIN}},
        {"secondary", {mainRoadFill, mainRoadOutline, BASE_WIDTH*2, RoadType::MAIN}},
        {"secondary_link", {mainRoadFill, mainRoadOutline, BASE_WIDTH, RoadType::MAIN}},
        {"tertiary", {sideRoadFill, sideRoadOutline, BASE_WIDTH*2, RoadType::SIDE}},
        {"tertiary_link", {sideRoadFill, sideRoadOutline, BASE_WIDTH, RoadType::SIDE}},
        {"unclassified", {sideRoadFill, sideRoadOutline, BASE_WIDTH, RoadType::SIDE}}//,
        //{"residential", {sideRoadFill, sideRoadOutline, 3, RoadType::SIDE}},
        //{"service", {sideRoadFill, sideRoadOutline, 3, RoadType::SIDE}}
    };
}

OsmRoadsHandler::OsmRoadsHandler(const Projector& proj_, const MinMax& minmax_, int imageSize) : 
    imageFillMain(imageSize, imageSize, QImage::Format_ARGB32),
    imageFillSide(imageSize, imageSize, QImage::Format_ARGB32),
    imageOutline(imageSize, imageSize, QImage::Format_ARGB32),
    painterFillMain(&imageFillMain),
    painterFillSide(&imageFillSide),
    painterOutline(&imageOutline),
    proj(proj_),
    minmax(minmax_)
{
    imageFillMain.fill({255, 255, 255, 0});
    imageFillSide.fill({255, 255, 255, 0});
    imageOutline.fill({255, 255, 255, 0});
    for (auto painter: {&painterFillMain, &painterFillSide, &painterOutline}) {
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    }
    double scaleX = imageFillMain.width() / (minmax.maxx - minmax.minx);
    double scaleY = imageFillMain.height() / (minmax.maxy - minmax.miny);
    scale = std::min(scaleX, scaleY);
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
    stroker.setCapStyle(Qt::PenCapStyle::FlatCap);
    
    QPainterPath strokeOutline = stroker.createStroke(path);
    
    stroker.setCapStyle(Qt::PenCapStyle::SquareCap);
    QPainterPath strokeFill = stroker.createStroke(path);
    
    painterOutline.setPen(QPen(option->second.borderColor, 2));
    painterOutline.drawPath(strokeOutline);
    
    if (option->second.type == RoadType::MAIN)
        painterFillMain.fillPath(strokeFill, option->second.fillColor);
    else
        painterFillSide.fillPath(strokeFill, option->second.fillColor);

    static int nInside = 0; 
    if (strokeFill.intersects(QRectF(0, 0, imageFillMain.width(), imageFillMain.height()))) {
        nInside++;
        if (nInside % 100 == 0) std::cout << nInside << std::endl;
        allRoadsPath += strokeFill;
    }
}

QImage OsmRoadsHandler::getImage() const {
    const_cast<QPainter&>(painterFillMain).fillPath(allRoadsPath, QColor(0, 255, 0));
    return combine(combine(imageOutline, imageFillSide), imageFillMain);
}
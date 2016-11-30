#include "osm_roads.h"
#include "common.h"

#include <osmium/osm/way.hpp>
#include <QPainterPathStroker>
#include <Qt>
#include <map>
#include <unordered_map>

namespace {
    struct RoadOptions {
        int width;
        RoadType type;
    };
    
    QColor mainRoadFill{204, 92, 17};
    QColor mainRoadOutline{153, 69, 13};
    QColor sideRoadFill{255, 255, 255};
    QColor sideRoadOutline{128, 128, 128};

    std::map<RoadType, QColor> fillColor{{RoadType::MAIN, mainRoadFill}, {RoadType::SIDE, sideRoadFill}};
    std::map<RoadType, QColor> outlineColor{{RoadType::MAIN, mainRoadOutline}, {RoadType::SIDE, sideRoadOutline}};
    
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

void OsmRoadsHandler::way(const osmium::Way& way)  {
    if (!way.get_value_by_key("highway"))
        return;
    std::string type = way.get_value_by_key("highway");
    auto option = options.find(type);
    if (option == options.end())
        return;
    QPainterPath path0;
    bool first = true;
    for (const auto& node: way.nodes()) {
        if (!node.location())
            continue;
        point p = proj.transform({node.lon(), node.lat()});
        double x = scale * (p.x-minmax.minx);
        double y = scale * (minmax.maxy-p.y);
        if (first) {
            path0.moveTo(x, y);
            first = false;
        } else {
            path0.lineTo(x, y);
        }
    }
    QPainterPathStroker stroker;
    stroker.setWidth(0.1);
    stroker.setJoinStyle(Qt::PenJoinStyle::RoundJoin);
    stroker.setCapStyle(Qt::PenCapStyle::RoundCap);

    QPainterPath path = stroker.createStroke(path0);

    stroker.setWidth(option->second.width);
    stroker.setJoinStyle(Qt::PenJoinStyle::RoundJoin);
    stroker.setCapStyle(Qt::PenCapStyle::RoundCap);
    QPainterPath strokeOutline = stroker.createStroke(path0);
    
    stroker.setWidth(option->second.width + 4);
    QPainterPath strokeOutlineWide = stroker.createStroke(path0);
    
    static int nInside = 0; 
    if (strokeOutline.intersects(QRectF(0, 0, image.width(), image.height()))) {
        nInside++;
        if (nInside % 100 == 0) std::cout << nInside << std::endl;
        unitedPath += strokeOutlineWide;
        paths.push_back({path, option->second.type, option->second.width});
        if (option->second.type == RoadType::MAIN) 
            mainPath += strokeOutline;
        else
            sidePath += strokeOutline;
    }
}

void OsmRoadsHandler::finalize()
{
    sort(paths.begin(), paths.end(), 
         [](const RoadPath& a, const RoadPath& b) { return a.type>b.type; } );
    
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    {
        QPainterPathStroker stroker;
        stroker.setWidth(4);
        stroker.setJoinStyle(Qt::PenJoinStyle::RoundJoin);
        stroker.setCapStyle(Qt::PenCapStyle::RoundCap);

        QPainterPath outlineSide = stroker.createStroke(sidePath) - *placesPath;
        painter.fillPath(outlineSide, outlineColor[RoadType::SIDE]);
        QPainterPath outlineMain = stroker.createStroke(mainPath) - *placesPath;
        painter.fillPath(outlineMain, outlineColor[RoadType::MAIN]);
    }
    
    painter.fillPath(mainPath + sidePath, QColor(255, 255, 255));
    
    for (const auto& ppath: paths) {
        auto path = ppath.path;
        path -= *placesPath;
        
        QPainterPathStroker stroker;
        stroker.setWidth(ppath.width);
        stroker.setJoinStyle(Qt::PenJoinStyle::RoundJoin);
        if (ppath.type == RoadType::MAIN) {
            stroker.setCapStyle(Qt::PenCapStyle::SquareCap);
        } else 
            stroker.setCapStyle(Qt::PenCapStyle::FlatCap);
        
        QPainterPath outline = stroker.createStroke(path);
        
        painter.fillPath(outline, fillColor[ppath.type]);
        //painter.fillPath(outline, QColor(255, 0, 0, 128));
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
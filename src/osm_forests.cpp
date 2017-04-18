#include "osm_forests.h"
#include "common.h"
#include "srtm.h"

#include <osmium/osm/area.hpp>
#include <osmium/osm/way.hpp>
#include <QPainterPathStroker>
#include <Qt>

#include <set>
#include <map>
#include <cmath>
#include <random>
#include <queue>
#include <random>
#include <iostream>

namespace {
    std::map<std::string, std::set<std::string>> TAGS_TO_INCLUDE{
        {"natural", {"wood", "scrub", "canal"}},
        {"landuse", {"forest", "cemetery", "orchard", "vineyard", "allotments"}},
        {"landcover", {"trees"}},
        {"leisure", {"park"}}
    };
    
    const QColor BASE_COLOR(0, 128, 0);
    const int MARGIN = 100;
}

OsmForestsHandler::OsmForestsHandler(const Projector& proj_, const MinMax& minmax_, int imageSize, int xTile_, int yTile_) : 
    image(imageSize, imageSize, QImage::Format_ARGB32),
    proj(proj_),
    minmax(minmax_),
    xTile(xTile_),
    yTile(yTile_)
{
    image.fill({255, 255, 255, 0});
    double scaleX = image.width() / (minmax.maxx - minmax.minx);
    double scaleY = image.height() / (minmax.maxy - minmax.miny);
    scale = std::min(scaleX, scaleY);
}

template<class Object>
bool OsmForestsHandler::needObject(const Object& object) const {
    for (const auto& tag: TAGS_TO_INCLUDE) {
        if (object.get_value_by_key(tag.first.c_str())) {
            std::string type = object.get_value_by_key(tag.first.c_str());
            if (tag.second.count(type) != 0)
                return true;
        }
    }
    return false;
}

void OsmForestsHandler::area(const osmium::Area& area)  {
    if (!needObject(area)) return;
    for (const auto& ring: area.outer_rings()) {
        QPainterPath path;
        bool first = true;
        for (const auto& node: ring) {
            if (!node.location())
                continue;
            point p = proj.transform({node.lon(), node.lat()});
            double x = scale * (p.x-minmax.minx) + MARGIN;
            double y = scale * (minmax.maxy-p.y) + MARGIN;
            if (first) {
                path.moveTo(x, y);
                first = false;
            } else {
                path.lineTo(x, y);
            }
        }
        if (path.intersects(QRectF(0, 0, image.width() + 2*MARGIN, image.height() + 2*MARGIN))) {
            areas += path;
        }
    }
}

namespace {
    void clip(int& c) {
        if (c < 0) c = 0;
        if (c > 255) c = 255;
    }
        
    QImage paintGrads(const cv::Mat& xGrad, const cv::Mat& yGrad) {
        QColor dark{0, 64, 0};
        QColor light{64, 255, 32};
        QImage image(xGrad.cols, xGrad.rows, QImage::Format_ARGB32);
        QPainter painter(&image);
        double corr = 100;
        for (int x=0; x<image.width(); x++) {
            for (int y=0; y<image.height(); y++) {
                float xg = xGrad.at<float>(y, x) / corr;
                float yg = yGrad.at<float>(y, x) / corr;
                float f1 = (-1*yg) / sqrt(1 + 0.3*0.3) / sqrt(xg*xg + yg*yg + 1);
                float f2 = (2*yg - 1*xg - 0.5)/sqrt(2*2 + 1*1 + 0.5*0.5) / sqrt(xg*xg + yg*yg + 1);
                f1 = (1 + f1) / 2;
                f2 = (1 + f2) / 2;
                int r = (dark.red()*f1 + light.red()*f2) / (f1+f2);
                int g = (dark.green()*f1 + light.green()*f2) / (f1+f2);
                int b = (dark.blue()*f1 + light.blue()*f2) / (f1+f2);
                clip(r);
                clip(g);
                clip(b);
                QColor color({r, g, b});
                image.setPixel(x, y, color.rgba());
            }
        }
        return image;
    }
}

void OsmForestsHandler::finalize()
{
    QImage imageBase(image.width() + 2*MARGIN, image.height() + 2*MARGIN, QImage::Format_ARGB32);
    imageBase.fill({255, 255, 255, 0});
    QPainter painterBase(&imageBase);
    painterBase.fillPath(areas, BASE_COLOR);
    
    //std::random_device r;
    //std::default_random_engine e1(r());
    static const double PLACER_THRESHOLD = 1e-1;
    static const double BASE_HEIGHT = 0;
    
    
    cv::Mat source(imageBase.height(), imageBase.width(), CV_32FC1, 0.0);
    std::cout << "tile " << xTile << " " << yTile << std::endl;
    for (int y=0; y<imageBase.height(); y++) {
        for (int x=0; x<imageBase.width(); x++) {
            int seed = xTile * image.width() + x - MARGIN + 10000 * (yTile * image.height() + (y - MARGIN));
            std::mt19937 e1(seed);
            std::uniform_real_distribution<float> noise(0, 1.5);
            std::uniform_real_distribution<float> placer(0, 1);
            std::normal_distribution<double> height(10, 2);
            std::normal_distribution<double> radius(10, 1);
            
            //if (x >= MARGIN && x < imageBase.width()-MARGIN && y >= MARGIN && y < imageBase.height()-MARGIN)
                //std::cout << "*";
            //std::cout << placer(e1) << " ";
            //source.at<float>(y, x) = placer(e1);
            if ((imageBase.pixel(x, y) & 0xffffff) == (BASE_COLOR.rgb() & 0xffffff) && placer(e1) < PLACER_THRESHOLD) {
                double h = height(e1);
                double r = radius(e1);
                if (h < 0 || r < 0) continue;
                //std::cout << "h=" << h << " r=" << r << std::endl;
                for (int dx = -std::ceil(r); dx < std::ceil(r); dx++) 
                    for (int dy = -std::ceil(r); dy < std::ceil(r); dy++) {
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx < 0 || nx >= source.cols || ny < 0 || ny >= source.rows)
                            continue;
                        double d = dx*dx + dy*dy;
                        double rem = 1 - d / r / r;
                        //std::cout << dx << " " << dy << " " << d << " " << rem << std::endl;
                        if (rem < 0) continue;
                        double ch = std::sqrt(rem) * h + noise(e1) + BASE_HEIGHT;
                        if (ch > source.at<float>(y+dy, x+dx))
                            source.at<float>(y+dy, x+dx) = ch;
                    }
            }
        }
        //std::cout << std::endl;
    }
    source.at<float>(MARGIN,MARGIN) = -1;
    source.at<float>(source.cols-MARGIN,MARGIN) = -1;
    source.at<float>(source.cols-MARGIN,source.rows-MARGIN) = 30;
    source.at<float>(MARGIN,source.rows-MARGIN) = 30;
    
    auto totalHeights = source.clone();
    for (int x=0; x<imageBase.width(); x++)
        for (int y=0; y<imageBase.height(); y++) {
            int xx = std::min(std::max(x-MARGIN, 0), image.width()-1);
            int yy = std::min(std::max(y-MARGIN, 0), image.height()-1);
            totalHeights.at<float>(y,x) += heights.at<float>(yy,xx);
        }
    cvPaint::paint(source).save(("source0" + std::to_string(xTile) + std::to_string(yTile) + ".png").c_str());
    cvPaint::paint(totalHeights).save(("totalHeights" + std::to_string(xTile) + std::to_string(yTile) + ".png").c_str());

    //cv::Mat sourceSmoothed;
    //cv::bilateralFilter(source, sourceSmoothed, -1, 10, 5);
    //source = sourceSmoothed;
    
    //cvPaint::paint(source).save("source1.png");
    
    cv::Mat sourceXgrad;
    Sobel(totalHeights, sourceXgrad, -1, 1, 0, 5);
    cv::Mat sourceYgrad;
    Sobel(totalHeights, sourceYgrad, -1, 0, 1, 5);
    
    cvPaint::paint(sourceXgrad).save(("sourceX" + std::to_string(xTile) + std::to_string(yTile) + ".png").c_str());
    cvPaint::paint(sourceYgrad).save(("sourceY" + std::to_string(xTile) + std::to_string(yTile) + ".png").c_str());
    
    static const double VAL_THRESHOLD = 5;
    auto imageGrad = paintGrads(sourceXgrad, sourceYgrad);
    //auto imageGrad = cvPaint::paint(source);
    for (int x=0; x<image.width(); x++)
        for (int y=0; y<image.height(); y++) {
            double val = source.at<float>(y+MARGIN,x+MARGIN);
            int alpha = 0;
            if (val > VAL_THRESHOLD)
                alpha = 255;
            else if (val < 0) 
                alpha = 0;
            else alpha = (val / VAL_THRESHOLD * 256);
            
            //int seed = xTile * image.width() + x - MARGIN + 10000 * (yTile * image.height() + (y - MARGIN));
            //int r = (seed + 60000) * 255 / 120000;
            //QColor color(r, r, r);

            QColor color = imageGrad.pixel(x+MARGIN, y+MARGIN);
            color.setAlpha(alpha);
            image.setPixel(x, y, color.rgba());
        }
    image.save("forests.png");
}

QImage OsmForestsHandler::getImage() const {
    return image;
}

const QPainterPath& OsmForestsHandler::getAreas() const {
    return areas;
}

void OsmForestsHandler::setHeights(cv::Mat mat) {
    heights = mat;
}

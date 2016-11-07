#include <QImage>
#include <QPainter>

#include <proj_api.h>

#include <iostream>
#include <cmath>
#include <cstdlib>
#include <fstream>

#include <getopt.h>

#include <osmium/area/assembler.hpp>
#include <osmium/area/multipolygon_collector.hpp>
#include <osmium/dynamic_handler.hpp>
#include <osmium/geom/wkt.hpp>
#include <osmium/handler/dump.hpp>
#include <osmium/handler/node_locations_for_ways.hpp>
#include <osmium/index/map/dummy.hpp>
#include <osmium/index/map/sparse_mem_array.hpp>
#include <osmium/io/pbf_input.hpp>
#include <osmium/visitor.hpp>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

typedef osmium::index::map::Dummy<osmium::unsigned_object_id_type, osmium::Location> index_neg_type;
typedef osmium::index::map::SparseMemArray<osmium::unsigned_object_id_type, osmium::Location> index_pos_type;
typedef osmium::handler::NodeLocationsForWays<index_pos_type, index_neg_type> location_handler_type;

const int IMAGE_SIZE = 4000;
std::vector<int> dx{-1,-1,-1, 0, 1, 1, 1, 0};
std::vector<int> dy{-1, 0, 1, 1, 1, 0, -1,-1};

struct point {
    double x,y;
};

class Projector {
public:
    Projector() :
        latlonProj(pj_init_plus("+proj=latlong +datum=WGS84")),
        resultProj(pj_init_plus("+init=epsg:3857"))
    {}
    
    point transform(point p) const {
        p.x *= M_PI/180;
        p.y *= M_PI/180;
        pj_transform(latlonProj, resultProj, 1, 1, &p.x, &p.y, NULL);
        return p;
    }
    
    point invertTransform(point p) const {
        pj_transform(resultProj, latlonProj, 1, 1, &p.x, &p.y, NULL);
        p.x /= M_PI/180;
        p.y /= M_PI/180;
        return p;
    }
    
private:
    projPJ latlonProj;
    projPJ resultProj;
};

struct MinMax {
    double maxx, maxy, minx, miny;
};

QImage paint(const cv::Mat& mat) {
    QImage image(mat.cols, mat.rows, QImage::Format_ARGB32);
    QPainter painter(&image);
    double maxVal, minVal;
    cv::minMaxLoc(mat, &minVal, &maxVal);
    for (int x=0; x<image.width(); x++) {
        for (int y=0; y<image.height(); y++) {
            float val = mat.at<float>(y, x);
            int col = (val - minVal) / (maxVal - minVal) * 255;
            QColor color({col, col, col});
            image.setPixel(x, y, color.rgba());
        }
    }
    return image;
}

void clip(int& c) {
    if (c < 0) c = 0;
    if (c > 255) c = 255;
}
    
QImage paintGrads(const cv::Mat& xGrad, const cv::Mat& yGrad) {
    QImage image(xGrad.cols, xGrad.rows, QImage::Format_ARGB32);
    QPainter painter(&image);
    double maxVal = 2000;
    double yellowFac = 0.5;
    for (int x=0; x<image.width(); x++) {
        for (int y=0; y<image.height(); y++) {
            float xg = xGrad.at<float>(y, x);
            float yg = yGrad.at<float>(y, x);
            float gray = (-xg - 3*yg)/sqrt(3*3+1*1);
            float yellow = yellowFac * (2 * xg + yg)/sqrt(2*2+1*1);
            if (yellow < 0) yellow = 0;
            if (gray < 0) gray = 0;
            gray = gray/maxVal*255;
            yellow = yellow/maxVal*255;
            int r = 255 - gray;
            int g = 255 - gray - 0.05*yellow;
            int b = 255 - gray - yellow;
            clip(r);
            clip(g);
            clip(b);
            QColor color({r, g, b});
            image.setPixel(x, y, color.rgba());
        }
    }
    return image;
}
    
class SRTMProvider {
public:
    typedef std::vector<std::vector<int32_t>> Heights;
    
    SRTMProvider(const Projector& proj_) : 
        proj(proj_)
    {}
    
    const Heights& getHeights(int x, int y) {
        if (heights.count({x,y}) == 0)
            heights[{x,y}] = loadHeights(x,y);
        return heights[{x,y}];
    }

    int16_t getHeight(double x, double y) {
        int xx = std::floor(x);
        int yy = std::floor(y);
        const Heights& thisHeights = getHeights(xx, yy);
        int fx = (x - xx) * SRTMSize;
        int fy = (1 - (y - yy)) * SRTMSize;
        //std::cout << "getHeight(" << x << " " << y << "   " << xx << " " << yy << "    " << fx << " " << fy << std::endl;
        return thisHeights[fx][fy];
    }
    
    static const int SRTMSize = 3601;
private:
    
    const Projector& proj;
    std::map<std::pair<int, int>, Heights> heights;
    
    Heights loadHeights(std::string filename) {
        std::ifstream file(filename, std::ios::in|std::ios::binary);
        if (!file) {
            throw std::runtime_error("Can't open file " + filename);
        }

        unsigned char buffer[2];
        Heights heights;
        heights.resize(SRTMSize);
        for (int i = 0; i < SRTMSize; ++i) {
            heights[i].resize(SRTMSize);
        }
        for (int i = 0; i < SRTMSize; ++i) {
            for (int j = 0; j < SRTMSize; ++j) {
                if (!file.read(reinterpret_cast<char*>(buffer), sizeof(buffer))) {
                    throw std::runtime_error("Can't read data from file " + filename);
                }
                heights[j][i] = (buffer[0] << 8) | buffer[1];
            }
        }
        return heights;
    }
    
    Heights loadHeights(int x, int y) {
        char cx='E', cy='N';
        if (x < 0) {
            x = -x;
            cx = 'W';
        }
        if (y < 0) {
            y = -y;
            cy = 'S';
        }
        char filename[8];
        snprintf(filename, 8, "%c%02d%c%03d", cy, y, cx, x);
        
        system((std::string("./download_srtm.sh ") + filename).c_str());
        
        return loadHeights(std::string("srtm/") + filename + ".hgt");
    }
};

class SRTMtoCV {
public:
    typedef SRTMProvider::Heights Heights;
    static const int SRTMSize = SRTMProvider::SRTMSize;
    
    SRTMtoCV(const Projector& proj_, const MinMax& minmax_) : 
        provider(proj_),
        minmax(minmax_),
        proj(proj_)
    {
        calc();
    }
    

    void calc() {
        MinMax floorMinMax;
        point minp = proj.invertTransform({minmax.minx, minmax.miny});
        floorMinMax.minx = floor(minp.x);
        floorMinMax.miny = floor(minp.y);
        point maxp = proj.invertTransform({minmax.maxx, minmax.maxy});
        floorMinMax.maxx = floor(maxp.x);
        floorMinMax.maxy = floor(maxp.y);
        cv::Mat source(
            (provider.SRTMSize-1) * (floorMinMax.maxy-floorMinMax.miny+1) + 1,
            (provider.SRTMSize-1) * (floorMinMax.maxx-floorMinMax.minx+1) + 1,
            CV_32FC1,
            -1
        );
        for (int x = floorMinMax.minx; x<=floorMinMax.maxx; x++) {
            for (int y = floorMinMax.miny; y<=floorMinMax.maxy; y++) {
                const Heights& heights = provider.getHeights(x, y);
                for (int dx = 0; dx < SRTMSize; dx++) {
                    for (int dy = 0; dy < SRTMSize; dy++) {
                        int nx = (x-floorMinMax.minx)*(SRTMSize-1)+dx;
                        int ny = (floorMinMax.maxy-y)*(SRTMSize-1)+dy;
                        source.at<float>(ny, nx) = heights[dx][dy];
                    }
                }
            }
        }
        cv::Mat sourceSmoothed;
        cv::bilateralFilter(source, sourceSmoothed, -1, 10, 5);
        source = sourceSmoothed;
        writeMatrix(source, "source");
        
        paint(source).save("source.png");
        
        cv::Mat sourceXgrad;
        Sobel(source, sourceXgrad, -1, 1, 0, 5);
        cv::Mat sourceYgrad;
        Sobel(source, sourceYgrad, -1, 0, 1, 5);

        paint(sourceXgrad).save("sourceXgrad.png");
        paint(sourceYgrad).save("sourceYgrad.png");
        
        cvHeights.create(IMAGE_SIZE, IMAGE_SIZE, CV_32FC1);
        cv::Mat trX(cvHeights.rows, cvHeights.cols, CV_32FC1, -1);
        cv::Mat trY(cvHeights.rows, cvHeights.cols, CV_32FC1, -1);
        for (int x=0; x<cvHeights.cols; x++) {
            for (int y=0; y<cvHeights.rows; y++) {
                double rx = (minmax.minx + 1.0*x/cvHeights.cols*(minmax.maxx-minmax.minx));
                double ry = (minmax.maxy - 1.0*y/cvHeights.rows*(minmax.maxy-minmax.miny));
                point p = proj.invertTransform({rx, ry});
                double fx = (p.x - floorMinMax.minx) * SRTMSize;
                double fy = (floorMinMax.maxy + 1 - p.y) * SRTMSize;
                trX.at<float>(y, x) = fx;
                trY.at<float>(y, x) = fy;
            }
        }
        std::cout << "Before remap " << std::endl;
        cv::remap(source, cvHeights, trX, trY, cv::INTER_LINEAR, cv::BORDER_TRANSPARENT);
        cv::remap(sourceXgrad, xGrad, trX, trY, cv::INTER_LINEAR, cv::BORDER_TRANSPARENT);
        cv::remap(sourceYgrad, yGrad, trX, trY, cv::INTER_LINEAR, cv::BORDER_TRANSPARENT);
        writeMatrix(trX, "trX");
        writeMatrix(trY, "trY");
        writeMatrix(cvHeights, "cvHeights");
    }
    
    cv::Mat getCvHeights() {
        return cvHeights;
    }
    
    cv::Mat getXGrad() {
        return xGrad;
    }
    
    cv::Mat getYGrad() {
        return yGrad;
    }
    
private:
    SRTMProvider provider;
    const MinMax& minmax;
    const Projector& proj;
    cv::Mat cvHeights;
    cv::Mat xGrad, yGrad;
    
    void writeMatrix(const cv::Mat& mat, const std::string& filename) {
        std::ofstream f(filename);
        for (int x=0; x<mat.size().width; x++) {
            for (int y=0; y<mat.size().height; y++) {
                f << mat.at<float>(y,x) << " ";
            }
            f << "\n";
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " OSMFILE\n";
        exit(1);
    }

    Projector proj;
    MinMax minmax;
    
    /*
    minmax.minx = 4550000; // 4860000
    minmax.maxx = 5200000; // 4880000
    minmax.miny = 7300000; // 7555000
    */
    
    /*
    point center = proj.transform({41.720581, 57.858635});
    minmax.minx = center.x - 100000;
    minmax.maxx = center.x + 100000;
    minmax.miny = center.y - 100000;
    */
    
    
    point center = proj.transform({43.739319, 56.162759});
    minmax.minx = center.x - 10000;
    minmax.maxx = center.x + 10000;
    minmax.miny = center.y - 10000;
    
    
    
    /*
    point center = proj.transform({37.6, 55.8});
    minmax.minx = center.x - 1000000;
    minmax.maxx = center.x + 1000000;
    minmax.miny = center.y - 1000000;
    */
    
    minmax.maxy = minmax.miny + (minmax.maxx - minmax.minx); 
    
    SRTMtoCV srtm(proj, minmax);
    
    paint(srtm.getCvHeights()).save("test-cv.png");
    paint(srtm.getXGrad()).save("test-xgrad.png");
    paint(srtm.getYGrad()).save("test-ygrad.png");
    paintGrads(srtm.getXGrad(), srtm.getYGrad()).save("test-grads.png");
    
    return 0;
}


#pragma once

#include "common.h"

#include <QImage>

#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include <map>

class SRTMProvider {
public:
    typedef std::vector<std::vector<int32_t>> Heights;
    
    SRTMProvider(const Projector& proj_);
    
    const Heights& getHeights(int x, int y);

    int16_t getHeight(double x, double y);
    
private:
    
    const Projector& proj;
    std::map<std::pair<int, int>, Heights> heights;
    
    Heights loadHeights(std::string filename);
    
    Heights loadHeights(int x, int y);
};

class SRTMtoCV {
public:
    typedef SRTMProvider::Heights Heights;
    
    SRTMtoCV(const Projector& proj_, const MinMax& minmax_, int imageSize_);

    void calc();
    
    cv::Mat getCvHeights();
    
    cv::Mat getXGrad();
    
    cv::Mat getYGrad();
    
private:
    SRTMProvider provider;
    const MinMax& minmax;
    const Projector& proj;
    cv::Mat cvHeights;
    cv::Mat xGrad, yGrad;
    int imageSize;
    
    void writeMatrix(const cv::Mat& mat, const std::string& filename);
};

namespace cvPaint {
    
    QImage paint(const cv::Mat& mat);

    QImage paintGrads(const cv::Mat& xGrad, const cv::Mat& yGrad);
    
}        

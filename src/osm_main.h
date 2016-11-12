#pragma once

#include "osm_common.h"

#include <string>
#include <vector>

class OsmDrawer {
public:
    OsmDrawer();
    
    void addHandler(BaseHandler* handler);
    
    void dispatch(const std::string& filename);
    
private:
    std::vector<BaseHandler*> handlers;
};
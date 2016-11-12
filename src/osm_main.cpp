#include "osm_main.h"
#include "osm_common.h"

#include <osmium/osm/types.hpp>
#include <osmium/index/map/dummy.hpp>
#include <osmium/index/map/sparse_mem_array.hpp>
#include <osmium/handler/node_locations_for_ways.hpp>
#include <osmium/io/file.hpp>
#include <osmium/area/assembler.hpp>
#include <osmium/area/multipolygon_collector.hpp>
#include <osmium/io/pbf_input.hpp>

typedef osmium::index::map::Dummy<osmium::unsigned_object_id_type, osmium::Location> IndexNeg;
typedef osmium::index::map::SparseMemArray<osmium::unsigned_object_id_type, osmium::Location> IndexPos;
typedef osmium::handler::NodeLocationsForWays<IndexPos, IndexNeg> LocationHandler;

namespace {
class ProxyHandler: public BaseHandler {
public:
    ProxyHandler(std::vector<BaseHandler*>& handlers_):
        handlers(handlers_) {}
        
    virtual void osm_object (const osmium::OSMObject &o) const noexcept {
        for (auto& h : handlers) h->osm_object(o);
    }
 
    virtual void node (const osmium::Node & o) const noexcept {
        for (auto& h : handlers) h->node(o);
    }
 
    virtual void way (const osmium::Way &o) const noexcept {
        for (auto& h : handlers) h->way(o);
    }
 
    virtual void relation (const osmium::Relation &o) const noexcept {
        for (auto& h : handlers) h->relation(o);
    }
 
    virtual void area (const osmium::Area &o) const noexcept {
        for (auto& h : handlers) h->area(o);
    }
 
    virtual void changeset (const osmium::Changeset &o) const noexcept {
        for (auto& h : handlers) h->changeset(o);
    }
 
    virtual void tag_list (const osmium::TagList &o) const noexcept {
        for (auto& h : handlers) h->tag_list(o);
    }
 
    virtual void way_node_list (const osmium::WayNodeList &o) const noexcept {
        for (auto& h : handlers) h->way_node_list(o);
    }
 
    virtual void relation_member_list (const osmium::RelationMemberList &o) const noexcept {
        for (auto& h : handlers) h->relation_member_list(o);
    }
 
    virtual void outer_ring (const osmium::OuterRing &o) const noexcept {
        for (auto& h : handlers) h->outer_ring(o);
    }
 
    virtual void inner_ring (const osmium::InnerRing &o) const noexcept {
        for (auto& h : handlers) h->inner_ring(o);
    }
 
    virtual void changeset_discussion (const osmium::ChangesetDiscussion &o) const noexcept {
        for (auto& h : handlers) h->changeset_discussion(o);
    }
 
    virtual void flush () const noexcept {
        for (auto& h : handlers) h->flush();
    }
private:
    std::vector<BaseHandler*> handlers;
};
}

OsmDrawer::OsmDrawer()
{}

void OsmDrawer::addHandler(BaseHandler* handler) 
{
    handlers.push_back(handler);
}

void OsmDrawer::dispatch(const std::string& filename) {
    ProxyHandler proxy(handlers);
    
    osmium::io::File infile(filename);

    osmium::area::Assembler::config_type assembler_config;
    osmium::area::MultipolygonCollector<osmium::area::Assembler> collector(assembler_config);

    std::cerr << "Pass 1...\n";
    osmium::io::Reader reader1(infile);
    collector.read_relations(reader1);
    reader1.close();
    std::cerr << "Pass 1 done\n";

    IndexPos indexPos;
    IndexNeg indexNeg;
    LocationHandler locationHandler(indexPos, indexNeg);
    locationHandler.ignore_errors();

    std::cerr << "Pass 2...\n";
    osmium::io::Reader reader2(infile);
    osmium::apply(reader2, locationHandler, proxy, collector.handler([&proxy](osmium::memory::Buffer&& buffer) {
        osmium::apply(buffer, proxy);
    }));
    reader2.close();
    std::cerr << "Pass 2 done\n";
}
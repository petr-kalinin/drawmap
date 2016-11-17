#pragma once

#include <osmium/handler.hpp>

class BaseHandler {
public:
    virtual void osm_object (const osmium::OSMObject &) {}
    virtual void node (const osmium::Node &) {}
    virtual void way (const osmium::Way &) {}
    virtual void relation (const osmium::Relation &) {}
    virtual void area (const osmium::Area &) {}
    virtual void changeset (const osmium::Changeset &) {}
    virtual void tag_list (const osmium::TagList &) {}
    virtual void way_node_list (const osmium::WayNodeList &) {}
    virtual void relation_member_list (const osmium::RelationMemberList &) {}
    virtual void outer_ring (const osmium::OuterRing &) {}
    virtual void inner_ring (const osmium::InnerRing &) {}
    virtual void changeset_discussion (const osmium::ChangesetDiscussion &) {}
    virtual void flush () {}
    virtual void finalize() {};
};

#include "experiments/csharp/ManagedGoreCatalog.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace bl::csharp::gore;

Source source(const char* name) {
    Source value;
    value.name = name;
    value.relativePath = std::string("Content/Gores/") + name + ".png";
    value.absolutePath = std::string("/tmp/mod/content/") + value.relativePath;
    value.width = 12;
    value.height = 12;
    value.byteCount = 128;
    return value;
}

int main() {
    Catalog catalog;
    std::string error;
    assert(catalog.stage("modA", {source("Head"), source("Leaf")}, &error));
    assert(catalog.stage("modB", {source("Head")}, &error));
    assert(catalog.entries().size() == 3);
    assert(catalog.typeOf("modA", "Head") == 0);
    assert(catalog.typeOf("modB", "Head") == 0);
    assert(!catalog.stage("modA", {source("Head")}, &error));
    assert(catalog.entries().size() == 3);
    assert(!catalog.allocate(1000, 1002, &error));
    assert(!catalog.allocated());
    assert(catalog.allocate(1000, 1003, &error));
    assert(catalog.typeOf("modA", "Head") == 1000);
    assert(catalog.typeOf("modA", "Leaf") == 1001);
    assert(catalog.typeOf("modB", "Head") == 1002);
    assert(catalog.typeOf("modB", "Leaf") == 0);
    assert(catalog.allocate(1000, 1003, &error));
    assert(!catalog.allocate(1500, 1503, &error));
    assert(!catalog.stage("modC", {source("Arm")}, &error));
    catalog.clear();
    assert(catalog.stage("modC", {source("Arm")}, &error));
    assert(catalog.allocate(200, 201, &error));
    assert(catalog.typeOf("modC", "Arm") == 200);
    std::puts("CSHARP_GORE_CATALOG_PASS");
}

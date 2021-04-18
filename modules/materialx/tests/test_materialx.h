#ifndef TEST_MATERIALX_H
#define TEST_MATERIALX_H

#include "tests/test_macros.h"

namespace TestMaterialX {

TEST_CASE("[MaterialX] Hello MaterialX!") {
    String hello = "Hello MaterialX!";
    CHECK(hello == "Hello MaterialX!");
}

} // namespace TestMaterialX

#endif // TEST_MATERIALX_H
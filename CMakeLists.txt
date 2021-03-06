cmake_minimum_required(VERSION 3.1)
project(Miyuki)

set(CMAKE_CXX_STANDARD 14)
link_libraries(embree3 boost_system boost_filesystem boost_unit_test_framework)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")
file(GLOB samplerSRC "src/samplers/*.h" "src/samplers/*.cc")
file(GLOB rendererSRC "src/renderer/*.h" "src/renderer/*.hpp")
file(GLOB lightSRC "src/lights/*.h" "src/lights/*.hpp" "src/lights/*.cc")
file(GLOB coreSRC "src/core/*.h" "src/core/*.hpp" "src/core/*.cc")
file(GLOB bsdfSRC "src/bsdfs/*.h" "src/bsdfs/*.hpp" "src/bsdfs/*.cc")
file(GLOB cameraSRC "src/cameras/*.h" "src/cameras/*.hpp" "src/cameras/*.cc")
file(GLOB mathSRC "src/math/*.h" "src/math/*.hpp" "src/math/*.cc")
file(GLOB utilSRC "src/utils/*.h" "src/utils/*.hpp" "src/utils/*.cc")
file(GLOB integratorSRC
        "src/integrators/*.h"
        "src/integrators/*.hpp"
        "src/integrators/*.cc"
        "src/integrators/ao/*.h"
        "src/integrators/ao/*.cc"
        "src/integrators/pathtracer/*.h"
        "src/integrators/pathtracer/*.cc"
        "src/integrators/pssmlt/*.h"
        "src/integrators/pssmlt/*.cc"
        "src/integrators/bdpt/*.h"
        "src/integrators/bdpt/*.cc")
file(GLOB thirdpartySRC
        "thirdparty/lodepng/lodepng.cpp"
        "thirdparty/tiny_obj_loader/tiny_obj_loader.cc"
        "thirdparty/rand48/rand48.c"
        "thirdparty/rapidjson/rapidjson.h"
        "thirdparty/fmt/format.cc"
        "thirdparty/fmt/posix/cc")
add_executable(Miyuki
        ${rendererSRC}
        ${lightSRC}
        ${coreSRC}
        ${bsdfSRC}
        ${cameraSRC}
        ${thirdpartySRC}
        ${mathSRC}
        ${integratorSRC}
        ${utilSRC}
        ${samplerSRC}
        src/renderer/miyuki.cc
        src/renderer/rendersystem.cc)
# ============== Spatial Index Library ==============
add_library(murim_spatial INTERFACE)

target_include_directories(murim_spatial INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(murim_spatial INTERFACE
    murim_core
)

target_compile_features(murim_spatial INTERFACE cxx_std_20)

# Source files for spatial library
set(SPATIAL_SOURCES
    QuadTree.cpp
    SpatialIndex.cpp
)

# Header files
set(SPATIAL_HEADERS
    QuadTree.hpp
    SpatialIndex.hpp
)

# Create a static library for spatial components
add_library(murim_spatial_lib STATIC ${SPATIAL_SOURCES})

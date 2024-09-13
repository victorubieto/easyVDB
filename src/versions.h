#pragma once

#include <vector>

namespace easyVDB
{

    const uint32_t OPENVDB_FILE_VERSION_ROOTNODE_MAP = 213;
    const uint32_t OPENVDB_FILE_VERSION_INTERNALNODE_COMPRESSION = 214;
    const uint32_t OPENVDB_FILE_VERSION_SIMPLIFIED_GRID_TYPENAME = 215;
    const uint32_t OPENVDB_FILE_VERSION_GRID_INSTANCING = 216;
    const uint32_t OPENVDB_FILE_VERSION_BOOL_LEAF_OPTIMIZATION = 217;
    const uint32_t OPENVDB_FILE_VERSION_BOOST_UUID = 218;
    const uint32_t OPENVDB_FILE_VERSION_NO_GRIDMAP = 219;
    const uint32_t OPENVDB_FILE_VERSION_NEW_TRANSFORM = 219;
    const uint32_t OPENVDB_FILE_VERSION_SELECTIVE_COMPRESSION = 220;
    const uint32_t OPENVDB_FILE_VERSION_FLOAT_FRUSTUM_BBOX = 221;
    const uint32_t OPENVDB_FILE_VERSION_NODE_MASK_COMPRESSION = 222;
    const uint32_t OPENVDB_FILE_VERSION_BLOSC_COMPRESSION = 223;
    const uint32_t OPENVDB_FILE_VERSION_POINT_INDEX_GRID = 223;
    const uint32_t OPENVDB_FILE_VERSION_MULTIPASS_IO = 224;

    const uint32_t OPENVDB_MIN_SUPPORTED_VERSION = OPENVDB_FILE_VERSION_ROOTNODE_MAP;

    // @internal Per-node indicator byte that specifies what additional metadata is stored to permit reconstruction of inactive values
    enum NodeMetaData {
        NoMaskOrInactiveVals,       /*0*/ // no inactive vals, or all inactive vals are +background
        NoMaskAndMinusBg,           /*1*/ // all inactive vals are -background
        NoMaskAndOneInactiveVal,    /*2*/ // all inactive vals have the same non-background val
        MaskAndNoInactiveVals,      /*3*/ // mask selects between -background and +background
        MaskAndOneInactiveVal,      /*4*/ // mask selects between backgd and one other inactive val
        MaskAndTwoInactiveVals,     /*5*/ // mask selects between two non-background inactive vals
        NoMaskAndAllVals,           /*6*/ // > 2 inactive vals, so no mask compression at all
    };

}
#pragma once

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
    enum NodeMetadata {
        EASYVDB_NO_MASK_OR_INACTIVE_VALS,       /*0*/ // no inactive vals, or all inactive vals are +background
        EASYVDB_NO_MASK_AND_MINUS_BG,           /*1*/ // all inactive vals are -background
        EASYVDB_NO_MASK_AND_ONE_INACTIVE_VAL,   /*2*/ // all inactive vals have the same non-background val
        EASYVDB_MASK_AND_NO_INACTIVE_VALS,      /*3*/ // mask selects between -background and +background
        EASYVDB_MASK_AND_ONE_INACTIVE_VAL,      /*4*/ // mask selects between backgd and one other inactive val
        EASYVDB_MASK_AND_TWO_INACTIVE_VALS,     /*5*/ // mask selects between two non-background inactive vals
        EASYVDB_NO_MASK_AND_ALL_VALS,           /*6*/ // > 2 inactive vals, so no mask compression at all
    };

    enum ErrorCode {
        EASYVDB_SUCCESS,
        EASYVDB_UNSUPPORTED_VERSION,
        EASYVDB_UNKNOWN_VALUE_TYPE,
        EASYVDB_UNKNOWN_COMPRESSION,
        EASYVDB_COMPRESSION_ERROR_ZLIB,
        EASYVDB_COMPRESSION_ERROR_BLOSC,
    };

}
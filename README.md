# easyVDB
Light-weight library to import/export .vdb files with easy integration in other projects using submodules via cmake.

This repository was created using [@mjurczyk](https://github.com/mjurczyk) [repository](https://github.com/mjurczyk/openvdb) as reference, which at the same time is based on original [OpenVDB](https://github.com/AcademySoftwareFoundation/openvdb/tree/master/openvdb/openvdb). He adapted the code with JavaScript's limitations in mind, threfore my C++ implementation is not currently optimal. However, it remains easy and fast to integrate into other projects.

## Usage
I highly recommend including this library as a submodule in your projects by doing this:

```bash
# In the path of the project where you want to store the external libraries
git submodule add https://github.com/victorubieto/easyVDB

# Then init and update all the submodules of easyVDB
git submodule update --init --recursive
```

Once the library is added as a submodule, you can include it in your project's cmake like this (replace `LIBRARIES_PATH`):

```bash
# easyVDB
add_subdirectory(LIBRARIES_PATH/easyVDB)
target_link_libraries(${PROJECT_NAME} PUBLIC easyVDB)
set_property(TARGET easyVDB PROPERTY FOLDER "External/easyVDB")
```

## Functionalities
[TODO]

## Limitations
OpenVDB has several versions, which means that maybe not all of your vdb files will be parsed correctly. By now, the supported versions are:
- [TODO]

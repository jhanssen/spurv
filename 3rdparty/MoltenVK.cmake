include(ExternalProject)

add_library(MoltenVK SHARED IMPORTED GLOBAL)
set(MOLTENVK_DIR "${THIRDPARTY_DIR}/MoltenVK/Package/Release/MoltenVK/dynamic/dylib/macOS/")
target_link_libraries(MoltenVK INTERFACE MoltenVK)
target_link_directories(MoltenVK INTERFACE ${MOLTENVK_DIR})
set_property(TARGET MoltenVK PROPERTY IMPORTED_LOCATION "${MOLTENVK_DIR}/libMoltenVK.dylib")

ExternalProject_Add(MoltenVKLib
    SOURCE_DIR ${THIRDPARTY_DIR}/MoltenVK
    CONFIGURE_COMMAND ./fetchDependencies --macos
    BUILD_COMMAND make macos
    BUILD_IN_SOURCE TRUE
    INSTALL_COMMAND ""
    )

add_dependencies(MoltenVK MoltenVKLib)

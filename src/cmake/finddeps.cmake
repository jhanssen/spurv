find_package(PkgConfig)

if (NOT PKG_CONFIG_FOUND)
    message(FATAL_ERROR "No pkgconfig")
endif()

macro(definelib pkgname name)
    if (${${pkgname}_FOUND})
        if (NOT TARGET ${name})
            add_library(${name} INTERFACE IMPORTED)
            target_include_directories(${name} INTERFACE ${${pkgname}_INCLUDE_DIRS})
            target_link_libraries(${name} INTERFACE ${${pkgname}_LIBRARIES})
            target_link_directories(${name} INTERFACE ${${pkgname}_LIBRARY_DIRS})
        endif()
    endif()
endmacro()

pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
definelib(FONTCONFIG FontConfig::FontConfig)

pkg_check_modules(LIBFMT REQUIRED fmt)
definelib(LIBFMT Fmt::Fmt)

pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)
definelib(HARFBUZZ Harfbuzz::Harfbuzz)

if (APPLE)
    # attempt to use moltenvk from homebrew
    set(MOLTENVK_DIR "/opt/homebrew/opt/molten-vk" CACHE STRING "MoltenVK directory")

    if (EXISTS "${MOLTENVK_DIR}/lib/libMoltenVK.dylib")
        set(Vulkan_FOUND 1)
        set(Vulkan_INCLUDE_DIRS "${THIRDPARTY_DIR}/Vulkan-Headers/include")
        set(Vulkan_LIBRARIES MoltenVK)
        set(Vulkan_LIBRARY_DIRS "${MOLTENVK_DIR}/lib")
        message("-- Found MoltenVK")
    else()
        message(FATAL_ERROR "MoltenVK not found, looked in ${MOLTENVK_DIR}\nset MOLTENVK_DIR as an option if this resides elsewhere")
    endif()
else()
    find_package(Vulkan REQUIRED)
    if (Vulkan_FOUND)
        # always use our Vulkan-Headers
        set(Vulkan_INCLUDE_DIRS "${THIRDPARTY_DIR}/Vulkan-Headers/include")
    endif()
endif()
definelib(Vulkan Vulkan::Vulkan)

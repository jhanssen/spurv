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

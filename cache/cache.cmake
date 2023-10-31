set(${PROJECT_NAME}_m_evacu ${m})
set(m ${PROJECT_NAME})
list(APPEND ${m}_unsetter )

set(${m}_kautil_cmake_heeder CMakeKautilHeader.v0.0.cmake)
if(DEFINED KAUTIL_THIRD_PARTY_DIR AND EXISTS "${KAUTIL_THIRD_PARTY_DIR}/${${m}_kautil_cmake_heeder}")
    include("${KAUTIL_THIRD_PARTY_DIR}/${${m}_kautil_cmake_heeder}")
else()
    if(NOT EXISTS ${CMAKE_BINARY_DIR}/${${m}_kautil_cmake_heeder})
        file(DOWNLOAD https://raw.githubusercontent.com/kautils/CMakeKautilHeader/v0.0/CMakeKautilHeader.cmake ${CMAKE_BINARY_DIR}/${${m}_kautil_cmake_heeder})
    endif()
    include(${CMAKE_BINARY_DIR}/${${m}_kautil_cmake_heeder})
endif()


git_clone(https://raw.githubusercontent.com/kautils/CMakeLibrarytemplate/v0.0.1/CMakeLibrarytemplate.cmake)
git_clone(https://raw.githubusercontent.com/kautils/CMakeFetchKautilModule/v1.0.1/CMakeFetchKautilModule.cmake)

list(APPEND ${m}_unsetter ${m}_kautil_gap_v ${m}_kautil_merge_v ${m}_kautil_exists_v)
set(${m}_kautil_gap_v v0.0)
set(${m}_kautil_merge_v v0.0)
set(${m}_kautil_exists_v v0.0)
CMakeFetchKautilModule(${m}_kautil_gap  GIT https://github.com/kautils/range.gap.git REMOTE origin BRANCH ${${m}_kautil_gap_v} PROJECT_SUFFIX ${${m}_kautil_gap_v})
CMakeFetchKautilModule(${m}_kautil_merge GIT https://github.com/kautils/range.merge.git REMOTE origin BRANCH ${${m}_kautil_merge_v} PROJECT_SUFFIX ${${m}_kautil_merge_v}) 
CMakeFetchKautilModule(${m}_kautil_exists GIT https://github.com/kautils/range.exists.git REMOTE origin BRANCH ${${m}_kautil_exists_v} PROJECT_SUFFIX ${${m}_kautil_exists_v})

list(APPEND ${m}_unsetter  ${m}_cache_hpp)
file(GLOB ${m}_cache_hpp ${CMAKE_CURRENT_LIST_DIR}/*.hpp)
install(FILES ${${m}_cache_hpp} DESTINATION include/kautil/cache )
install(SCRIPT "${${${m}_kautil_merge.STRUCT_ID}.BUILD_DIR}/cmake_install.cmake")
install(SCRIPT "${${${m}_kautil_exists.STRUCT_ID}.BUILD_DIR}/cmake_install.cmake")
install(SCRIPT "${${${m}_kautil_gap.STRUCT_ID}.BUILD_DIR}/cmake_install.cmake")

set(${m}_set_module_range_gap KautilRangeGap.${${m}_kautil_gap_v})
set(${m}_set_module_range_exists KautilRangeExists.${${m}_kautil_exists_v})
set(${m}_set_module_range_merge KautilRangeMerge.${${m}_kautil_merge_v})

find_package(${${m}_set_module_range_merge}.interface REQUIRED)
find_package(${${m}_set_module_range_gap}.interface REQUIRED)
find_package(${${m}_set_module_range_exists}.interface REQUIRED)

foreach(export_mod
        ${${m}_set_module_range_merge}
        ${${m}_set_module_range_exists}
        ${${m}_set_module_range_gap})

    string(APPEND ${m}_findpkgs
        "if(EXISTS \"\\$\\{PACKAGE_PREFIX_DIR}/lib/cmake/${export_mod}\")\n"
        "\t set(${export_mod}.interface_DIR \"\\$\\{PACKAGE_PREFIX_DIR}/lib/cmake/${export_mod}\")\n"
        "\t find_package(${export_mod}.interface REQUIRED)\n"
        "endif()\n"
        "\n"
    )
endforeach()


set(module_name cache)
unset(srcs)
set(${module_name}_common_pref
    MODULE_PREFIX kautil 
    MODULE_NAME ${module_name}
    INCLUDES $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}> $<INSTALL_INTERFACE:include>  
    EXPORT_NAME_PREFIX ${PROJECT_NAME}
    EXPORT_NAME_SUFFIX ${KAUTIL_PROJECT_SUFFIX}
    EXPORT_RENAME ${KAUTIL_PROJECT_RENAME}
    EXPORT_VERSION ${PROJECT_VERSION}
    EXPORT_VERSION_COMPATIBILITY AnyNewerVersion
    LINK_LIBS 
        kautil::range::merge::${${m}_kautil_merge_v}::interface
        kautil::range::exists::${${m}_kautil_exists_v}::interface
        kautil::range::gap::${${m}_kautil_gap_v}::interface

    DESTINATION_INCLUDE_DIR include/kautil/cache
    DESTINATION_CMAKE_DIR cmake
    DESTINATION_LIB_DIR lib
)

CMakeLibraryTemplate(${module_name} EXPORT_LIB_TYPE interface ${${module_name}_common_pref} )
#CMakeLibraryTemplate(${module_name} EXPORT_LIB_TYPE shared ${${module_name}_common_pref} )


set(BUILD_TEST ON)

if(${BUILD_TEST})
set(__t ${${module_name}_interface_tmain})
add_executable(${__t})
target_sources(${__t} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/unit_test.cc)
target_link_libraries(${__t} PRIVATE ${${module_name}_interface})
target_compile_definitions(${__t} PRIVATE ${${module_name}_interface_tmain_ppcs})
endif()

foreach(__v ${${m}_unsetter})
    unset(${__v})
endforeach()
unset(${m}_unsetter)
set(m ${${PROJECT_NAME}_m_evacu})

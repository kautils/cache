set(${PROJECT_NAME}_m_evacu ${m})
set(m ${PROJECT_NAME})
list(APPEND ${m}_unsetter )

if(NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeKautilHeader.v0.0.cmake)
    file(DOWNLOAD https://raw.githubusercontent.com/kautils/CMakeKautilHeader/v0.0/CMakeKautilHeader.cmake ${CMAKE_BINARY_DIR}/CMakeKautilHeader.v0.0.cmake)
endif()

include(${CMAKE_BINARY_DIR}/CMakeKautilHeader.cmake)
git_clone(https://raw.githubusercontent.com/kautils/CMakeLibrarytemplate/v0.0.1/CMakeLibrarytemplate.cmake)
git_clone(https://raw.githubusercontent.com/kautils/CMakeFetchKautilModule/v1.0.1/CMakeFetchKautilModule.cmake)

CMakeFetchKautilModule(${m}_kautil_gap   GIT https://github.com/kautils/range.gap.git REMOTE origin BRANCH v0.0)
CMakeFetchKautilModule(${m}_kautil_merge GIT https://github.com/kautils/range.merge.git REMOTE origin BRANCH v0.0)
CMakeFetchKautilModule(${m}_kautil_exists GIT https://github.com/kautils/range.exists.git REMOTE origin BRANCH v0.0)

list(APPEND ${m}_unsetter  ${m}_cache_hpp)
file(GLOB ${m}_cache_hpp ${CMAKE_CURRENT_LIST_DIR}/*.hpp)
install(FILES ${${m}_gap_hpp} DESTINATION include/kautil/cache )
install(SCRIPT "${${${m}_kautil_merge.STRUCT_ID}.BUILD_DIR}/cmake_install.cmake")
install(SCRIPT "${${${m}_kautil_exists.STRUCT_ID}.BUILD_DIR}/cmake_install.cmake")
install(SCRIPT "${${${m}_kautil_gap.STRUCT_ID}.BUILD_DIR}/cmake_install.cmake")


set(${m}_set_module_rangee_merge KautilRangeMerge.0.0.1)
set(${m}_set_module_rangee_exists KautilRangeExists.0.0.1)
set(${m}_set_module_rangee_gap KautilRangeGap.0.0.1)

find_package(${${m}_set_module_rangee_merge}.interface REQUIRED)
find_package(${${m}_set_module_rangee_exists}.interface REQUIRED)
find_package(${${m}_set_module_rangee_gap}.interface REQUIRED)

foreach(export_mod
        ${${m}_set_module_rangee_merge}
        ${${m}_set_module_rangee_exists}
        ${${m}_set_module_rangee_gap}
)
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
    EXPORT_VERSION ${PROJECT_VERSION}
    EXPORT_VERSION_COMPATIBILITY AnyNewerVersion
    LINK_LIBS 
        kautil::range::merge::0.0.1::interface
        kautil::range::exists::0.0.1::interface
        kautil::range::gap::0.0.1::interface
        
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

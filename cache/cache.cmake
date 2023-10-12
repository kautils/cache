set(${PROJECT_NAME}_m_evacu ${m})
set(m ${PROJECT_NAME})
list(APPEND ${m}_unsetter )




if(NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeKautilHeader.cmake)
    file(DOWNLOAD https://raw.githubusercontent.com/kautils/CMakeKautilHeader/v0.0.1/CMakeKautilHeader.cmake ${CMAKE_BINARY_DIR}/CMakeKautilHeader.cmake)
endif()
include(${CMAKE_BINARY_DIR}/CMakeKautilHeader.cmake)
git_clone(https://raw.githubusercontent.com/kautils/CMakeLibrarytemplate/v0.0.1/CMakeLibrarytemplate.cmake)
git_clone(https://raw.githubusercontent.com/kautils/CMakeFetchKautilModule/v0.0.1/CMakeFetchKautilModule.cmake)

CMakeFetchKautilModule(${m}_kautil_region GIT https://github.com/kautils/region.git       REMOTE origin BRANCH v0.0)
CMakeFetchKautilModule(${m}_kautil_btree GIT https://github.com/kautils/btree_search.git REMOTE origin BRANCH v1.0)

list(APPEND ${m}_unsetter ${m}_btree_hpp ${m}_region_hpp)
file(GLOB ${m}_btree_hpp ${${m}_kautil_btree.INSTALL_PREFIX}/include/kautil/algorithm/btree_search/*.hpp)
file(GLOB ${m}_region_hpp ${${m}_kautil_region.INSTALL_PREFIX}/include/kautil/region/*.hpp)

file(GLOB ${m}_cache_hpp ${CMAKE_CURRENT_LIST_DIR}/*.hpp)
install(FILES ${${m}_btree_hpp} DESTINATION include/kautil/btree_search )
install(FILES ${${m}_region_hpp} DESTINATION include/kautil/region )
install(FILES ${${m}_cache_hpp} DESTINATION include/kautil/cache )



find_package(KautilRegion.0.0.1.interface REQUIRED)
find_package(KautilAlgorithmBtreeSearch.1.0.1.interface REQUIRED)

string(APPEND ${m}_findpkgs
    "if(EXISTS \"\\$\\{PACKAGE_PREFIX_DIR}/lib/cmake/KautilRegion.0.0.1\")\n"
    "\t set(KautilRegion.0.0.1.interface_DIR \"\\$\\{PACKAGE_PREFIX_DIR}/lib/cmake/KautilRegion.0.0.1\")\n"
    "\t find_package(KautilRegion.0.0.1.interface REQUIRED)\n"
    "endif()\n"
        
    "if(EXISTS \"\\$\\{PACKAGE_PREFIX_DIR}/lib/cmake/KautilAlgorithmBtreeSearch.1.0.1\")\n"
    "\t set(KautilAlgorithmBtreeSearch.1.0.1_DIR \"\\$\\{PACKAGE_PREFIX_DIR}/lib/cmake/KautilAlgorithmBtreeSearch.1.0.1\")\n"
    "\t find_package(KautilAlgorithmBtreeSearch.1.0.1.interface REQUIRED)\n"
    "endif()\n"
)

set(module_name cache)
unset(srcs)
file(GLOB srcs ${CMAKE_CURRENT_LIST_DIR}/*.cc)
set(${module_name}_common_pref
    MODULE_PREFIX kautil
    MODULE_NAME ${module_name}
    INCLUDES $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}> $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include> $<INSTALL_INTERFACE:include> 
    SOURCES ${srcs}
    LINK_LIBS 
        kautil::region::0.0.1::interface 
        kautil::algorithm::btree_search::1.0.1::interface
    EXPORT_NAME_PREFIX ${PROJECT_NAME}
    EXPORT_VERSION ${PROJECT_VERSION}
    EXPORT_VERSION_COMPATIBILITY AnyNewerVersion
        
    DESTINATION_INCLUDE_DIR include/kautil/cache
    DESTINATION_CMAKE_DIR cmake
    DESTINATION_LIB_DIR lib
)

 list(APPEND ${module_name}_common_pref EXPORT_CONFIG_IN_ADDITIONAL_CONTENT_BEFORE ${${m}_findpkgs})



CMakeLibraryTemplate(${module_name} EXPORT_LIB_TYPE interface ${${module_name}_common_pref} )

if(NOT EXISTS ${CMAKE_CURRENT_LIST_DIR}/include/kautil)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/kautil)
    file(CREATE_LINK ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/include/kautil/cache SYMBOLIC)
endif()
    

set(__t ${${module_name}_interface_tmain})
add_executable(${__t})
target_sources(${__t} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/unit_test.cc)
target_link_libraries(${__t} PRIVATE ${${module_name}_interface})
target_compile_definitions(${__t} PRIVATE ${${module_name}_interface_tmain_ppcs})


# process

foreach(__v ${${m}_unsetter})
    unset(${__v})
endforeach()
unset(${m}_unsetter)
set(m ${${PROJECT_NAME}_m_evacu})

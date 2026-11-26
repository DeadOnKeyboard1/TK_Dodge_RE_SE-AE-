cmake_minimum_required(VERSION 3.25)
cmake_policy(SET CMP0141 NEW)
set(TKRE_PATHMAP_ROOT "X:/workspace" CACHE STRING "Neutral deterministic debug source prefix")
if(WIN32)
	set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
	set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
endif()

# standards & flags
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
	set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "")
endif()

# info
project(
	TK_Dodge_RE
	VERSION 0.4.10.2
	LANGUAGES CXX
)
include(GNUInstallDirs)

# out-of-source builds only
if(${PROJECT_SOURCE_DIR} STREQUAL ${PROJECT_BINARY_DIR})
	message(FATAL_ERROR "In-source builds are not allowed.")
endif()

# in-place configuration
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.h.in
	${CMAKE_CURRENT_BINARY_DIR}/include/Version.h
	@ONLY
)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in
	${CMAKE_CURRENT_BINARY_DIR}/version.rc
	@ONLY
)

# source files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp" "src/*.h")
source_group(
	TREE 
		${CMAKE_CURRENT_SOURCE_DIR}
	FILES 
		${SOURCES}
)

source_group(
	TREE ${CMAKE_CURRENT_BINARY_DIR}
	FILES ${CMAKE_CURRENT_BINARY_DIR}/include/Version.h
)

# dependencies
set(_tkre_toolchain "${CMAKE_CURRENT_SOURCE_DIR}/../_toolchain")
if(DEFINED CommonLibSSEPath AND EXISTS "${CommonLibSSEPath}/CMakeLists.txt")
elseif(DEFINED ENV{SKSE_COMMONLIB_SOURCE_ROOT} AND EXISTS "$ENV{SKSE_COMMONLIB_SOURCE_ROOT}/CMakeLists.txt")
	set(CommonLibSSEPath "$ENV{SKSE_COMMONLIB_SOURCE_ROOT}" CACHE PATH "Path to CommonLibSSE-NG")
elseif(DEFINED ENV{CommonLibSSEPath} AND EXISTS "$ENV{CommonLibSSEPath}/CMakeLists.txt")
	set(CommonLibSSEPath "$ENV{CommonLibSSEPath}" CACHE PATH "Path to CommonLibSSE-NG")
else()
	set(CommonLibSSEPath "${_tkre_toolchain}/CommonLibSSE-NG-v12.1.0" CACHE PATH "Path to CommonLibSSE-NG")
endif()
if(NOT EXISTS "${CommonLibSSEPath}/CMakeLists.txt")
	message(FATAL_ERROR "CommonLibSSE-NG not found; pass -DCommonLibSSEPath=<path>")
endif()

if(NOT DEFINED SMF_API_INCLUDE OR NOT EXISTS "${SMF_API_INCLUDE}/SKSEMenuFramework.h")
	set(SMF_API_INCLUDE "${_tkre_toolchain}/SKSE-Menu-Framework-3-API" CACHE PATH "Path to SKSE Menu Framework 3 API")
endif()

if(EXISTS "${_tkre_toolchain}/vcpkg-installed/x64-windows-static-md")
	list(PREPEND CMAKE_PREFIX_PATH "${_tkre_toolchain}/vcpkg-installed/x64-windows-static-md")
endif()
if(EXISTS "${_tkre_toolchain}/skyrim_1_7_104_compat/deps-msvc1951")
	list(PREPEND CMAKE_PREFIX_PATH "${_tkre_toolchain}/skyrim_1_7_104_compat/deps-msvc1951")
endif()
if(EXISTS "${_tkre_toolchain}/minhook-v1.3.4/CMakeLists.txt")
	set(FETCHCONTENT_SOURCE_DIR_HDE64 "${_tkre_toolchain}/minhook-v1.3.4" CACHE PATH "Cached HDE64 sources")
endif()

set(ENABLE_SKYRIM_SE ON CACHE BOOL "Enable Skyrim SE support" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "Enable Skyrim AE support" FORCE)
set(ENABLE_SKYRIM_VR ON CACHE BOOL "Preserve original VR preset" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(COMMONLIB_PREBUILT OFF CACHE BOOL "" FORCE)
set(COMMONLIB_PREBUILT_DIR "" CACHE PATH "" FORCE)
set(COMMONLIB_ENABLE_IPO OFF CACHE BOOL "" FORCE)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
if(MSVC)
	add_compile_options("/experimental:deterministic")
	if(DEFINED ENV{USERPROFILE})
		file(TO_NATIVE_PATH "$ENV{USERPROFILE}" privacy_prefix)
		add_compile_options("/pathmap:${privacy_prefix}=X:/user")
	endif()
	file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" src_prefix)
	add_compile_options("/pathmap:${src_prefix}=${TKRE_PATHMAP_ROOT}/source")
	file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}" bin_prefix)
	add_compile_options("/pathmap:${bin_prefix}=${TKRE_PATHMAP_ROOT}/build")
	file(TO_NATIVE_PATH "${CommonLibSSEPath}" commonlib_prefix)
	add_compile_options("/pathmap:${commonlib_prefix}=${TKRE_PATHMAP_ROOT}/deps/CommonLibSSE-NG")
	if(EXISTS "${SMF_API_INCLUDE}")
		file(TO_NATIVE_PATH "${SMF_API_INCLUDE}" smf_prefix)
		add_compile_options("/pathmap:${smf_prefix}=${TKRE_PATHMAP_ROOT}/deps/SKSE-Menu-Framework-3-API")
	endif()
endif()
add_subdirectory("${CommonLibSSEPath}" CommonLibSSE EXCLUDE_FROM_ALL)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
find_package(spdlog CONFIG REQUIRED)
if(NOT DEFINED SIMPLEINI_INCLUDE_DIR AND EXISTS "${_tkre_toolchain}/skyrim_1_7_104_compat/deps-msvc1951" AND EXISTS "${_tkre_toolchain}/vcpkg/packages/simpleini_x64-windows-static-md/include/SimpleIni.h")
	set(SIMPLEINI_INCLUDE_DIR "${_tkre_toolchain}/vcpkg/packages/simpleini_x64-windows-static-md/include" CACHE PATH "Path to SimpleIni.h")
endif()
find_path(
	SIMPLEINI_INCLUDE_DIR
	NAMES SimpleIni.h
	HINTS
		"${_tkre_toolchain}/vcpkg/packages/simpleini_x64-windows-static-md/include"
		"${_tkre_toolchain}/vcpkg-installed/x64-windows-static-md/include"
)
if(NOT SIMPLEINI_INCLUDE_DIR)
	message(FATAL_ERROR "SimpleIni.h not found; pass -DSIMPLEINI_INCLUDE_DIR=<path> or add its prefix to CMAKE_PREFIX_PATH")
endif()

# cmake target
# runtime
add_library(
	${PROJECT_NAME}
	SHARED
		${SOURCES}
		${CMAKE_CURRENT_BINARY_DIR}/include/Version.h
		${CMAKE_CURRENT_BINARY_DIR}/version.rc
		.clang-format
		vcpkg.json
)

# include dir
target_include_directories(
	${PROJECT_NAME}
	PRIVATE
		${CMAKE_CURRENT_BINARY_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}/src
		${SMF_API_INCLUDE}
		${SIMPLEINI_INCLUDE_DIR}
)

# linkage
target_link_libraries(
	${PROJECT_NAME} 
	PRIVATE
		CommonLibSSE::CommonLibSSE
		spdlog::spdlog
)

# compiler def
if (MSVC)
	add_compile_definitions(_UNICODE NOMINMAX)
	# Keep the dynamic menu API wrappers out of link-time inlining.
	set_property(SOURCE src/Menu.cpp APPEND PROPERTY COMPILE_OPTIONS /GL-)

	target_compile_options(
		${PROJECT_NAME}
		PRIVATE
			/MP
			/W3
			/permissive-
			/Zc:alignedNew
			/Zc:auto
			/Zc:__cplusplus
			/Zc:externC
			/Zc:externConstexpr
			/Zc:forScope
			/Zc:hiddenFriend
			/Zc:implicitNoexcept
			/Zc:lambda
			/Zc:noexceptTypes
			/Zc:preprocessor
			/Zc:referenceBinding
			/Zc:rvalueCast
			/Zc:sizedDealloc
			/Zc:strictStrings
			/Zc:ternary
			/Zc:threadSafeInit
			/Zc:trigraphs
			/Zc:wchar_t
			/wd4200 # nonstandard extension used : zero-sized array in struct/union
			"$<$<CONFIG:Release>:/Z7;/O2>"
	)
	target_link_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:Release>:/DEBUG:FULL;/INCREMENTAL:NO;/PDBALTPATH:%_PDB%;/Brepro>")
endif()

include(CTest)
if(BUILD_TESTING)
	add_executable(tkre_logic_tests tests/LogicTests.cpp src/SettingsStore.cpp)
	target_compile_features(tkre_logic_tests PRIVATE cxx_std_23)
	target_include_directories(tkre_logic_tests PRIVATE src "${SIMPLEINI_INCLUDE_DIR}")
	target_link_libraries(tkre_logic_tests PRIVATE spdlog::spdlog)
	add_test(NAME TKRELogicTests COMMAND tkre_logic_tests)
endif()

# PCH
target_precompile_headers(
	${PROJECT_NAME}
	PRIVATE
		src/PCH.h
)

project(vcdextract)

include(CheckFunctionExists)
include(CheckIncludeFile)

# disable strdup warning in MSVC
if (MSVC)
	add_definitions(/wd4996)
endif ()

include(CheckCSourceCompiles)

add_definitions(-DPACKAGE=\"${VCDEXTRACT_PACKAGE}\")
add_definitions(-DVERSION=\"${VCDEXTRACT_VERSION}\")

# _wfopen
check_function_exists(_wfopen WFOPEN_OK)
if (WFOPEN_OK)
	add_definitions(-DHAVE_WFOPEN=1)
endif ()

# stricmp/strcasecmp
check_function_exists(strcasecmp STRCASECMP_OK)
if (STRCASECMP_OK)
	add_definitions(-DHAVE_STRCASECMP=1)
endif ()
check_function_exists(stricmp STRICMP_OK)
if (STRICMP_OK)
	add_definitions(-DHAVE_STRICMP=1)
endif ()

# pure C headers
set( vcdextract_HEADERS
	DBClass.h
	FileListClass.h
	iso.h
	ISOExtract.h )

# C sources
set( vcdextract_SOURCES
	DBClass.cpp
	FileListClass.cpp
	ISOExtract.cpp
	main.cpp )

add_executable( vcdextract WIN32
	${vcdextract_HEADERS}
	${vcdextract_SOURCES} )

if(WIN32)
   set_target_properties(vcdextract PROPERTIES COMPILE_DEFINITIONS_DEBUG "_CONSOLE")
   set_target_properties(vcdextract PROPERTIES COMPILE_DEFINITIONS_MINSIZEREL "_CONSOLE")
   set_target_properties(vcdextract PROPERTIES COMPILE_DEFINITIONS_RELEASE "_CONSOLE")
   set_target_properties(vcdextract PROPERTIES COMPILE_DEFINITIONS_RELWITHDEBINFO "_CONSOLE")
   set_target_properties(vcdextract PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE")
   set_target_properties(vcdextract PROPERTIES LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:CONSOLE")
   set_target_properties(vcdextract PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:CONSOLE")
   set_target_properties(vcdextract PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:CONSOLE")
endif(WIN32)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR} )

target_link_libraries( vcdextract ${VCDEXTRACT_LIBRARIES} )

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)


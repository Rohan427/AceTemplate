cmake_minimum_required (VERSION 3.16)

project (ACETemplateApp VERSION 1.0 LANGUAGES CXX C)

# ==================== Compiler & Standard Detection ====================
message(STATUS "CXX compiler: ${CMAKE_CXX_COMPILER}")

if(CMAKE_CXX_COMPILER MATCHES "amdclang" OR CMAKE_CXX_COMPILER MATCHES "rocm")
    message(STATUS "ROCm amdclang detected - forcing C++17")
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    
    # Force the flag explicitly for amdclang (CMake sometimes needs help)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
    add_compile_options(-std=c++17)
else()
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

# Legacy RHEL 6 fallback (gcc 4.4.7)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.7")
    message(WARNING "Legacy GCC 4.4.7 detected - falling back to C++98 mode")
    set(CMAKE_CXX_STANDARD 98)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
    add_definitions(-DLEGACY_GCC_44)
endif()

################## Find ACE (and TAO if we need it) ##################
# ACE/TAO typically don't ship with CMake 'config' files. 
# We manually find headers and libraries based on ACE_ROOT/TAO_ROOT.

#    find_path (TAO_INCLUDE_DIR tao/ORB.h PATHS "$ENV{TAO_ROOT}" NO_DEFAULT_PATH)
#    find_library (TAO_LIBRARY NAMES TAO PATHS "$ENV{TAO_ROOT}/lib" NO_DEFAULT_PATH)

find_path (ACE_INCLUDE_DIR ace/ACE.h PATHS "$ENV{ACE_ROOT}/include" NO_DEFAULT_PATH)
find_library (ACE_LIBRARY NAMES libACE.so PATHS "$ENV{ACE_ROOT}/lib" NO_DEFAULT_PATH)

#    find_path (TAO_INCLUDE_DIR tao/ORB.h PATHS ${TAO_ROOT} NO_DEFAULT_PATH)
#    find_library (TAO_LIBRARY NAMES TAO PATHS ${TAO_ROOT}/lib NO_DEFAULT_PATH)


if (NOT ACE_INCLUDE_DIR) # OR NOT TAO_LIBRARY)
    message (FATAL_ERROR "ACE include not found. Please set ACE_ROOT environment variable.")
endif()

#if (NOT TAO_INCLUDE_DIR) # OR NOT TAO_LIBRARY)
#    message (FATAL_ERROR "TAO include not found. Please set TAO_ROOT environment variable.")
#endif()

if (NOT ACE_LIBRARY) # OR NOT TAO_LIBRARY)
    message (FATAL_ERROR "ACE library not found. Please set ACE_ROOT environment variable.")
endif()

# Legacy ACE compatibility (older versions on RHEL 6)
if(GCC_VERSION VERSION_LESS "4.7")
    add_definitions(-DACE_LACKS_CXX11)
endif()

# ==================== ACE Compile Definitions ====================
add_definitions (
    -DACE_HAS_THREADS
    -DACE_MT_SAFE=1
    -DACE_NTRACE=0          # Enable logging
)


# ==================== Build Settings ====================
set (CMAKE_CXX_STANDARD 98)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

set (SOURCE_ROOT ${CMAKE_SOURCE_DIR}/src)
set (HEADER_ROOT ${CMAKE_SOURCE_DIR}/include)

# ==================== Source Files ====================
set (SOURCES
     ${SOURCE_ROOT}/Config.cxx
     ${SOURCE_ROOT}/DataProcessor.cxx
     ${SOURCE_ROOT}/ConfigurableTask.cxx     # NEW
     ${SOURCE_ROOT}/TaskOrchestrator.cxx     # NEW
     ${SOURCE_ROOT}/Main.cxx
     # Add your derived task .cxx files here
)

set (HEADERS
    ${HEADER_ROOT}/Compatibility.hxx
    ${HEADER_ROOT}/TestTasks.hxx
    ${HEADER_ROOT}/DataProcessor.hxx
    ${HEADER_ROOT}/ConfigurableTask.hxx
    ${HEADER_ROOT}/TaskOrchestrator.hxx
    ${HEADER_ROOT}/ObjectPool.hxx
    ${HEADER_ROOT}/ObjectData.hxx
    ${HEADER_ROOT}/SystemCapabilities.hxx
    ${HEADER_ROOT}/Config.hxx
    # Add your derived task headers
)

# ==================== Output Directories ====================
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

add_executable (ACETemplateApp ${SOURCES} ${HEADERS})

# ==================== Includes & Linking ====================
target_include_directories (ACETemplateApp PRIVATE
    ${HEADER_ROOT}
    ${ACE_INCLUDE_DIR}
    $ENV{XTRA_INCLUDE}/rl/
)

target_link_libraries (ACETemplateApp PRIVATE
    ${ACE_LIBRARY}
    ${CMAKE_DL_LIBS}
    "$ENV{XTRA_LIB}/rel/rl/librllibconfig.so"
)

# Set standard per-platform (avoid hard C++17 on legacy gcc)
if (NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU" OR NOT GCC_VERSION VERSION_LESS "4.7")
    set_target_properties (ACETemplateApp PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
else()
    set_target_properties (ACETemplateApp PROPERTIES
        CXX_STANDARD 98
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )
endif()

# Optional: Enable warnings
target_compile_options (ACETemplateApp PRIVATE ${COMPATIBILITY_FLAGS})

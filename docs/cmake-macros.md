# CMake Build Macros

CMake macros for building protoflow applications and libraries with integrated Debian packaging.

---

## Application Macro

### protoflow_add_app

Creates a protoflow application with all necessary configuration and Debian package generation.

```cmake
protoflow_add_app(
    NAME my_app
    VERSION 1.0.0
    DESCRIPTION "My protoflow application"
    
    # Source files
    SOURCES
        src/main.cpp
        src/app_logic.cpp
    
    # Hardware requirements (declaration only)
    # Apps have NO direct hardware access
    # Main app uses this for validation and arbitration
    HW_REQUIREMENTS
        /dev/ttyUSB0    # Serial port
        /dev/i2c-1      # I2C bus
        gpio-23         # GPIO pin
    
    # Security
    PKI_PATH /etc/protoflow/pki/${NAME}
    
    # Runtime user
    USER protoflow-${NAME}
    
    # Dependencies
    LINK_LIBRARIES
        protoflow::runtime
        protoflow::messaging
        protoflow::rpc
        protoflow::transport-tcp
        my_custom_lib
    
    # Optional
    DEPENDS_APPS
        main-app>=1.0.0
    
    # Logging configuration (optional)
    LOGGING_CONFIG
        apps/${NAME}/logging.ini
    
    # Debian packaging
    DEBIAN_DEPENDS
        libssl3
        libsystemd0
    
    MAINTAINER "Your Name <your.email@example.com>"
    HOMEPAGE "https://github.com/yourorg/my_app"
)
```

### Generated Outputs

**Binary**: `/usr/bin/protoflow-${NAME}`

**Systemd Service**: `/lib/systemd/system/protoflow-${NAME}.service`
```ini
[Unit]
Description=Protoflow Application: ${DESCRIPTION}
After=network.target protoflow-main-app.service
Requires=protoflow-main-app.service

[Service]
Type=simple
User=${USER}
Group=${USER}
ExecStart=/usr/bin/protoflow-${NAME}
Restart=always
RestartSec=5

# Configuration
Environment="PROTOFLOW_LOGGING_CONFIG=/etc/protoflow/${NAME}/logging.ini"

# Security hardening (NO hardware access)
PrivateTmp=yes
NoNewPrivileges=yes
ProtectSystem=strict
ProtectHome=yes
ProtectKernelTunables=yes
ProtectKernelModules=yes
ProtectControlGroups=yes
ReadWritePaths=/var/lib/protoflow/${NAME}

# No device access - all hardware via main app RPC
PrivateDevices=yes

[Install]
WantedBy=multi-user.target
```

**Debian Package**: `protoflow-${NAME}_${VERSION}_${ARCH}.deb`

**Package Structure**:
```
/usr/bin/protoflow-${NAME}
/lib/systemd/system/protoflow-${NAME}.service/etc/protoflow/${NAME}/logging.ini         (logging configuration)/etc/protoflow/pki/${NAME}/          (directory, owned by ${USER})
/var/lib/protoflow/${NAME}/          (directory, owned by ${USER})
/usr/share/doc/protoflow-${NAME}/
    copyright
    changelog.gz
```

**Post-Install Script** (`postinst`):
```bash
#!/bin/bash
set -e

# Create unprivileged user if not exists (NO hardware access)
if ! id "${USER}" &>/dev/null; then
    useradd --system --no-create-home \
            --shell /usr/sbin/nologin \
            "${USER}"
fi

# Create directories
mkdir -p /etc/protoflow/${NAME}
mkdir -p /etc/protoflow/pki/${NAME}
mkdir -p /var/lib/protoflow/${NAME}

# Set permissions
chown root:${USER} /etc/protoflow/${NAME}
chmod 750 /etc/protoflow/${NAME}

# Logging config is readable by app user
if [ -f /etc/protoflow/${NAME}/logging.ini ]; then
    chown root:${USER} /etc/protoflow/${NAME}/logging.ini
    chmod 640 /etc/protoflow/${NAME}/logging.ini
fi

chown ${USER}:${USER} /etc/protoflow/pki/${NAME}
chown ${USER}:${USER} /var/lib/protoflow/${NAME}
chmod 700 /etc/protoflow/pki/${NAME}

# Reload systemd
systemctl daemon-reload

# Enable service
systemctl enable protoflow-${NAME}.service

# Note: Hardware access is managed exclusively by protoflow-main-app
# This app declares requirements via HW_REQUIREMENTS but has no direct access
```

---

## Library Macro

### protoflow_add_library

Creates a protoflow library with proper modern CMake target configuration and Debian package generation.

```cmake
protoflow_add_library(
    NAME my_library
    VERSION 1.2.3
    DESCRIPTION "My protoflow library"
    
    # Type
    TYPE STATIC  # or SHARED or INTERFACE
    
    # Source files
    SOURCES
        src/component1.cpp
        src/component2.cpp
    
    # Public headers
    PUBLIC_HEADERS
        include/my_library/component1.hpp
        include/my_library/component2.hpp
    
    # Include directories
    PUBLIC_INCLUDE_DIRS
        include
    
    PRIVATE_INCLUDE_DIRS
        src/internal
    
    # Dependencies
    LINK_LIBRARIES
        protoflow::runtime
        protoflow::messaging
    
    PUBLIC_LINK_LIBRARIES
        protoflow::logging
    
    # Debian packaging
    DEBIAN_DEPENDS
        libprotoflow-runtime
        libprotoflow-messaging
    
    MAINTAINER "Your Name <your.email@example.com>"
    HOMEPAGE "https://github.com/yourorg/my_library"
)
```

### Generated Outputs

**Library Files**:
- Static: `libprotoflow-${NAME}.a`
- Shared: `libprotoflow-${NAME}.so.${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}`
- Interface: Header-only

**Development Package**: `libprotoflow-${NAME}-dev_${VERSION}_${ARCH}.deb`

**Runtime Package** (shared only): `libprotoflow-${NAME}_${VERSION}_${ARCH}.deb`

**Package Structure (dev)**:
```
/usr/lib/${ARCH}/libprotoflow-${NAME}.a
/usr/lib/${ARCH}/libprotoflow-${NAME}.so -> libprotoflow-${NAME}.so.${MAJOR}
/usr/include/protoflow/${NAME}/
    *.hpp
/usr/lib/${ARCH}/cmake/protoflow-${NAME}/
    protoflow-${NAME}Config.cmake
    protoflow-${NAME}ConfigVersion.cmake
    protoflow-${NAME}Targets.cmake
/usr/lib/${ARCH}/pkgconfig/
    protoflow-${NAME}.pc
/usr/share/doc/libprotoflow-${NAME}-dev/
    copyright
    changelog.gz
```

**CMake Config** (`protoflow-${NAME}Config.cmake`):
```cmake
include(CMakeFindDependencyMacro)

# Find dependencies
find_dependency(protoflow-runtime)
find_dependency(protoflow-messaging)

include("${CMAKE_CURRENT_LIST_DIR}/protoflow-${NAME}Targets.cmake")
```

**pkg-config** (`protoflow-${NAME}.pc`):
```
prefix=/usr
exec_prefix=${prefix}
libdir=${exec_prefix}/lib/${ARCH}
includedir=${prefix}/include

Name: protoflow-${NAME}
Description: ${DESCRIPTION}
Version: ${VERSION}
Requires: protoflow-runtime protoflow-messaging
Libs: -L${libdir} -lprotoflow-${NAME}
Cflags: -I${includedir}
```

---

## Logging Configuration

### Configuration File Location

Each app has its own logging configuration file:
- **Path**: `/etc/protoflow/${APP_NAME}/logging.ini` (or `.csv`)
- **Permissions**: `root:${USER}`, mode `640` (readable by app)
- **Environment**: `PROTOFLOW_LOGGING_CONFIG` points to the file

### INI Format

**Example** (`/etc/protoflow/sensor-monitor/logging.ini`):
```ini
# Logging configuration for sensor-monitor

[global]
min_level = info
default_sink = multi

[sink.console]
type = console
format = "[{timestamp}] {level}: {message}"
color = true

[sink.file]
type = file
path = /var/lib/protoflow/sensor-monitor/app.log
max_size = 10M
max_files = 5
format = "{timestamp} | {level:5} | {logger:20} | {message}"

[sink.syslog]
type = syslog
facility = local0
identifier = protoflow-sensor-monitor

[sink.multi]
type = fanout
sinks = console,file,syslog

# Logger-specific configuration
[logger.sensor_service]
min_level = debug
sink = multi

[logger.rpc_client]
min_level = warn
sink = file

[logger.fsm]
min_level = info
sink = multi
# FSM transitions logged here
```

### CSV Format

**Example** (`/etc/protoflow/sensor-monitor/logging.csv`):
```csv
logger,min_level,sink,format
global,info,multi,
sensor_service,debug,multi,
rpc_client,warn,file,
fsm,info,multi,
fsm.otherwise,error,syslog,"FSM otherwise: {fsm} {from}->{to} event={event}"
```

**Sink Definitions** (`sinks.csv`):
```csv
name,type,path,max_size,max_files,facility,format
console,console,,,,,[{timestamp}] {level}: {message}
file,file,/var/lib/protoflow/sensor-monitor/app.log,10M,5,,{timestamp} | {level} | {message}
syslog,syslog,,,,,local0,
multi,fanout,"console,file,syslog",,,,
```

### Configuration Fields

**Global**:
- `min_level`: Minimum log level (debug, info, warn, error, fatal)
- `default_sink`: Default sink for loggers

**Sinks**:
- `type`: console, file, syslog, fanout, null
- `format`: Format string with placeholders
- `path`: File path (for file sink)
- `max_size`: Maximum log file size
- `max_files`: Number of rotated files to keep
- `facility`: Syslog facility (for syslog sink)
- `identifier`: Syslog identifier
- `sinks`: Comma-separated list of sinks (for fanout)

**Loggers**:
- Named loggers for different components
- Per-logger min_level and sink overrides
- Special logger `fsm.otherwise` for FSM misbehavior

### Runtime Behavior

**Loading**:
1. App starts and reads `$PROTOFLOW_LOGGING_CONFIG`
2. Parse INI or CSV based on file extension
3. Configure logging sinks and loggers
4. Inject sinks into FSMs, services, etc.

**Dynamic Reconfiguration**:
```cpp
// Apps can reload config on SIGHUP
signal(SIGHUP, [](int) {
    auto config_path = std::getenv("PROTOFLOW_LOGGING_CONFIG");
    logging::reload_config(config_path);
});
```

**Validation**:
- Invalid config → log to stderr and use defaults
- Missing config → use built-in defaults
- File permissions → must be readable by app user

---

## Implementation Details

### CMake Module File

**Location**: `cmake/ProtoflowMacros.cmake`

```cmake
function(protoflow_add_app)
    set(options "")
    set(oneValueArgs 
        NAME VERSION DESCRIPTION PKI_PATH USER MAINTAINER HOMEPAGE LOGGING_CONFIG)
    set(multiValueArgs 
        SOURCES HW_REQUIREMENTS LINK_LIBRARIES 
        DEPENDS_APPS DEBIAN_DEPENDS)
    
    cmake_parse_arguments(
        APP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Create executable
    add_executable(protoflow-${APP_NAME} ${APP_SOURCES})
    
    # Link libraries
    target_link_libraries(protoflow-${APP_NAME} 
        PRIVATE ${APP_LINK_LIBRARIES})
    
    # Set properties
    set_target_properties(protoflow-${APP_NAME} PROPERTIES
        VERSION ${APP_VERSION}
        OUTPUT_NAME protoflow-${APP_NAME}
    )
    
    # Generate systemd service
    configure_file(
        ${CMAKE_SOURCE_DIR}/cmake/templates/app.service.in
        ${CMAKE_BINARY_DIR}/systemd/protoflow-${APP_NAME}.service
        @ONLY
    )
    
    # Install targets
    install(TARGETS protoflow-${APP_NAME}
        RUNTIME DESTINATION bin
    )
    
    install(FILES 
        ${CMAKE_BINARY_DIR}/systemd/protoflow-${APP_NAME}.service
        DESTINATION lib/systemd/system
    )
    
    # Install logging configuration if provided
    if(APP_LOGGING_CONFIG)
        install(FILES ${APP_LOGGING_CONFIG}
            DESTINATION /etc/protoflow/${APP_NAME}/
            RENAME logging.ini
        )
    endif()
    
    # Generate Debian package
    protoflow_generate_deb_app(
        ${APP_NAME} ${APP_VERSION} "${APP_DESCRIPTION}"
        "${APP_DEBIAN_DEPENDS}" "${APP_MAINTAINER}" "${APP_HOMEPAGE}"
        "${APP_USER}" "${APP_HW_REQUIREMENTS}"
    )
endfunction()

function(protoflow_add_library)
    set(options "")
    set(oneValueArgs 
        NAME VERSION DESCRIPTION TYPE MAINTAINER HOMEPAGE)
    set(multiValueArgs 
        SOURCES PUBLIC_HEADERS PUBLIC_INCLUDE_DIRS PRIVATE_INCLUDE_DIRS
        LINK_LIBRARIES PUBLIC_LINK_LIBRARIES DEBIAN_DEPENDS)
    
    cmake_parse_arguments(
        LIB "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Default to STATIC if not specified
    if(NOT LIB_TYPE)
        set(LIB_TYPE STATIC)
    endif()
    
    # Create library
    add_library(protoflow-${LIB_NAME} ${LIB_TYPE} ${LIB_SOURCES})
    add_library(protoflow::${LIB_NAME} ALIAS protoflow-${LIB_NAME})
    
    # Include directories
    target_include_directories(protoflow-${LIB_NAME}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/${LIB_PUBLIC_INCLUDE_DIRS}>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${LIB_PRIVATE_INCLUDE_DIRS}
    )
    
    # Link libraries
    target_link_libraries(protoflow-${LIB_NAME}
        PUBLIC ${LIB_PUBLIC_LINK_LIBRARIES}
        PRIVATE ${LIB_LINK_LIBRARIES}
    )
    
    # Set properties
    set_target_properties(protoflow-${LIB_NAME} PROPERTIES
        VERSION ${LIB_VERSION}
        SOVERSION ${LIB_VERSION_MAJOR}
        OUTPUT_NAME protoflow-${LIB_NAME}
        PUBLIC_HEADER "${LIB_PUBLIC_HEADERS}"
    )
    
    # Install targets
    install(TARGETS protoflow-${LIB_NAME}
        EXPORT protoflow-${LIB_NAME}Targets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        PUBLIC_HEADER DESTINATION include/protoflow/${LIB_NAME}
    )
    
    # Install CMake config files
    install(EXPORT protoflow-${LIB_NAME}Targets
        FILE protoflow-${LIB_NAME}Targets.cmake
        NAMESPACE protoflow::
        DESTINATION lib/cmake/protoflow-${LIB_NAME}
    )
    
    # Generate config files
    include(CMakePackageConfigHelpers)
    
    configure_package_config_file(
        ${CMAKE_SOURCE_DIR}/cmake/templates/Config.cmake.in
        ${CMAKE_BINARY_DIR}/protoflow-${LIB_NAME}Config.cmake
        INSTALL_DESTINATION lib/cmake/protoflow-${LIB_NAME}
    )
    
    write_basic_package_version_file(
        ${CMAKE_BINARY_DIR}/protoflow-${LIB_NAME}ConfigVersion.cmake
        VERSION ${LIB_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    
    install(FILES
        ${CMAKE_BINARY_DIR}/protoflow-${LIB_NAME}Config.cmake
        ${CMAKE_BINARY_DIR}/protoflow-${LIB_NAME}ConfigVersion.cmake
        DESTINATION lib/cmake/protoflow-${LIB_NAME}
    )
    
    # Generate Debian package
    protoflow_generate_deb_library(
        ${LIB_NAME} ${LIB_VERSION} "${LIB_DESCRIPTION}" ${LIB_TYPE}
        "${LIB_DEBIAN_DEPENDS}" "${LIB_MAINTAINER}" "${LIB_HOMEPAGE}"
    )
endfunction()
```

---

## Debian Package Generation

### CPack Configuration

```cmake
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${MAINTAINER}")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "${HOMEPAGE}")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS ON)

# Strict dependencies
set(CPACK_DEBIAN_PACKAGE_DEPENDS "${DEBIAN_DEPENDS}")

# Compression
set(CPACK_DEBIAN_COMPRESSION_TYPE "xz")

# Package naming: name_version_arch.deb
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
```

---

## Usage Examples

### Example App: Sensor Monitor

```cmake
protoflow_add_app(
    NAME sensor-monitor
    VERSION 2.1.0
    DESCRIPTION "Temperature and humidity monitoring application"
    
    SOURCES
        apps/sensor-monitor/main.cpp
        apps/sensor-monitor/sensor_service.cpp
    
    HW_REQUIREMENTS
        /dev/i2c-1
        /dev/spidev0.0
    
    PKI_PATH /etc/protoflow/pki/sensor-monitor
    USER protoflow-sensor
    
    LOGGING_CONFIG apps/sensor-monitor/logging.ini
    
    LINK_LIBRARIES
        protoflow::runtime
        protoflow::messaging
        protoflow::rpc
        protoflow::transport-tcp
        protoflow::fsm
    
    DEPENDS_APPS
        main-app>=1.0.0
    
    DEBIAN_DEPENDS
        "protoflow-main-app (>= 1.0.0)"
        "i2c-tools"
        "libssl3"
    
    MAINTAINER "Andreas <andreas@example.com>"
    HOMEPAGE "https://github.com/yourorg/protoflow"
)
```

### Example Library: Custom Protocol

```cmake
protoflow_add_library(
    NAME custom-protocol
    VERSION 0.5.0
    DESCRIPTION "Custom protocol implementation for protoflow"
    TYPE STATIC
    
    SOURCES
        libs/custom-protocol/src/parser.cpp
        libs/custom-protocol/src/encoder.cpp
    
    PUBLIC_HEADERS
        libs/custom-protocol/include/custom_protocol/parser.hpp
        libs/custom-protocol/include/custom_protocol/encoder.hpp
    
    PUBLIC_INCLUDE_DIRS
        libs/custom-protocol/include
    
    PUBLIC_LINK_LIBRARIES
        protoflow::runtime
        protoflow::messaging
    
    DEBIAN_DEPENDS
        "libprotoflow-runtime-dev"
        "libprotoflow-messaging-dev"
    
    MAINTAINER "Andreas <andreas@example.com>"
    HOMEPAGE "https://github.com/yourorg/protoflow"
)
```

---

## Build Commands

### Build All

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Generate Debian Packages

```bash
cd build
cpack
```

### Install Locally

```bash
sudo cmake --install build
```

### Install from Debian Package

```bash
sudo dpkg -i build/protoflow-sensor-monitor_2.1.0_amd64.deb
sudo systemctl start protoflow-sensor-monitor
```

---

## Additional Features

### Hardware Requirements Validation

The macro can validate hardware requirements at build time:

```cmake
option(VALIDATE_HW_REQUIREMENTS 
    "Validate hardware requirements at build time" OFF)

if(VALIDATE_HW_REQUIREMENTS)
    foreach(hw IN LISTS APP_HW_REQUIREMENTS)
        if(NOT EXISTS "${hw}")
            message(WARNING 
                "Hardware requirement not found: ${hw}")
        endif()
    endforeach()
endif()
```

**Note**: HW_REQUIREMENTS are **declarations only**. They are:
- Used by the main app to validate hardware requests
- Included in app registration metadata
- **NOT used to grant direct hardware access**

### Main App Special Configuration

The main app requires special handling as it runs as root:

**Systemd Service** (`protoflow-main-app.service`):
```ini
[Unit]
Description=Protoflow Main Application
After=network.target

[Service]
Type=simple
User=root
Group=root
ExecStart=/usr/bin/protoflow-main-app
Restart=always
RestartSec=5

# Main app has hardware access
PrivateTmp=yes
ProtectHome=yes

# Hardware configuration
Environment="PROTOFLOW_HW_CONFIG=/etc/protoflow/hardware.conf"

[Install]
WantedBy=multi-user.target
```

**Hardware Configuration** (`/etc/protoflow/hardware.conf`):
```ini
# Hardware resources managed by main app
[serial.usb0]
device = /dev/ttyUSB0
mode = exclusive
timeout = 30s

[i2c.bus1]
device = /dev/i2c-1
mode = shared
max_clients = 4

[gpio.pin23]
pin = 23
mode = exclusive
direction = output
```

### Dependency Graph

Generate app dependency graphs:

```cmake
option(GENERATE_DEPENDENCY_GRAPH 
    "Generate application dependency graph" OFF)

if(GENERATE_DEPENDENCY_GRAPH)
    # Use graphviz to visualize app dependencies
endif()
```

### Version Management

Automatic version bumping and changelog generation:

```bash
cmake -DBUMP_VERSION=MINOR ..
# Increments version from 1.2.3 -> 1.3.0
# Updates CHANGELOG.md
# Creates git tag
```

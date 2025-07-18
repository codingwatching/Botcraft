project(protocolCraft_tests)

set(SRC_FILES
    src/constexpr_string_processing.cpp
    src/hashed_slot.cpp
    src/json.cpp
    src/nbt.cpp
    src/serialization.cpp
    src/templates.cpp
)

add_executable(${PROJECT_NAME} ${SRC_FILES})
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
target_link_libraries(${PROJECT_NAME} PRIVATE Catch2::Catch2WithMain)
target_link_libraries(${PROJECT_NAME} PRIVATE protocolCraft)
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER Tests)

if(BOTCRAFT_COMPRESSION)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_COMPRESSION=1)
endif(BOTCRAFT_COMPRESSION)

# Output the test executable next to the examples and library files
if(MSVC)
    # To avoid having folder for each configuration when building with Visual
    set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG "${BOTCRAFT_OUTPUT_DIR}/bin")
    set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${BOTCRAFT_OUTPUT_DIR}/bin")
    set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${BOTCRAFT_OUTPUT_DIR}/bin")
    set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${BOTCRAFT_OUTPUT_DIR}/bin")
    set_target_properties(${PROJECT_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY_DEBUG "${BOTCRAFT_OUTPUT_DIR}/bin")
    set_target_properties(${PROJECT_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY_RELEASE "${BOTCRAFT_OUTPUT_DIR}/bin")
    set_target_properties(${PROJECT_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${BOTCRAFT_OUTPUT_DIR}/bin")
    set_target_properties(${PROJECT_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${BOTCRAFT_OUTPUT_DIR}/bin")
    set_target_properties(${PROJECT_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${BOTCRAFT_OUTPUT_DIR}/lib")
    set_target_properties(${PROJECT_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${BOTCRAFT_OUTPUT_DIR}/lib")
    set_target_properties(${PROJECT_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${BOTCRAFT_OUTPUT_DIR}/lib")
    set_target_properties(${PROJECT_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL "${BOTCRAFT_OUTPUT_DIR}/lib")

    set_target_properties(${PROJECT_NAME} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded")
else()
    set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${BOTCRAFT_OUTPUT_DIR}/bin")
    set_target_properties(${PROJECT_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${BOTCRAFT_OUTPUT_DIR}/bin")
    set_target_properties(${PROJECT_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${BOTCRAFT_OUTPUT_DIR}/lib")
endif(MSVC)
catch_discover_tests(${PROJECT_NAME} WORKING_DIRECTORY "${BOTCRAFT_OUTPUT_DIR}/bin")

# LibGFXTest/CMakeLists.txt

# Executable erstellen
add_executable(LibGFXTest
    LibGFXTest.cpp
    # Weitere . cpp Dateien hier
 "DefaultPipeline.h" "DefaultPipeline.cpp" "Vertex.h")

# LibGFX linken (GLFW und Vulkan kommen automatisch mit)
target_link_libraries(LibGFXTest 
    PRIVATE LibGFX
    PRIVATE glm::glm
)

# Include-Verzeichnisse (falls nötig)
target_include_directories(LibGFXTest 
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}
)
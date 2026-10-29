cmake_minimum_required(VERSION 3.21)
project(Axion LANGUAGES CXX)

# KR uses LLVM-MinGW on Windows: WIN32 is true, MSVC is false.
if(WIN32 AND MSVC)
    set(MRE_SQLITE_LIBRARY winsqlite3)
else()
    find_package(SQLite3 REQUIRED)
    if(NOT TARGET SQLite3::SQLite3 AND TARGET SQLite::SQLite3)
        add_library(SQLite3::SQLite3 ALIAS SQLite::SQLite3)
    endif()
    set(MRE_SQLITE_LIBRARY SQLite3::SQLite3)
endif()


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ENGINE_SOURCES
    src/main.cpp
    k4d/k4d.cpp
    src/database.cpp
    src/semantic_types.cpp
    src/type_graph.cpp
    src/premise_resolver.cpp
    src/numeric_evaluator.cpp
    src/verifier.cpp
    src/generator.cpp
    src/hypothesis_evolution.cpp
    src/experiment_runner.cpp
    src/cli_monitor.cpp
    src/selftest.cpp
    src/semantic_batch.cpp
    src/module_cache.cpp
    src/open_research.cpp
    src/open_structures.cpp
    src/algorithm_lab.cpp
    src/autonomous_software.cpp
    src/database_first_research.cpp
    src/database_first_complete.cpp
    src/evolution_engine.cpp
)

add_executable(math_research_v03 ${ENGINE_SOURCES})
add_executable(axion_engine ${ENGINE_SOURCES})

target_include_directories(math_research_v03 PRIVATE src k4d)
target_include_directories(axion_engine PRIVATE src k4d)
add_test(NAME k4d_selftest COMMAND math_research_v03 k4d-selftest)

if(WIN32)
    target_link_libraries(math_research_v03 PRIVATE ${MRE_SQLITE_LIBRARY} user32)
    target_compile_definitions(math_research_v03 PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(axion_engine PRIVATE ${MRE_SQLITE_LIBRARY} user32)
    target_compile_definitions(axion_engine PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
    if(MSVC)
        target_compile_options(math_research_v03 PRIVATE /W4 /WX /permissive- /EHsc)
        target_compile_options(axion_engine PRIVATE /W4 /WX /permissive- /EHsc)
    else()
        target_compile_options(math_research_v03 PRIVATE -Wall -Wextra -Wpedantic -Werror)
        target_compile_options(axion_engine PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
else()
    find_package(SQLite3 REQUIRED)
    target_link_libraries(math_research_v03 PRIVATE ${MRE_SQLITE_LIBRARY})
    target_compile_options(math_research_v03 PRIVATE -Wall -Wextra -Wpedantic -Werror)
    target_link_libraries(axion_engine PRIVATE ${MRE_SQLITE_LIBRARY})
    target_compile_options(axion_engine PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

enable_testing()
add_test(NAME engine_selftest COMMAND math_research_v03 selftest "${CMAKE_CURRENT_BINARY_DIR}/selftest")

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND AND NOT WIN32)
    add_test(NAME semantic_batch_integration COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/test_semantic_batch.py" "$<TARGET_FILE:math_research_v03>")
endif()
if(Python3_Interpreter_FOUND AND NOT WIN32)
    add_test(NAME module_inventory_integration COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/test_module_inventory.py" "$<TARGET_FILE:math_research_v03>")
endif()

if(Python3_Interpreter_FOUND)
    add_test(NAME open_research_integration COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/test_open_research.py" "$<TARGET_FILE:math_research_v03>")
endif()

if(Python3_Interpreter_FOUND)
    add_test(NAME open_structures_integration COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/test_open_structures.py" "$<TARGET_FILE:math_research_v03>")
endif()

if(Python3_Interpreter_FOUND AND NOT WIN32)
    add_test(NAME algorithm_lab_integration COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/test_algorithm_lab.py" "$<TARGET_FILE:math_research_v03>")
endif()

if(WIN32)
    add_executable(math_research_ide WIN32 ui/research_ide.cpp)
    target_compile_definitions(math_research_ide PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(math_research_ide PRIVATE /W4 /WX /permissive- /EHsc)
    else()
        target_compile_options(math_research_ide PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    target_link_libraries(math_research_ide PRIVATE comctl32 comdlg32 shell32 dwmapi uxtheme gdi32 user32 ${MRE_SQLITE_LIBRARY})

    add_executable(axion_ide WIN32 ui/research_ide.cpp)
    target_compile_definitions(axion_ide PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(axion_ide PRIVATE /W4 /WX /permissive- /EHsc)
    else()
        target_compile_options(axion_ide PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    target_link_libraries(axion_ide PRIVATE comctl32 comdlg32 shell32 dwmapi uxtheme gdi32 user32 ${MRE_SQLITE_LIBRARY})
endif()

if(MSVC)
    add_executable(math_database_first_ide WIN32 ui/database_first_ide.cpp)
    target_compile_options(math_database_first_ide PRIVATE /W4 /WX /permissive- /EHsc)
endif()

# Autonomous Application Discovery Lab (standalone C++20 generator).
add_executable(math_application_discovery discovery/application_discovery.cpp)
if(MSVC)
    target_compile_options(math_application_discovery PRIVATE /W4 /WX /permissive- /EHsc)
else()
    target_compile_options(math_application_discovery PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME application_discovery_selftest COMMAND math_application_discovery selftest)

# Reproducible application utility evaluation against explicit baselines.
add_executable(math_application_evaluator evaluation/application_evaluator.cpp)
if(MSVC)
    target_compile_options(math_application_evaluator PRIVATE /W4 /WX /permissive- /EHsc)
else()
    target_compile_options(math_application_evaluator PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME application_evaluator_selftest COMMAND math_application_evaluator selftest)

add_executable(math_autonomous_research evolution/autonomous_research.cpp k4d/k4d.cpp)
target_include_directories(math_autonomous_research PRIVATE k4d)
if(MSVC)
 target_compile_options(math_autonomous_research PRIVATE /W4 /WX /permissive- /EHsc)
else()
 target_compile_options(math_autonomous_research PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME autonomous_research_selftest COMMAND math_autonomous_research selftest)

# MRE_APP_1 typed operator-graph language and checked C++ app generator
add_executable(math_application_language language/application_language.cpp)
if(MSVC)
 target_compile_options(math_application_language PRIVATE /W4 /WX /permissive- /EHsc)
else()
 target_compile_options(math_application_language PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME application_language_selftest COMMAND math_application_language selftest)

# Open Goal Lab v5: vector data, autonomous objective hypotheses, held-out tests.
add_executable(math_open_goal_lab open_goals/open_goal_lab.cpp)
if(MSVC)
 target_compile_options(math_open_goal_lab PRIVATE /W4 /WX /permissive- /EHsc)
else()
 target_compile_options(math_open_goal_lab PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME open_goal_lab_selftest COMMAND math_open_goal_lab selftest)

# v6: evolutionary discovery of utility/goal hypotheses and generated standalone applications
add_executable(math_meta_goal_lab meta_goals/meta_goal_lab.cpp)
if(MSVC)
 target_compile_options(math_meta_goal_lab PRIVATE /W4 /WX /permissive- /EHsc)
 target_link_libraries(math_meta_goal_lab PRIVATE ${MRE_SQLITE_LIBRARY})
else()
 target_compile_options(math_meta_goal_lab PRIVATE -Wall -Wextra -Wpedantic -Werror)
 find_package(SQLite3 REQUIRED)
 target_link_libraries(math_meta_goal_lab PRIVATE ${MRE_SQLITE_LIBRARY})
endif()
add_test(NAME meta_goal_lab_selftest COMMAND math_meta_goal_lab selftest)

# v7: isolated persistent research history. Never modifies source databases.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/memory/research_memory.cpp")
add_executable(math_research_memory memory/research_memory.cpp)
if(MSVC)
 target_compile_options(math_research_memory PRIVATE /W4 /WX /permissive- /EHsc)
 target_link_libraries(math_research_memory PRIVATE ${MRE_SQLITE_LIBRARY})
else()
 target_compile_options(math_research_memory PRIVATE -Wall -Wextra -Wpedantic -Werror)
 find_package(SQLite3 REQUIRED)
 target_link_libraries(math_research_memory PRIVATE ${MRE_SQLITE_LIBRARY})
endif()

endif()

add_executable(math_semantic_catalog semantic/semantic_catalog.cpp)
if(MSVC)
 target_compile_options(math_semantic_catalog PRIVATE /W4 /WX /permissive- /EHsc)
 target_link_libraries(math_semantic_catalog PRIVATE ${MRE_SQLITE_LIBRARY})
else()
 target_compile_options(math_semantic_catalog PRIVATE -Wall -Wextra -Wpedantic -Werror)
 find_package(SQLite3 REQUIRED)
 target_link_libraries(math_semantic_catalog PRIVATE ${MRE_SQLITE_LIBRARY})
endif()

# v9 - source-derived, read-only, traceable mathematical discovery
add_executable(math_semantic_discovery semantic_v9/semantic_discovery.cpp)
if(MSVC)
 target_compile_options(math_semantic_discovery PRIVATE /W4 /WX /permissive- /EHsc)
 target_link_libraries(math_semantic_discovery PRIVATE ${MRE_SQLITE_LIBRARY})
else()
 target_compile_options(math_semantic_discovery PRIVATE -Wall -Wextra -Wpedantic -Werror)
 target_link_libraries(math_semantic_discovery PRIVATE ${MRE_SQLITE_LIBRARY})
endif()
add_test(NAME semantic_discovery_selftest COMMAND math_semantic_discovery selftest)

# v10: evidence-traced multi-relation hypothesis graph
add_executable(math_multi_relation multi_relation/multi_relation.cpp)
if(MSVC)
 target_compile_options(math_multi_relation PRIVATE /W4 /WX /permissive- /EHsc)
 target_link_libraries(math_multi_relation PRIVATE ${MRE_SQLITE_LIBRARY})
else()
 target_compile_options(math_multi_relation PRIVATE -Wall -Wextra -Wpedantic -Werror)
 target_link_libraries(math_multi_relation PRIVATE ${MRE_SQLITE_LIBRARY})
endif()
add_test(NAME multi_relation_selftest COMMAND math_multi_relation selftest)

# v11 independent evidence audit; reads existing v10 output without changing the source DB.
add_executable(math_research_audit verification/research_audit.cpp)
if(MSVC)
  target_compile_options(math_research_audit PRIVATE /W4 /WX /permissive- /EHsc)
else()
  target_compile_options(math_research_audit PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME research_audit_selftest COMMAND math_research_audit selftest)

# Autonomous Software Laboratory - real native application generation and measurement.
add_executable(math_software_lab laboratory/software_lab.cpp)
if(MSVC)
 target_compile_options(math_software_lab PRIVATE /W4 /WX /permissive- /EHsc)
 target_link_libraries(math_software_lab PRIVATE psapi)
else()
 target_compile_options(math_software_lab PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME software_lab_selftest COMMAND math_software_lab selftest)

if(WIN32)
 add_executable(math_gui_probe laboratory/gui_probe.cpp)
 if(MSVC)
  target_compile_options(math_gui_probe PRIVATE /W4 /WX /permissive- /EHsc)
 else()
  target_compile_options(math_gui_probe PRIVATE -Wall -Wextra -Wpedantic -Werror)
 endif()
 target_link_libraries(math_gui_probe PRIVATE user32)
endif()

# v14: open, extensible purpose hypotheses + consent-based human study protocol
add_executable(math_open_purpose_v14 purpose_lab/open_purpose.cpp)
if(MSVC)
 target_compile_options(math_open_purpose_v14 PRIVATE /W4 /WX /permissive- /EHsc)
else()
 target_compile_options(math_open_purpose_v14 PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME open_purpose_v14_selftest COMMAND math_open_purpose_v14 selftest)

# Application Designer: turns v14 numerical research candidates into usable Win32 products
add_executable(math_application_designer application_designer/designer.cpp)
if(MSVC)
 target_compile_options(math_application_designer PRIVATE /W4 /WX /permissive- /EHsc)
else()
 target_compile_options(math_application_designer PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME application_designer_selftest COMMAND math_application_designer selftest)

# Open-ended software research: purpose and architecture are data, generated source is an artifact.
add_test(NAME autonomous_software_selftest COMMAND math_research_v03 software-selftest "${CMAKE_CURRENT_BINARY_DIR}/software-selftest")

# Standalone research executable; remains usable when unrelated engine targets fail.
add_executable(math_k4d_research k4d/standalone.cpp k4d/k4d.cpp)
target_include_directories(math_k4d_research PRIVATE k4d)
if(MSVC)
 target_compile_options(math_k4d_research PRIVATE /W4 /WX /permissive- /EHsc)
else()
 target_compile_options(math_k4d_research PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME k4d_standalone_selftest COMMAND math_k4d_research selftest)

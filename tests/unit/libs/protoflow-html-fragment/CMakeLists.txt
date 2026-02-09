# Unit tests for protoflow-html-fragment

add_executable(test_html_fragment
    test_fixed_string.cpp
    test_text.cpp
    test_tags.cpp
    test_elements.cpp
    test_fragment.cpp
)

target_link_libraries(test_html_fragment
    PRIVATE
        protoflow::html-fragment
        GTest::gtest_main
)

gtest_discover_tests(test_html_fragment)

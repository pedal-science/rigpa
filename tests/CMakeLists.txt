# One Qt Test executable per area; each links rigpa_core and runs without a window.
function(rigpa_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE rigpa_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

rigpa_test(tst_navigator)
rigpa_test(tst_devinput)
rigpa_test(tst_settings)
rigpa_test(tst_config)

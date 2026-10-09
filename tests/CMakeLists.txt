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
rigpa_test(tst_framing)
rigpa_test(tst_engineclient)
rigpa_test(tst_enginesession)
rigpa_test(tst_library)
rigpa_test(tst_flow)
rigpa_test(tst_rig)
rigpa_test(tst_schema)
rigpa_test(tst_edits)

# Against the real daemon, when namchain is built beside this repo (skipped otherwise).
set(RIGPA_NAMCHAIN_BUILD "${PROJECT_SOURCE_DIR}/../namchain/build-mac" CACHE PATH
    "A namchain build with namchaind and generated/ test models, for the live tests")
rigpa_test(tst_engine_live)
rigpa_test(tst_rig_live)
rigpa_test(tst_edit_live)
set_tests_properties(tst_engine_live tst_rig_live tst_edit_live PROPERTIES ENVIRONMENT
    "RIGPA_NAMCHAIND=${RIGPA_NAMCHAIN_BUILD}/namchaind;RIGPA_NAMCHAIN_MODELS=${RIGPA_NAMCHAIN_BUILD}/generated")

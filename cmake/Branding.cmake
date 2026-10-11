if(NOT TARGET dLabBranding)
  juce_add_binary_data(dLabBranding HEADER_NAME dLabBranding.h NAMESPACE dLabBranding
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../resources/images/dLab_by_Frozz_vector.svg")
  target_include_directories(dLabBranding PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../include")
endif()

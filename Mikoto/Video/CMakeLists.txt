# Video uses the engine's asset workers; demuxing and H.264 parsing stay on the CPU.
file(GLOB SOURCE_FILES CONFIGURE_DEPENDS Sources/*.cc)
target_sources(Mikoto PRIVATE ${SOURCE_FILES})

if(WIN32)
    target_link_libraries(Mikoto PRIVATE mfplat mfuuid wmcodecdspuuid ole32)
endif()

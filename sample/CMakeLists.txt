cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{DOLCESDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{DOLCESDK}/share/dolce.toolchain.cmake" CACHE PATH "toolchain file")
    else()
    message(FATAL_ERROR "Please define DOLCESDK to point to your SDK path!")
    endif()
endif()

project(libvitaSAS_sample)
include("${DOLCESDK}/share/dolce.cmake" REQUIRED)
set(VITA_APP_NAME "libvitaSAS Sample")
set(VITA_TITLEID  "LVSASAMPL")

link_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
	)

add_executable(${PROJECT_NAME}
	src/main.c
	)

target_link_libraries(${PROJECT_NAME}
	SceLibKernel_stub
	SceIofilemgr_stub 	
	SceSysmodule_stub
	SceCodecEngine_stub 
	SceAudiodec_stub
	SceAudio_stub 
	SceDbg_stub
	SceSas_stub
	SceFios2_stub
	SceGxm_stub
	SceDisplay_stub
	SceAppMgr_stub
	vitaSAS
    )

dolce_create_self(eboot.bin ${PROJECT_NAME} UNSAFE)

dolce_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} eboot.bin
    VERSION ${VITA_VERSION}
    NAME ${VITA_APP_NAME}
	FILE resource/Image.vag resource/Image.vag
    )

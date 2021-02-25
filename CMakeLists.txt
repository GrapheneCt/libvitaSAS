cmake_minimum_required(VERSION 3.19)

set(CMAKE_TOOLCHAIN_FILE "$ENV{SCE_PSP2_SDK_DIR}/host_tools/build/cmake/psp2-snc-toolchain.cmake")

project(vitasas LANGUAGES C)

include(VitaDevelopmentSuite)
set(CMAKE_C_STANDARD 99)

add_compile_options(
  -Xdiag=0 -Xquit=2 -O3
)

add_link_options(
  -s
)

include_directories(
  ${VDSUITE_USER_INCLUDE_DIRECTORIES}
  libvitasas/include
)

link_directories(
  ${VDSUITE_LIBRARY_DIRECTORIES}
  libvitasas/include
)

add_library(${PROJECT_NAME} STATIC
  libvitasas/source/SAS.c
  libvitasas/source/heap.c
  libvitasas/source/audio_out.c
  libvitasas/source/audio_dec_common.c
  libvitasas/source/audio_dec_at9.c
  libvitasas/source/audio_dec_mp3.c
  libvitasas/source/audio_dec_aac.c
)

add_definitions(-DVITASAS_PRX)

add_library("${PROJECT_NAME}.suprx" SHARED
  libvitasas/source/SAS.c
  libvitasas/source/heap.c
  libvitasas/source/audio_out.c
  libvitasas/source/audio_dec_common.c
  libvitasas/source/audio_dec_at9.c
  libvitasas/source/audio_dec_mp3.c
  libvitasas/source/audio_dec_aac.c
)

target_link_libraries("${PROJECT_NAME}.suprx"
  SceSysmem_stub_weak
  SceIofilemgr_stub_weak
  SceSysmodule_stub_weak
  SceCodecEngineUser_stub_weak
  SceThreadmgr_stub_weak
  SceAudiodecUser_stub_weak
  SceAudio_stub_weak
  SceDbg_stub_weak
  SceSas_stub_weak
  SceFios2_stub_weak
)

/**
 * vk_mem_alloc.h
 * Created by kate on 8/8/2023.
 * */

#define VMA_IMPLEMENTATION

#include <cstdio>
#include <string>

static int vmaDestroyBufferCount = 0;
static int vmaCreateImageCount = 0;


#ifndef VMA_DEBUG_LOG
   #define VMA_DEBUG_LOG(format, ...)

   #define VMA_DEBUG_LOG(format, ...) do { \
       std::printf(format, __VA_ARGS__); \
       vmaDestroyBufferCount += std::string(format) == "vmaCreateBuffer" ? 1 : 0;  \
       vmaDestroyBufferCount -= std::string(format) == "vmaDestroyBuffer" ? 1 : 0;  \
vmaCreateImageCount += std::string(format) == "vmaCreateImage" ? 1 : 0;  \
vmaCreateImageCount -= std::string(format) == "vmaDestroyImage" ? 1 : 0;  \
       std::printf("%d", vmaDestroyBufferCount); \
       std::printf("%d", vmaCreateImageCount); \
       std::printf("\n"); \
   } while(false)

#endif

#undef VMA_DEBUG_LOG // enable if u want to see the logs from vma

#include <vk_mem_alloc.h>
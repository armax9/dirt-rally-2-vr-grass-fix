#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define STATIC_ASSERT(x) _Static_assert(x, #x)
enum vkd3d_md5_variant { VKD3D_MD5_STANDARD, VKD3D_MD5_DXBC };
static inline uint16_t vkd3d_make_u16(uint8_t a,uint8_t b){return (uint16_t)a|((uint16_t)b<<8);}
static inline uint32_t vkd3d_make_u32(uint16_t a,uint16_t b){return (uint32_t)a|((uint32_t)b<<16);}

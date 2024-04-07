#ifndef _STRUCTS
#define _STRUCTS

#ifdef __cplusplus
extern "C" {
#endif

struct BufferData {
    unsigned int buffer_size;
    unsigned short bitmap_pitch;
    unsigned short bitmap_width;
    unsigned short bitmap_height;
    unsigned char bitmap_bpp;
    unsigned char attrs;
    unsigned int magic;
};

#ifdef __cplusplus
} // End of extern "C"
#endif

#define MAGIC_NUMBER 0xd34db33f

#define ATTR_VFLIP 0x01

#endif // _STRUCTS

#ifndef _STRUCTS
#define _STRUCTS

#ifdef __cplusplus
extern "C" {
#endif

struct BufferData {
    int buffer_size;
    int bitmap_pitch;
    int bitmap_width;
    int bitmap_height;
    int bitmap_bpp;
};

#ifdef __cplusplus
} // End of extern "C"
#endif

#endif // _STRUCTS

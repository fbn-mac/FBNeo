#include <stdlib.h>
#include <stdio.h>

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "burner.h"
#include "rgbserver.h"

#define SCALE_MODE_NONE            0
#define SCALE_MODE_SHORTESTXASPECT 1

#define RED(c) ((((c)>>11)&0x1f)*255/31)
#define GREEN(c) ((((c)>>5)&0x3f)*255/63)
#define BLUE(c) (((c)&0x1f)*255/31)
#define RGB(r,g,b) ((((r)&0xff)>>3)<<11)|((((g)&0xff)>>2)<<5)|(((b)&0xff)>>3)

static int renderBufferSize;
static int renderBufferWidth;
static int renderBufferHeight;
static int bufferBpp;
static int outputBufferSize;
static int outputBufferWidth;
static int outputBufferHeight;
static int aspectX;
static int aspectY;
static unsigned char *renderBuffer = NULL;
// In instances where there is no processing,
// outputBuffer will be pointing to renderBuffer.
static unsigned char *outputBuffer = NULL;

// Screen dimensions, in landscape orientation
static int landscapeWidth = 0;
static int landscapeHeight = 0;
static int scaleMode = SCALE_MODE_NONE;

static int screenRotated = 0;
static int screenFlipped = 0;

static int show_server_fps = 0;

static int resetBuffers()
{
    fprintf(stderr, "Setting up renderBuffer...\n");

    if (outputBuffer != renderBuffer) {
        free(outputBuffer);
    }
    free(renderBuffer);

    renderBufferSize = renderBufferWidth * renderBufferHeight * bufferBpp;
    if ((renderBuffer = (unsigned char *)malloc(renderBufferSize)) == NULL) {
        fprintf(stderr, "Error allocating render buffer\n");
        return 0;
    }
    fprintf(stderr, "Allocated render buffer of size (%d)\n", renderBufferSize);

    if (
        outputBufferWidth != renderBufferWidth
            || outputBufferHeight != renderBufferHeight
    ) {
        outputBufferSize = outputBufferWidth * outputBufferHeight * bufferBpp;
        if ((outputBuffer = (unsigned char *)malloc(outputBufferSize)) == NULL) {
            fprintf(stderr, "Error allocating output buffer\n");
            return 0;
        }
        fprintf(stderr, "Allocated output buffer of size (%d)\n", outputBufferSize);
    } else {
        // No processing
        outputBufferSize = renderBufferSize;
        outputBuffer = renderBuffer;
    }

    nBurnBpp = bufferBpp;
    nBurnPitch = nVidImagePitch;
    pVidImage = renderBuffer;

    memset(renderBuffer, 0, renderBufferSize);
    if (outputBuffer != renderBuffer) {
        memset(outputBuffer, 0, outputBufferSize);
    }

    int width = screenRotated ? outputBufferHeight : outputBufferWidth;
    struct FrameGeometry data = {
        outputBufferSize,
        width * bufferBpp,
        width,
        screenRotated ? outputBufferWidth : outputBufferHeight,
        PIXEL_FORMAT_RGB565,
        (screenRotated && !screenFlipped) ? ATTR_ROT180 : 0,
        MAGIC_NUMBER
    };
    rgbs_set_buffer_data(data);

    return 1;
}

static void initOutputBuffer()
{
    if (!screenRotated) {
        if (renderBufferWidth > landscapeWidth) {
            if (scaleMode == SCALE_MODE_SHORTESTXASPECT) {
                outputBufferWidth = (int)(renderBufferHeight * ((float) aspectX / aspectY));
                outputBufferHeight = renderBufferHeight;
            }
        } else if (renderBufferHeight > landscapeHeight) {
            if (scaleMode == SCALE_MODE_SHORTESTXASPECT) {
                outputBufferWidth = renderBufferWidth;
                outputBufferHeight = (int)(renderBufferWidth * ((float) aspectY / aspectX));
            }
        }
    } else {
        if (renderBufferHeight > landscapeWidth) {
            if (scaleMode == SCALE_MODE_SHORTESTXASPECT) {
                outputBufferWidth = renderBufferWidth;
                outputBufferHeight = (int)(renderBufferWidth * ((float) aspectY / aspectX));
            }
        } else if (renderBufferWidth > landscapeHeight) {
            if (scaleMode == SCALE_MODE_SHORTESTXASPECT) {
                outputBufferWidth = (int)(renderBufferHeight * ((float) aspectX / aspectY));
                outputBufferHeight = renderBufferHeight;
            }
        }
    }

    if (outputBufferWidth == 0 || outputBufferHeight == 0) {
        outputBufferWidth = renderBufferWidth;
        outputBufferHeight = renderBufferHeight;
        fprintf(stderr, "no dedicated outputBuffer\n");
    } else {
        fprintf(stderr, "outputBuffer: %dx%d\n",
            outputBufferWidth,
            outputBufferHeight);
    }
}

static int parseDimensions(const char *arg)
{
    char buf[64];
    strncpy(buf, arg, sizeof(buf) - 1);

    char *delim = strchr(buf, 'x');
    if (!delim) {
        fprintf(stderr, "warning: dimensions ignored; missing w/h delimiter\n");
        return 0;
    }
    *delim = '\0';
    int width = atoi(buf);
    int height = atoi(delim + 1);

    if (width == 0 || height == 0) {
        fprintf(stderr, "warning: dimensions ignored; '%s' not parsed as valid dimensions\n", arg);
        return 0;
    }

    fprintf(stderr, "output device dimensions: %dx%d\n", width, height);

    landscapeWidth = width;
    landscapeHeight = height;

    return 1;
}

static int parseScaleMode(const char *arg)
{
    if (strcmp(arg, "shortestxaspect") == 0) {
        scaleMode = SCALE_MODE_SHORTESTXASPECT;
        fprintf(stderr, "scaling mode: SCALE_MODE_SHORTESTXASPECT\n");
    } else if (strcmp(arg, "none") == 0) {
        scaleMode = SCALE_MODE_NONE;
        fprintf(stderr, "scaling mode: SCALE_MODE_NONE\n");
    } else {
        fprintf(stderr, "warning: unrecognized scaling mode: %s\n", arg);
        return 0;
    }

    return 1;
}

// Specific to FB

static int FbInit()
{
    int virtualWidth;
    int virtualHeight;

    extern int exec_argc;
    extern char **exec_argv;

    for (int i = 1; i < exec_argc; i++) {
        const char *arg = exec_argv[i];
        if (strcmp(arg, "--show-fps") == 0) {
            show_server_fps = 1;
        } else if (strstr(arg, "--output-dims=") != NULL) {
            parseDimensions(arg + 14);
        } else if (strstr(arg, "--scale=") != NULL) {
            parseScaleMode(arg + 8);
        }
    }

    if (scaleMode != SCALE_MODE_NONE && (landscapeWidth == 0 || landscapeHeight == 0)) {
        fprintf(stderr, "error: scaling mode supplied without device dimensions\n");
        return 1;
    }

    if (bDrvOkay) {
        BurnDrvGetAspect(&aspectX, &aspectY);
        BurnDrvGetVisibleSize(&virtualWidth, &virtualHeight);
        if (BurnDrvGetFlags() & BDF_ORIENTATION_VERTICAL) {
            screenRotated = 1;
        }
        
        if (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED) {
            screenFlipped = 1;
        }
        
        fprintf(stderr, "Game screen size: %dx%d (%s,%s) (aspect: %d:%d)\n",
            virtualWidth, virtualHeight,
            screenRotated ? "rotated" : "not rotated",
            screenFlipped ? "flipped" : "not flipped",
            aspectX, aspectY);

        nVidImageDepth = 16;
        nVidImageBPP = 2;
        
        float ratio = (float) aspectX / aspectY;

        if (!screenRotated) {
            nVidImageWidth = virtualWidth;
            nVidImageHeight = virtualHeight;
        } else {
            nVidImageWidth = virtualHeight;
            nVidImageHeight = virtualWidth;
        }

        nVidImagePitch = nVidImageWidth * nVidImageBPP;
        
        SetBurnHighCol(nVidImageDepth);
        
        renderBufferWidth = virtualWidth;
        renderBufferHeight = virtualHeight;
        bufferBpp = nVidImageBPP;

        fprintf(stderr, "renderBuffer: %dx%d (%.04f ratio) @ %dbpp\n",
            renderBufferWidth,
            renderBufferHeight,
            ratio,
            bufferBpp);

        initOutputBuffer();
        if (!resetBuffers()) {
            fprintf(stderr, "Error resetting buffers\n");
            return 1;
        }

        if (!rgbs_start()) {
            fprintf(stderr, "Error initializing rgbs\n");
            return 1;
        }
    }

    return 0;
}

static int FbExit()
{
    rgbs_end();

    fprintf(stderr, "Destroying buffers\n");
    if (outputBuffer != renderBuffer) {
        free(outputBuffer);
        outputBuffer = NULL;
    }
    free(renderBuffer);
    renderBuffer = NULL;

    return 0;
}

static int FbRunFrame(bool bRedraw)
{
    if (pVidImage == NULL) {
        return 1;
    }

    if (bDrvOkay) {
        if (bRedraw) {								// Redraw current frame
            if (BurnDrvRedraw()) {
                BurnDrvFrame();						// No redraw function provided, advance one frame
            }
        } else {
            BurnDrvFrame();							// Run one frame and draw the screen
        }
    }

    return 0;
}

static unsigned long long current_millis()
{
    struct timeval te;
    gettimeofday(&te, NULL);
    return te.tv_sec * 1000LL + te.tv_usec / 1000;
}

static inline void log_fps()
{
    static unsigned long long pms = 0;
    static int frames = 0;
    unsigned long long ms = current_millis();
    unsigned long long delta = ms - pms;
    if (delta > 1000L) {
        float fps = (float) frames / (delta / 1000L);
        fprintf(stderr, "\rfps: %.02f", fps);
        frames = 0;
        pms = ms;
    } else {
        frames++;
    }
}

static void process() {
    if (
        renderBufferWidth != outputBufferWidth
            || renderBufferHeight != outputBufferHeight
    ) {
        // Based on https://www.reddit.com/r/C_Programming/comments/16j7k4d/optimizing_image_downsampling_for_speed/
        // Assumes a 2bpp bitmap
        unsigned short *destRow = (unsigned short *) outputBuffer;
        unsigned short *source = (unsigned short *) renderBuffer;
        int destWidth = (screenRotated) ? outputBufferHeight : outputBufferWidth;
        int destHeight = (screenRotated) ? outputBufferWidth : outputBufferHeight;
        int srcWidth = (screenRotated) ? renderBufferHeight : renderBufferWidth;
        int srcHeight = (screenRotated) ? renderBufferWidth : renderBufferHeight;

        float ratioX = srcWidth / (float) (destWidth + 1);
        float ratioY = srcHeight / (float) (destHeight + 1);
        for (int y = 0; y < destHeight; ++y) {
            int y1 = y * ratioY;
            int y2 = (y + 1) * ratioY;

            unsigned short *sourceRow = source + y1 * srcWidth;
            unsigned short *dest = destRow;
            for (int x = 0; x < destWidth; ++x) {
                int x1 = x * ratioX;
                int x2 = (x + 1) * ratioX;

                unsigned int r = 0;
                unsigned int g = 0;
                unsigned int b = 0;

                unsigned short *pixels = sourceRow;
                for (int i = y1; i < y2; ++i) {
                    for (int j = x1; j < x2; ++j) {
                        unsigned short c = pixels[j];
                        r += RED(c);
                        g += GREEN(c);
                        b += BLUE(c);
                    }
                    pixels += srcWidth;
                }

                int pixelCount = (x2 - x1) * (y2 - y1);
                r /= pixelCount;
                g /= pixelCount;
                b /= pixelCount;
                *dest++ = RGB(r,g,b);
            }
            destRow += destWidth;
        }
    } else {
        fprintf(stderr, "Attempt to process identically-sized buffers\n");
    }
}

static int FbPaint(int bValidate)
{
    rgbs_poll();

    if (renderBuffer != outputBuffer) {
        process();
    }

    rgbs_send(outputBuffer, outputBufferSize);
    if (show_server_fps) {
        log_fps();
    }

    return 0;
}

static int FbGetSettings(InterfaceInfo *)
{
    return 0;
}

static int FbVidScale(RECT *, int, int)
{
    return 0;
}

struct VidOut VidOutPi = { FbInit, FbExit, FbRunFrame, FbPaint, FbVidScale, FbGetSettings, _T("streaming server output") };

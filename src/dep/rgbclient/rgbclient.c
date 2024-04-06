#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "led-matrix-c.h"
#include "structs.h"

#define DESTPORT 3500

struct ViewRect {
    unsigned short sx;
    unsigned short sy;
    unsigned short dx;
    unsigned short dy;
};

static int show_panel_fps = 0;
static int show_server_fps = 0;
static int sockfd = -1;
static int exit_main_loop = 0;
static struct BufferData data;
static unsigned char *buffer = NULL;

static const char *server;
static int screen_width;
static int screen_height;

static struct RGBLedMatrix *matrix = NULL;
static struct LedCanvas *canvas = NULL;
static struct ViewRect source;
static struct ViewRect dest;

#define RED(x) (((x>>11)&0x1f)*255/31)
#define GREEN(x) (((x>>5)&0x3f)*255/63)
#define BLUE(x) ((x&0x1f)*255/31)

static int connect_es(const char *server)
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        fprintf(stderr, "socket() failed\n");
        return 0;
    }

    struct sockaddr_in dest_addr = {};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DESTPORT);
    dest_addr.sin_addr.s_addr = inet_addr(server);

    if (connect(sockfd,(struct sockaddr*)&dest_addr, sizeof(struct sockaddr)) != 0) {
        fprintf(stderr, "connect() failed\n");
        close(sockfd);
        return 0;
    }

    fprintf(stderr, "connected!\n");

    return 1;
}

static int read_preamble()
{
    int r = read(sockfd, &data, sizeof(data));
    if (r != sizeof(data)) {
        fprintf(stderr, "error reading buffer data (read %d bytes)\n", r);
        return 0;
    }

    fprintf(stderr, "buffer size: %d\npitch: %d\nwidth: %d\nheight: %d\nbpp: %d\n",
        data.buffer_size,
        data.bitmap_pitch,
        data.bitmap_width,
        data.bitmap_height,
        data.bitmap_bpp
    );

    if (data.bitmap_bpp * data.bitmap_width != data.bitmap_pitch) {
        fprintf(stderr, "bitmap pitch does not fempute (%d != %d)\n",
            data.bitmap_bpp * data.bitmap_width,
            data.bitmap_pitch
        );
        return 0;
    }
    fprintf(stderr, "projected Mbps: %.02f\n",
        (float)(data.buffer_size * 60) / 1024 / 1024
    );

    return 1;
}

static int create_buffer()
{
    if ((buffer = (unsigned char *)malloc(data.buffer_size)) == NULL) {
        fprintf(stderr, "Error allocating buffer\n");
        return 0;
    }

    return 1;
}

static void clean_up()
{
    if (matrix != NULL) {
        led_matrix_delete(matrix);
        matrix = NULL;
    }
    if (sockfd != -1) {
        fprintf(stderr, "closing...\n");
        close(sockfd);
        sockfd = -1;
    }
    if (buffer != NULL) {
        free(buffer);
        buffer = NULL;
    }
}

static int init_rgb(int argc, char **argv)
{
    fprintf(stderr, "initializing LED matrix...\n");
    struct RGBLedMatrixOptions options = {};
    options.show_refresh_rate = show_panel_fps;
    matrix = led_matrix_create_from_options(&options, &argc, &argv);
    if (matrix == NULL) {
        fprintf(stderr, "error initializing matrix\n");
        return 0;
    }

    canvas = led_matrix_create_offscreen_canvas(matrix);

    led_canvas_get_size(canvas, &screen_width, &screen_height);
    led_canvas_set_pixel(canvas, 0, 0, 0, 0, 0);

    fprintf(stderr, "initialized %dx%d canvas...\n", screen_width, screen_height);
    return 1;
}

static void render()
{
    // VALIDATE BITMAP!
    for (int ry = source.sy, yo = 0; ry < source.dy; ry++, yo++) {
        if (ry >= data.bitmap_height) {
            break; // out of bounds
        }
        unsigned char *rrow = buffer + ry * data.bitmap_pitch;
        for (int rx = source.sx, xo = 0; rx < source.dx; rx++, xo++) {
            if (rx >= data.bitmap_width) {
                break; // out of bounds
            }
            int wy = dest.sy + yo;
            int wx = dest.sx + xo;
            unsigned short rcol = *((unsigned short *)rrow + rx);
            if (wx < screen_width && wy < screen_height) {
                led_canvas_set_pixel(canvas, wx, wy, RED(rcol), GREEN(rcol), BLUE(rcol));
            }
        }
    }
    canvas = led_matrix_swap_on_vsync(matrix, canvas);
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
    int delta = ms - pms;
    if (delta > 1000L) {
        float fps = (float) frames / (delta / 1000L);
        fprintf(stderr, "\rfps: %.02f", fps);
        frames = 0;
        pms = ms;
    } else {
        frames++;
    }
}

static void main_loop()
{
    int nread = 0;
    while (!exit_main_loop) {
        while (nread < data.buffer_size) {
            int r = read(sockfd, buffer + nread, data.buffer_size - nread);
            if (r <= 0) {
                fprintf(stderr, "server disconnected\n");
                return;
            }
            nread += r;
        }
        if (show_server_fps) {
            log_fps();
        }
        nread = nread - data.buffer_size;
        render();
    }
}

void sigint_handler(int s)
{
    fprintf(stderr, "Caught SIGINT, exiting...\n");
    exit_main_loop = 1;
}

int parse_rect(const char *name, const char *arg, struct ViewRect *rect)
{
    char temp[128];
    strncpy(temp, arg, 127);
    char *delim = strchr(temp, '-');
    if (delim == NULL) {
        fprintf(stderr, "%s: Missing '-' delimiter\n", name);
        return 0;
    }
    *delim = '\0';

    char *sdelim = strchr(temp, ',');
    if (sdelim == NULL) {
        fprintf(stderr, "%s: Missing ',' delimiter in source\n", name);
        return 0;
    }
    *sdelim = '\0';

    char *ddelim = strchr(delim + 1, ',');
    if (ddelim == NULL) {
        fprintf(stderr, "%s: Missing ',' delimiter in destination\n", name);
        return 0;
    }
    *ddelim = '\0';

    rect->sx = strtol(temp, NULL, 10);
    rect->sy = strtol(sdelim + 1, NULL, 10);
    rect->dx = strtol(delim + 1, NULL, 10);
    rect->dy = strtol(ddelim + 1, NULL, 10);

    return 1;
}

int validate_rect(const char *name, struct ViewRect rect)
{
    if (rect.sx > rect.dx) {
        fprintf(stderr, "%s: sx > dx\n", name);
        return 0;
    } else if (rect.sy > rect.dy) {
        fprintf(stderr, "%s: sx > dx\n", name);
        return 0;
    }
    return 1;
}

const char * format_rect(struct ViewRect rect)
{
    static char temp[128];
    snprintf(temp, 127, "%d,%d-%d,%d", rect.sx, rect.sy, rect.dx, rect.dy);
    return temp;
}

int parse_args(int argc, const char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Missing server IP\n");
        return 0;
    }

    server = argv[1];
    const char *srect = NULL;
    const char *drect = NULL;
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "-sfps") == 0) {
            show_server_fps = 1;
        } else if (strcmp(arg, "-lfps") == 0) {
            show_panel_fps = 1;
        } else if (strcmp(arg, "-srect") == 0) {
            if (i + 1 < argc) {
                srect = argv[i + 1];
            } else {
                fprintf(stderr, "Missing argument to -srect");
                return 0;
            }
        } else if (strcmp(arg, "-drect") == 0) {
            if (i + 1 < argc) {
                drect = argv[i + 1];
            } else {
                fprintf(stderr, "Missing argument to -drect");
                return 0;
            }
        }
    }

    if (srect != NULL && !parse_rect("-srect", srect, &source)) {
        return 0;
    } else if (srect == NULL) {
        fprintf(stderr, "Missing -srect\n");
        return 0;
    }

    if (drect != NULL && !parse_rect("-drect", drect, &dest)) {
        return 0;
    } else if (drect == NULL) {
        fprintf(stderr, "Missing -drect\n");
        return 0;
    }

    if (!validate_rect("-srect", source) || !validate_rect("-drect", dest)) {
        return 0;
    }

    return 1;
}

int main(int argc, char **argv)
{
    if (!parse_args(argc, (const char **)argv)) {
        return 1;
    }

    if (!init_rgb(argc, argv)) {
        clean_up();
        return 1;
    }

    if (dest.dx > screen_width) {
        fprintf(stderr, "drect: end (%d) exceeds max (%d)\n",
            dest.dx, screen_width);
        clean_up();
        return 1;
    } else if (dest.dy > screen_height) {
        fprintf(stderr, "drect: end (%d) exceeds max (%d)\n",
            dest.dy, screen_height);
        clean_up();
        return 1;
    }

    if (!connect_es(server)) {
        clean_up();
        return 1;
    }

    if (!read_preamble() || !create_buffer()) {
        clean_up();
        return 1;
    }

    signal(SIGINT, sigint_handler);
    main_loop();
    clean_up();

    fprintf(stderr, "done\n");

    return 0;
}

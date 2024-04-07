#include <stdlib.h>
#include <stdio.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "burner.h"
#include "structs.h"

#define PORT "3500"

static int bufferSize;
static int bufferWidth;
static int bufferHeight;
static int bufferBpp;
static unsigned char *bufferBitmap;

static int screenRotated = 0;
static int screenFlipped = 0;
static int sockfd = -1;

const int clientfd_size = 5;
static int clientfd_count = 0;
static int clientfds[clientfd_size];
static struct pollfd pfd;

static int show_server_fps = 0;

static int writePreamble(int clientfd)
{
    fprintf(stderr, "writing buffer data...\n");
    int flags = BurnDrvGetFlags();

    struct BufferData data = {
        bufferSize,
        nVidImagePitch,
        nVidImageWidth,
        nVidImageHeight,
        nVidImageBPP,
        ((flags & BDF_ORIENTATION_VERTICAL) && !(flags & BDF_ORIENTATION_FLIPPED)) ? ATTR_VFLIP : 0,
        MAGIC_NUMBER
    };

    int w = write(clientfd, &data, sizeof(data));
    if (w != sizeof(data)) {
        fprintf(stderr, "error writing data (wrote %d bytes)\n", w);
        return 0;
    }

    return 1;
}

static void listen()
{
    fprintf(stderr, "listen()\n");

    struct addrinfo hints, *ai, *p;

    // Get us a socket and bind it
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int rv;
    int yes = 1;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "getaddrinfo() failed: %s\n", gai_strerror(rv));
        return;
    }

    for (p = ai; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd < 0) {
            continue;
        }

        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) < 0) {
            close(sockfd);
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "bind() failed\n");
        close(sockfd);
        sockfd = -1;
        return;
    }

    freeaddrinfo(ai);
    if (listen(sockfd, 10) == -1) {
        fprintf(stderr, "listen() failed\n");
        close(sockfd);
        sockfd = -1;
        return;
    }

    pfd.fd = sockfd;
    pfd.events = POLLIN;
}

static int poll_and_accept()
{
    if (sockfd == -1) {
        fprintf(stderr, "sockfd not set\n");
        return 0;
    }

    int poll_count = poll(&pfd, 1, 0);
    if (poll_count == -1) {
        fprintf(stderr, "poll failed\n");
        return 0;
    }

    static socklen_t addrlen;
    static struct sockaddr_in remoteaddr;
    static int newfd;

    if (poll_count > 0 && pfd.events & POLLIN) {
        addrlen = sizeof(remoteaddr);
        newfd = accept(sockfd, (struct sockaddr *)&remoteaddr, &addrlen);
        if (newfd == -1) {
            fprintf(stderr, "accept() failed\n");
            return 0;
        } else if (clientfd_count + 1 < clientfd_size) {
            fprintf(stderr, "new connection from %s\n",
                inet_ntoa(remoteaddr.sin_addr));
            writePreamble(newfd);
            clientfds[clientfd_count++] = newfd;
        } else {
            fprintf(stderr, "ignoring new connection; maxed out\n");
        }
    }

    return 1;
}

static void stop_listening()
{
    fprintf(stderr, "Closing client sockets...\n");
    for (int i = 0; i < clientfd_count; i++) {
        close(clientfds[i]);
    }
    clientfd_count = 0;
    if (sockfd != -1) {
        fprintf(stderr, "Closing server socket\n");
        close(sockfd);
        sockfd = -1;
    }
}

static int resetBuffer()
{
    fprintf(stderr, "Setting up screen...\n");

    free(bufferBitmap);
    bufferSize = nVidImagePitch * bufferHeight;
    if ((bufferBitmap = (unsigned char *)malloc(bufferSize)) == NULL) {
        fprintf(stderr, "Error allocating buffer bitmap\n");
        return 0;
    }

    fprintf(stderr, 
        "Allocated bitmap buffer of size (%d; half: %d)\n",
        nVidImageHeight * nVidImagePitch,
        (nVidImageHeight * nVidImagePitch) / 2
    );
    
    nBurnBpp = bufferBpp;
    nBurnPitch = nVidImagePitch;
    pVidImage = bufferBitmap;

    memset(bufferBitmap, 0, bufferSize);

    return 1;
}

// Specific to FB

static int FbInit()
{
    int virtualWidth;
    int virtualHeight;
    int xAspect;
    int yAspect;

    extern int exec_argc;
    extern char **exec_argv;

    for (int i = 1; i < exec_argc; i++) {
        const char *arg = exec_argv[i];
        if (strcmp(arg, "-fps") == 0) {
            show_server_fps = 1;
        }
    }

    if (bDrvOkay) {
        BurnDrvGetAspect(&xAspect, &yAspect);
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
            xAspect, yAspect);

        nVidImageDepth = 16;
        nVidImageBPP = 2;
        
        float ratio = (float) yAspect / xAspect;

        if (!screenRotated) {
            nVidImageWidth = virtualWidth;
            nVidImageHeight = virtualHeight;
        } else {
            nVidImageWidth = virtualHeight;
            nVidImageHeight = virtualWidth;
        }

        nVidImagePitch = nVidImageWidth * nVidImageBPP;
        
        SetBurnHighCol(nVidImageDepth);
        
        bufferWidth = virtualWidth;
        bufferHeight = virtualHeight;
        bufferBpp = nVidImageBPP;

        fprintf(stderr, "buffer: %dx%d %dbpp\n", bufferWidth, bufferHeight, bufferBpp);

        if (!screenRotated) {
            if (bufferHeight / ratio >= bufferWidth)
                bufferWidth = bufferHeight / ratio;
            else
                bufferHeight = bufferWidth * ratio;
        } else {
            bufferWidth = bufferHeight / ratio;
        }
        fprintf(stderr, "W: %d; H: %d (%f ratio)\n", bufferWidth, bufferHeight, ratio);
        
        if (!resetBuffer()) {
            fprintf(stderr, "Error resetting buffer\n");
            return 1;
        }

        listen();
    }

    return 0;
}

static int FbExit()
{
    stop_listening();

    fprintf(stderr, "Destroying buffer\n");
    free(bufferBitmap);
    bufferBitmap = NULL;

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

static int FbPaint(int bValidate)
{
    if (sockfd != -1 && !poll_and_accept()) {
        fprintf(stderr, "polling failed; closing all connections\n");
        stop_listening();
    }
    for (int i = 0; i < clientfd_count; i++) {
        int w = write(clientfds[i], bufferBitmap, bufferSize);
        if (w <= 0) {
            fprintf(stderr, "client %d disconnected\n", i);
            close(clientfds[i]);
            clientfds[i--] = clientfds[--clientfd_count];
        }
    }
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "SDL.h"

/* Supported YUV-formats */
#define YV12 0
#define IYUV 1
#define YUY2 2
#define UYVY 3
#define YVYU 4

/* IPC */
#define NONE 0
#define MASTER 1
#define SLAVE 2

/* IPC Commands */
#define NEXT 'a'
#define PREV 'b'
#define REW 'c'
#define ZOOM_IN 'd'
#define ZOOM_OUT 'e'
#define QUIT 'f'
#define Y_ONLY 'g'
#define CB_ONLY 'h'
#define CR_ONLY 'i'
#define ALL_PLANES 'j'

/* PROTOTYPES */
Uint32 rd(Uint8* data, Uint32 size);
Uint32 read_yv12(void);
Uint32 read_iyuv(void);
Uint32 read_422(void);
Uint32 allocate_memory(void);
void draw_grid422(void);
void draw_grid420(void);
void luma_only(void);
void cb_only(void);
void cr_only(void);
void draw_420(void);
void draw_422(void);
Uint32 diff_mode(void);
void calc_psnr(Uint8* frame0, Uint8* frame1);
void usage(char* name);
void mb_loop(char* str, Uint32 rows, Uint8* data, Uint32 pitch);
void show_mb(Uint32 mouse_x, Uint32 mouse_y);
void draw_frame(void);
Uint32 read_frame(void);
void setup_param(void);
void check_input(void);
Uint32 open_input(void);
Uint32 create_message_queue(void);
void destroy_message_queue(void);
Uint32 connect_message_queue(void);
Uint32 send_message(char cmd);
Uint32 read_message(void);
Uint32 event_dispatcher(void);
Uint32 event_loop(void);
Uint32 parse_input(int argc, char **argv);
Uint32 sdl_init(void);
void set_caption(char *array, Uint32 frame);
void set_zoom_rect(void);
void histogram(void);

SDL_Surface *screen;
SDL_Event event;
SDL_Rect video_rect;
SDL_Overlay *my_overlay;
const SDL_VideoInfo* info = NULL;
Uint32 FORMAT = YV12;
FILE* fd;

struct my_msgbuf {
    long mtype;
    char mtext[2];
};

struct param {
    Uint32 width;             /* frame width - in pixels */
    Uint32 height;            /* frame height - in pixels */
    Uint32 wh;                /* width x height */
    Uint32 frame_size;        /* size of 1 frame - in bytes */
    Sint32 zoom;              /* zoom-factor */
    Uint32 zoom_width;
    Uint32 zoom_height;
    Uint32 grid;              /* grid-mode - on or off */
    Uint32 hist;              /* histogram-mode - on or off */
    Uint32 grid_start_pos;
    Uint32 diff;              /* diff-mode */
    Uint32 y_start_pos;       /* start pos for first Y pel */
    Uint32 cb_start_pos;      /* start pos for first Cb pel */
    Uint32 cr_start_pos;      /* start pos for first Cr pel */
    Uint32 y_only;            /* Grayscale, i.e Luma only */
    Uint32 cb_only;           /* Only Cb plane */
    Uint32 cr_only;           /* Only Cr plane */
    Uint32 mb;                /* macroblock-mode - on or off */
    Uint32 y_size;            /* sizeof luma-data for 1 frame - in bytes */
    Uint32 cb_size;           /* sizeof croma-data for 1 frame - in bytes */
    Uint32 cr_size;           /* sizeof croma-data for 1 frame - in bytes */
    Uint8* raw;               /* pointer towards complete frame - frame_size bytes */
    Uint8* y_data;            /* pointer towards luma-data */
    Uint8* cb_data;           /* pointer towards croma-data */
    Uint8* cr_data;           /* pointer towards croma-data */
    char* filename;           /* obvious */
    char* fname_diff;         /* see above */
    Uint32 overlay_format;    /* YV12, IYUV, YUY2, UYVY or YVYU - SDL */
    Uint32 vflags;            /* HW support or SW support */
    Uint8 bpp;                /* bits per pixel */
    Uint32 mode;              /* MASTER, SLAVE or NONE - defaults to NONE */
    struct my_msgbuf buf;
    int msqid;
    key_t key;
    FILE* fd2;                /* diff file */
};

/* Global parameter struct */
struct param P;

Uint32 rd(Uint8* data, Uint32 size)
{
    Uint32 cnt;

    cnt = fread(data, sizeof(Uint8), size, fd);
    if (cnt < size) {
        fprintf(stderr, "No more data to read!\n");
        return 0;
    }
    return 1;
}

Uint32 read_yv12(void)
{
    if (!rd(P.y_data, P.y_size)) return 0;
    if (!rd(P.cb_data, P.cb_size)) return 0;
    if (!rd(P.cr_data, P.cr_size)) return 0;

    return 1;
}

Uint32 read_iyuv(void)
{
    if (!rd(P.y_data, P.y_size)) return 0;
    if (!rd(P.cb_data, P.cb_size)) return 0;
    if (!rd(P.cr_data, P.cr_size)) return 0;

    return 1;
}

Uint32 read_422(void)
{
    Uint8* y = P.y_data;
    Uint8* cb = P.cb_data;
    Uint8* cr = P.cr_data;

    if (!rd(P.raw, P.frame_size)) return 0;

    for (Uint32 i = P.y_start_pos; i < P.frame_size; i += 2) *y++ = P.raw[i];
    for (Uint32 i = P.cb_start_pos; i < P.frame_size; i += 4) *cb++ = P.raw[i];
    for (Uint32 i = P.cr_start_pos; i < P.frame_size; i += 4) *cr++ = P.raw[i];
    return 1;
}

Uint32 allocate_memory(void)
{
    P.raw = malloc(sizeof(Uint8) * P.frame_size);
    P.y_data = malloc(sizeof(Uint8) * P.y_size);
    P.cb_data = malloc(sizeof(Uint8) * P.cb_size);
    P.cr_data = malloc(sizeof(Uint8) * P.cr_size);

    if (!P.raw || !P.y_data || !P.cb_data || !P.cr_data) {
        fprintf(stderr, "Error allocating memory...\n");
        return 0;
    }
    return 1;
}

void draw_grid422(void)
{
    if (!P.grid) {
        return;
    }

    /* horizontal grid lines */
    for (Uint32 y = 0; y < P.height; y += 16) {
        for (Uint32 x = P.grid_start_pos; x < P.width * 2; x += 16) {
            *(my_overlay->pixels[0] + y * my_overlay->pitches[0] + x) = 0xF0;
            *(my_overlay->pixels[0] + y * my_overlay->pitches[0] + x + 8) = 0x20;
        }
    }
    /* vertical grid lines */
    for (Uint32 x = P.grid_start_pos; x < P.width * 2; x += 32) {
        for (Uint32 y = 0; y < P.height; y += 8) {
            *(my_overlay->pixels[0] + y * my_overlay->pitches[0] + x) = 0xF0;
            *(my_overlay->pixels[0] + (y + 4) * my_overlay->pitches[0] + x) = 0x20;
        }
    }
}

void draw_grid420(void)
{
    if (!P.grid) {
        return;
    }

    /* horizontal grid lines */
    for (Uint32 y = 0; y < P.height; y += 16) {
        for (Uint32 x = 0; x < P.width; x += 8) {
            *(my_overlay->pixels[0] + y * my_overlay->pitches[0] + x) = 0xF0;
            *(my_overlay->pixels[0] + y * my_overlay->pitches[0] + x + 4) = 0x20;
        }
    }
    /* vertical grid lines */
    for (Uint32 x = 0; x < P.width; x += 16) {
        for (Uint32 y = 0; y < P.height; y += 8) {
            *(my_overlay->pixels[0] + y * my_overlay->pitches[0] + x) = 0xF0;
            *(my_overlay->pixels[0] + (y + 4) * my_overlay->pitches[0] + x) = 0x20;
        }
    }
}

void luma_only(void)
{
    if (!P.y_only) {
        return;
    }

    if (FORMAT == YV12 || FORMAT == IYUV) {
        /* Set croma part to 0x80 */
        for (Uint32 i = 0; i < P.cr_size; i++) my_overlay->pixels[1][i] = 0x80;
        for (Uint32 i = 0; i < P.cb_size; i++) my_overlay->pixels[2][i] = 0x80;
        return;
    }

    /* YUY2, UYVY, YVYU */
    for (Uint32 i = P.cb_start_pos; i < P.frame_size; i += 4) {
        *(my_overlay->pixels[0] + i) = 0x80;
    }
    for (Uint32 i = P.cr_start_pos; i < P.frame_size; i += 4) {
        *(my_overlay->pixels[0] + i) = 0x80;
    }
}

void cb_only(void)
{
    if (!P.cb_only) {
        return;
    }

    if (FORMAT == YV12 || FORMAT == IYUV) {
        /* Set Luma part and Cr to 0x80 */
        for (Uint32 i = 0; i < P.y_size; i++) my_overlay->pixels[0][i] = 0x80;
        for (Uint32 i = 0; i < P.cr_size; i++) my_overlay->pixels[1][i] = 0x80;
        return;
    }

    /* YUY2, UYVY, YVYU */
    for (Uint32 i = P.y_start_pos; i < P.frame_size; i += 2) {
        *(my_overlay->pixels[0] + i) = 0x80;
    }
    for (Uint32 i = P.cr_start_pos; i < P.frame_size; i += 4) {
        *(my_overlay->pixels[1] + i) = 0x80;
    }
}

void cr_only(void)
{
    if (!P.cr_only) {
        return;
    }

    if (FORMAT == YV12 || FORMAT == IYUV) {
        /* Set Luma part and Cb to 0x80 */
        for (Uint32 i = 0; i < P.y_size; i++) my_overlay->pixels[0][i] = 0x80;
        for (Uint32 i = 0; i < P.cb_size; i++) my_overlay->pixels[2][i] = 0x80;
        return;
    }

    /* YUY2, UYVY, YVYU */
    for (Uint32 i = P.y_start_pos; i < P.frame_size; i += 2) {
        *(my_overlay->pixels[0] + i) = 0x80;
    }
    for (Uint32 i = P.cb_start_pos; i < P.frame_size; i += 4) {
        *(my_overlay->pixels[2] + i) = 0x80;
    }
}

void draw_420(void)
{
    memcpy(my_overlay->pixels[0], P.y_data, P.y_size);
    memcpy(my_overlay->pixels[1], P.cr_data, P.cr_size);
    memcpy(my_overlay->pixels[2], P.cb_data, P.cb_size);
    draw_grid420();
    luma_only();
    cb_only();
    cr_only();
    histogram();
}

void draw_422(void)
{
    memcpy(my_overlay->pixels[0], P.raw, P.frame_size);
    draw_grid422();
    luma_only();
    cb_only();
    cr_only();
    histogram();
}

void usage(char* name)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "%s filename width height format [diff_filename]\n", name);
}

void mb_loop(char* str, Uint32 rows, Uint8* data, Uint32 pitch)
{
    printf("%s\n", str);
    for (Uint32 i = 0; i < rows;i++) {
        for (Uint32 j = 0; j < 16; j++) {
            printf("%02X ", data[pitch+i]);
        }
        printf("\n");
    }
}

void show_mb(Uint32 mouse_x, Uint32 mouse_y)
{
    /* TODO: bad code */
    Uint32 MB;
    Uint32 pitch[5] = {64, 64, 128, 128, 128};
    Uint32 length[5] = {4, 4, 8, 8, 8};

    Uint16 y_pitch, cb_pitch, cr_pitch;

    if (!P.mb) {
        return;
    }

    /* which MB are we in? */
    MB = mouse_x / (16 * P.zoom) +
        (P.width / 16) *
        (mouse_y / (16 * P.zoom));

    y_pitch = 16 * MB;
    cb_pitch = pitch[FORMAT] * MB;
    cr_pitch = pitch[FORMAT] * MB;
    printf("\nMB #%d\n", MB);

    mb_loop("= Y =", 16, P.y_data, y_pitch);
    mb_loop("= Cb =", length[FORMAT], P.cb_data, cb_pitch);
    mb_loop("= Cr =", length[FORMAT], P.cr_data, cr_pitch);

    printf("\n");
    fflush(stdout);
}

Uint32 (*reader[5])(void) = {read_yv12, read_iyuv, read_422, read_422, read_422};
void (*drawer[5])(void) = {draw_420, draw_420, draw_422, draw_422, draw_422};

void draw_frame(void)
{
    SDL_LockYUVOverlay(my_overlay);
    (*drawer[FORMAT])();
    set_zoom_rect();
    video_rect.x = 0;
    video_rect.y = 0;
    video_rect.w = P.zoom_width;
    video_rect.h = P.zoom_height;
    SDL_UnlockYUVOverlay(my_overlay);
    SDL_DisplayYUVOverlay(my_overlay, &video_rect);
}

Uint32 read_frame(void)
{
    if (!P.diff) {
        return (*reader[FORMAT])();
    } else {
        return diff_mode();
    }
}

Uint32 diff_mode(void)
{
    FILE* fd_tmp;
    Uint8* y_tmp;

    /* Perhaps a bit ugly but it seams to work...
     * 1. read frame from fd
     * 2. store data away
     * 3. read frame from P.fd2
     * 4. calculate diff
     * 5. place result in P.raw or P.y_data depending on FORMAT
     * 6. diff works on luma data so clear P.cb_data and P.cr_data
     * Fiddle with the file descriptors so that we read
     * from correct file.
     */

    if (!(*reader[FORMAT])()) {
        return 0;
    }

    y_tmp = malloc(sizeof(Uint8) * P.y_size);

    if (!y_tmp) {
        fprintf(stderr, "Error allocating memory...\n");
        return 0;
    }

    for (Uint32 i = 0; i < P.y_size; i++) {
        y_tmp[i] = P.y_data[i];
    }

    fd_tmp = fd;
    fd = P.fd2;

    if (!(*reader[FORMAT])()) {
        free(y_tmp);
        fd = fd_tmp;
        return 0;
    }

    /* restore file descriptor */
    fd = fd_tmp;

    /* now, P.y_data contains luminance data for fd2 and
     * y_tmp contains luma data for fd.
     * Calculate diff and place result where it belongs
     * Clear croma data */

    calc_psnr(y_tmp, P.y_data);

    if (FORMAT == YV12 || FORMAT == IYUV) {
        for (Uint32 i = 0; i < P.y_size; i++) {
            P.y_data[i] = 0x80 - (y_tmp[i] - P.y_data[i]);
        }
        for (Uint32 i = 0; i < P.cb_size; i++) P.cb_data[i] = 0x80;
        for (Uint32 i = 0; i < P.cr_size; i++) P.cr_data[i] = 0x80;
    } else {
        Uint32 j = 0;
        for (Uint32 i = P.y_start_pos; i < P.frame_size; i += 2) {
            P.raw[i] = 0x80 - (y_tmp[j] - P.y_data[j]);
            j++;
        }
        for (Uint32 i = P.cb_start_pos; i < P.frame_size; i += 4)  P.raw[i] = 0x80;
        for (Uint32 i = P.cr_start_pos; i < P.frame_size; i += 4)  P.raw[i] = 0x80;
    }

    free(y_tmp);

    return 1;
}

void calc_psnr(Uint8* frame0, Uint8* frame1)
{
    double mse = 0.0;
    double mse_tmp = 0.0;
    double psnr = 0.0;

    for (Uint32 i = 0; i < P.y_size; i++) {
        mse_tmp = abs(frame0[i] - frame1[i]);
        mse += mse_tmp * mse_tmp;
    }

    /* division by zero */
    if (mse == 0) {
        fprintf(stdout, "PSNR: NaN\n");
        return;
    }

    mse /= P.y_size;

    psnr = 10.0*log10((256 * 256) / mse);

    fprintf(stdout, "PSNR: %f\n", psnr);
}

void histogram(void)
{
    if (!P.hist) {
        return;
    }

    Uint8 y[256] = {0};
    Uint8 b[256] = {0};
    Uint8 r[256] = {0};

    for (Uint32 i = 0; i < P.y_size; i++) y[P.y_data[i]]++;
    for (Uint32 i = 0; i < P.cb_size; i++) b[P.cb_data[i]]++;
    for (Uint32 i = 0; i < P.cr_size; i++) r[P.cr_data[i]]++;

    fprintf(stdout, "\nY,");
    for (Uint32 i = 0; i < 256; i++)  fprintf(stdout, "%u,", y[i]);
    fprintf(stdout, "\nCb,");
    for (Uint32 i = 0; i < 256; i++)  fprintf(stdout, "%u,", b[i]);
    fprintf(stdout, "\nCr,");
    for (Uint32 i = 0; i < 256; i++)  fprintf(stdout, "%u,", r[i]);
    fprintf(stdout, "\n");
    fflush(stdout);
}

void setup_param(void)
{
    P.zoom = 1;
    P.wh = P.width * P.height;

    if (FORMAT == YV12 || FORMAT == IYUV) {
        P.frame_size = P.wh * 3 / 2;
        P.y_size = P.wh;
        P.cb_size = P.wh / 4;
        P.cr_size = P.wh / 4;
    } else if (FORMAT == YUY2 || FORMAT == UYVY || FORMAT == YVYU) {
        P.grid_start_pos = 0;
        P.frame_size = P.wh * 2;
        P.y_size = P.wh;
        P.cb_size = P.wh / 2;
        P.cr_size = P.wh / 2;
    }

    if (FORMAT == YUY2) {
        /* Y U Y V
         * 0 1 2 3 */
        P.y_start_pos = 0;
        P.cb_start_pos = 1;
        P.cr_start_pos = 3;
    } else if (FORMAT == UYVY) {
        /* U Y V Y
         * 0 1 2 3 */
        P.y_start_pos = 1;
        P.grid_start_pos = 1;
        P.cb_start_pos = 0;
        P.cr_start_pos = 2;
    } else if (FORMAT == YVYU) {
        /* Y V Y U
         * 0 1 2 3 */
        P.y_start_pos = 0;
        P.cb_start_pos = 3;
        P.cr_start_pos = 1;
    }
}

void check_input(void)
{
    Uint32 file_size;

    /* Frame Size is an even multipe of 16x16? */
    if (P.width % 16 != 0) {
        fprintf(stderr, "WIDTH not multiple of 16, check input...\n");
    }
    if (P.height % 16 != 0) {
        fprintf(stderr, "HEIGHT not multiple of 16, check input...\n");
    }

    /* Even number of frames? */
    fseek(fd, 0L, SEEK_END);
    file_size = ftell(fd);
    fseek(fd, 0L, SEEK_SET);

    if (file_size % P.frame_size != 0) {
        fprintf(stderr, "#FRAMES not an integer, check input...\n");
    }
}

Uint32 create_message_queue(void)
{
    if ((P.key = ftok("yv.c", 'B')) == -1) {
        perror("ftok");
        return 0;
    }

    if ((P.msqid = msgget(P.key, 0644 | IPC_CREAT)) == -1) {
        perror("msgget");
        return 0;
    }

    /* we don't really care in this case */
    P.buf.mtype = 1;
    return 1;
}

Uint32 connect_message_queue(void)
{
    if ((P.key = ftok("yv.c", 'B')) == -1) {
        perror("ftok");
        return 0;
    }

    /* connect to the queue */
    if ((P.msqid = msgget(P.key, 0644)) == -1) {
        perror("msgget");
        return 0;
    }

    printf("Ready to receive messages, captain.\n");
    return 1;
}

Uint32 send_message(char cmd)
{
    if (P.mode != MASTER) {
        return 1;
    }

    P.buf.mtext[0] = cmd;
    P.buf.mtext[1] = '\0';

    if (msgsnd(P.msqid, &P.buf, 2, 0) == -1) {
        perror("msgsnd");
        return 0;
    }

    return 1;
}

Uint32 read_message(void)
{
    if (msgrcv(P.msqid, &P.buf, sizeof(P.buf.mtext), 0, 0) == -1) {
        perror("msgrcv");
        return 0;
    }

    return 1;
}

void destroy_message_queue(void)
{
    if (P.mode == MASTER) {
        if (msgctl(P.msqid, IPC_RMID, NULL) == -1) {
            perror("msgctl");
        }
        printf("queue destroyed\n");
    }
}

Uint32 event_dispatcher(void)
{
    if (read_message()) {

        switch (P.buf.mtext[0])
        {
            case NEXT:
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = SDLK_RIGHT;
                SDL_PushEvent(&event);
                break;

            case PREV:
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = SDLK_LEFT;
                SDL_PushEvent(&event);
                break;

            case REW:
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = SDLK_r;
                SDL_PushEvent(&event);
                break;

            case ZOOM_IN:
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = SDLK_UP;
                SDL_PushEvent(&event);
                break;

            case ZOOM_OUT:
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = SDLK_DOWN;
                SDL_PushEvent(&event);
                break;

            case QUIT:
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = SDLK_q;
                SDL_PushEvent(&event);
                break;

            case Y_ONLY:
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = SDLK_F5;
                SDL_PushEvent(&event);
                break;

            case CB_ONLY:
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = SDLK_F6;
                SDL_PushEvent(&event);
                break;

            case CR_ONLY:
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = SDLK_F7;
                SDL_PushEvent(&event);
                break;

            case ALL_PLANES:
                event.type = SDL_KEYDOWN;
                event.key.keysym.sym = SDLK_F8;
                SDL_PushEvent(&event);
                break;

            default:
                fprintf(stderr, "~TILT\n");
                return 0;
        }
    }
    return 1;
}

void set_caption(char *array, Uint32 frame)
{
    sprintf(array, "%s%s%s%s%s%s%s%s frame %d, size %dx%d",
            (P.mode == MASTER) ? "[MASTER]" :
            (P.mode == SLAVE) ? "[SLAVE]": "",
            P.grid ? "G" : "",
            P.mb ? "M" : "",
            P.diff ? "D" : "",
            P.hist ? "H" : "",
            P.y_only ? "Y" : "",
            P.cb_only ? "Cb" : "",
            P.cr_only ? "Cr" : "",
            frame,
            P.zoom_width,
            P.zoom_height);
}

void set_zoom_rect(void)
{
    if (P.zoom > 0) {
        P.zoom_width = P.width * P.zoom;
        P.zoom_height = P.height * P.zoom;
    } else if (P.zoom <= 0) {
        P.zoom_width = P.width / (abs(P.zoom) + 2);
        P.zoom_height = P.height / (abs(P.zoom) + 2);
    } else {
        fprintf(stderr, "ERROR in zoom:\n");
    }
}

/* loop inspired by yay
 * http://freecode.com/projects/yay
 */
Uint32 event_loop(void)
{
    char caption[64];
    Uint16 quit = 0;
    Uint32 frame = 0;
    int play_yuv = 0;
    unsigned int start_ticks = 0;

    while (!quit) {

        set_caption(caption, frame);
        SDL_WM_SetCaption(caption, NULL);

        /* wait for SDL event */
        if (P.mode == NONE || P.mode == MASTER) {
            SDL_WaitEvent(&event);
        } else if (P.mode == SLAVE) {
            if (!event_dispatcher()) {
                SDL_WaitEvent(&event);
            }
        }

        switch (event.type)
        {
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym)
                {
                    case SDLK_SPACE:
                        play_yuv = 1; /* play it, sam! */
                        while (play_yuv) {
                            start_ticks = SDL_GetTicks();
                            set_caption(caption, frame);
                            SDL_WM_SetCaption( caption, NULL );

                            /* check for next frame existing */
                            if (read_frame()) {
                                draw_frame();
                                /* insert delay for real time viewing */
                                if (SDL_GetTicks() - start_ticks < 40)
                                    SDL_Delay(40 - (SDL_GetTicks() - start_ticks));
                                frame++;
                                send_message(NEXT);
                            } else {
                                play_yuv = 0;
                            }
                            /* check for any key event */
                            if (SDL_PollEvent(&event)) {
                                if (event.type == SDL_KEYDOWN) {
                                    /* stop playing */
                                    play_yuv = 0;
                                }
                            }
                        }
                        break;
                    case SDLK_RIGHT: /* next frame */
                        /* check for next frame existing */
                        if (read_frame()) {
                            draw_frame();
                            frame++;
                            send_message(NEXT);
                        }
                        break;
                    case SDLK_LEFT: /* previous frame */
                        if (frame > 1) {
                            frame--;
                            fseek(fd, ((frame-1) * P.frame_size), SEEK_SET);
                            if (P.diff) {
                                fseek(P.fd2, ((frame-1) * P.frame_size), SEEK_SET);
                            }
                            read_frame();
                            draw_frame();
                            send_message(PREV);
                        }
                        break;
                    case SDLK_UP: /* zoom in */
                        P.zoom++;
                        set_zoom_rect();
                        screen = SDL_SetVideoMode(P.zoom_width,
                                                  P.zoom_height,
                                                  P.bpp, P.vflags);
                        video_rect.w = P.zoom_width;
                        video_rect.h = P.zoom_height;
                        SDL_DisplayYUVOverlay(my_overlay, &video_rect);
                        send_message(ZOOM_IN);
                        break;
                    case SDLK_DOWN: /* zoom out */
                        P.zoom--;
                        set_zoom_rect();
                        screen = SDL_SetVideoMode(P.zoom_width,
                                                  P.zoom_height,
                                                  P.bpp, P.vflags);
                        video_rect.w = P.zoom_width;
                        video_rect.h = P.zoom_height;
                        SDL_DisplayYUVOverlay(my_overlay, &video_rect);
                        send_message(ZOOM_OUT);
                        break;
                    case SDLK_r: /* rewind */
                        if (frame > 1) {
                            frame = 1;
                            fseek(fd, 0, SEEK_SET);
                            if (P.diff) {
                                fseek(P.fd2, 0, SEEK_SET);
                            }
                            read_frame();
                            draw_frame();
                            send_message(REW);
                        }
                        break;
                    case SDLK_g: /* display grid */
                        P.grid = ~P.grid;
                        if (P.zoom < 1)
                            P.grid = 0;
                        draw_frame();
                        break;
                    case SDLK_m: /* show mb-data on stdout */
                        P.mb = ~P.mb;
                        if (P.zoom < 1)
                            P.mb = 0;
                        draw_frame();
                        break;
                    case SDLK_F5: /* Luma data only */
                        P.y_only = ~P.y_only;
                        P.cb_only = 0;
                        P.cr_only = 0;
                        draw_frame();
                        send_message(Y_ONLY);
                        break;
                    case SDLK_F6: /* Cb data only */
                        P.cb_only = ~P.cb_only;
                        P.y_only = 0;
                        P.cr_only = 0;
                        draw_frame();
                        send_message(CB_ONLY);
                        break;
                    case SDLK_F7: /* Cr data only */
                        P.cr_only = ~P.cr_only;
                        P.y_only = 0;
                        P.cb_only = 0;
                        send_message(CR_ONLY);
                        draw_frame();
                        break;
                    case SDLK_F8: /* display all color planes */
                        P.y_only = 0;
                        P.cb_only = 0;
                        P.cr_only = 0;
                        draw_frame();
                        send_message(ALL_PLANES);
                        break;
                    case SDLK_h: /* histogram */
                        P.hist = ~P.hist;
                        draw_frame();
                        break;
                    case SDLK_F1: /* MASTER-mode */
                        create_message_queue();
                        P.mode = MASTER;
                        break;
                    case SDLK_F2: /* SLAVE-mode */
                        if (connect_message_queue()) {
                            P.mode = SLAVE;
                        }
                        break;
                    case SDLK_F3: /* NONE-mode */
                        destroy_message_queue();
                        P.mode = NONE;
                        break;
                    case SDLK_q: /* quit */
                        quit = 1;
                        send_message(QUIT);
                        break;
                    default:
                        break;
                } /* switch key */
                break;
            case SDL_QUIT:
                quit = 1;
                break;
            case SDL_VIDEOEXPOSE:
                SDL_DisplayYUVOverlay(my_overlay, &video_rect);
                break;
            case SDL_MOUSEBUTTONDOWN:
                /* If the left mouse button was pressed */
                if (event.button.button == SDL_BUTTON_LEFT ) {
                    show_mb(event.button.x, event.button.y);
                }
                break;

            default:
                break;

        } /* switch event type */

    } /* while */

    return quit;
}

Uint32 parse_input(int argc, char **argv)
{
    if (argc != 5 && argc != 6) {
        usage(argv[0]);
        return 0;
    }

    if (argc == 6) {
        /* diff mode */
        P.diff = 1;
        P.fname_diff = argv[5];
    }

    P.filename = argv[1];

    P.width = atoi(argv[2]);
    P.height = atoi(argv[3]);

    if (!strncmp(argv[4], "YV12", 4)) {
        P.overlay_format = SDL_YV12_OVERLAY;
        FORMAT = YV12;
    } else if (!strncmp(argv[4], "IYUV", 4)) {
        P.overlay_format = SDL_IYUV_OVERLAY;
        FORMAT = IYUV;
    } else if (!strncmp(argv[4], "YUY2", 4)) {
        P.overlay_format = SDL_YUY2_OVERLAY;
        FORMAT = YUY2;
    } else if (!strncmp(argv[4], "UYVY", 4)) {
        P.overlay_format = SDL_UYVY_OVERLAY;
        FORMAT = UYVY;
    } else if (!strncmp(argv[4], "YVYU", 4)) {
        P.overlay_format = SDL_YVYU_OVERLAY;
        FORMAT = YVYU;
    } else {
        fprintf(stderr, "The format option '%s' is not recognized\n", argv[4]);
        return 0;
    }

    return 1;
}

Uint32 open_input(void)
{
    fd = fopen(P.filename, "rb");
    if (fd == NULL) {
        fprintf(stderr, "Error opening %s\n", P.filename);
        return 0;
    }

    if (P.diff) {
        P.fd2 = fopen(P.fname_diff, "rb");
        if (P.fd2 == NULL) {
            fprintf(stderr, "Error opening %s\n", P.fname_diff);
            return 0;
        }
    }
    return 1;
}

Uint32 sdl_init(void)
{
    /* SDL init */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Unable to set video mode: %s\n", SDL_GetError());
        atexit(SDL_Quit);
        return 0;
    }

    info = SDL_GetVideoInfo();
    if (!info) {
        fprintf(stderr, "SDL ERROR Video query failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 0;
    }

    P.bpp = info->vfmt->BitsPerPixel;

    if (info->hw_available){
        P.vflags = SDL_HWSURFACE;
    } else {
        P.vflags = SDL_SWSURFACE;
    }

    if ((screen = SDL_SetVideoMode(P.width, P.height, P.bpp, P.vflags)) == 0) {
        fprintf(stderr, "SDL ERROR Video mode set failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 0;
    }

    my_overlay = SDL_CreateYUVOverlay(P.width, P.height, P.overlay_format, screen);
    if (!my_overlay) {
        fprintf(stderr, "Couldn't create overlay\n");
        return 0;
    }
    return 1;
}

int main(int argc, char** argv)
{
    int ret = EXIT_SUCCESS;

    /* Initialize param struct to zero */
    memset(&P, 0, sizeof(P));

    if (!parse_input(argc, argv)) {
        return EXIT_FAILURE;
    }

    /* Initialize parameters corresponding to YUV-format */
    setup_param();

    if (!sdl_init()) {
        return EXIT_FAILURE;
    }

    if (!open_input()) {
        return EXIT_FAILURE;
    }

    if (!allocate_memory()) {
        ret = EXIT_FAILURE;
        goto cleanup;
    }

    /* Lets do some basic consistency check on input */
    check_input();

    /* send event to display first frame */
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = SDLK_RIGHT;
    SDL_PushEvent(&event);

    /* while true */
    event_loop();

cleanup:
    destroy_message_queue();
    SDL_FreeYUVOverlay(my_overlay);
    free(P.raw);
    free(P.y_data);
    free(P.cb_data);
    free(P.cr_data);
    if (fd) {
        fclose(fd);
    }
    if (P.fd2) {
        fclose(P.fd2);
    }

    return ret;
}

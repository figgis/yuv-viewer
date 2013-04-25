Simple YCbCr-Viewer (YUV)
=========================

Supports the following formats:

- YV12
- IYUV
- YUVY
- UYVY
- YUY2
- YV1210

YV1210 is the same as YV12 with 10bpp.
Since SDL does not support this format, I fake it
by converting it to standard 8bpp YV12 prior to viewing.

Basically, because that's whats SDL supports.
Other YCbCr (YUV) formats are simple to add as long as
they are 4:2:0 or 4:2:2 8-bpp...

Features
--------

- Play
- Pause
- Rewind
- Single Step Forward
- Single Step Backwards
- Zoom In by a factor of 1..n
- Zoom Out by a factor of 1..n
- Display a 16x16 grid on top of a frame
- Dump Macro-Block-data to stdout for MB pointed
  to by mouse
- Only display Luma data
- Only display Cr data
- Only display Cb data
- Diff two files of the same size and format
- PSNR calculation
- Master/Slave mode that allows two instances of
  the binary to communicate using a message-queue.
  Commands issued in the Master are also executed
  in the Slave. Main usage is to single-step two clips
  side-by-side to compare them. Works regardless of
  format used
- Title reflects mode, feature used, including
  frame number and size.
- Histogram for the different color planes, per frame
  as csv-data to stdout (for now at least)

Usage
-----

    ./yv filename width height format
    ./yv foreman_cif.yuv 352 288 YV12

To use MASTER/SLAVE, type the following
command in two different shells or send them to
the background using a `&` at the end:

    ./yv foreman_1.yuv 352 288 YV12
    ./yv foreman_2.yuv 352 288 YV12

In the first window, press F1 (title should be updated
to show the mode. In the second window, press F2
(title should be updated to show the mode).
Commands given in window1 should be executed in window2.

To display diff between two files of the same size
and format, just add file as the last argument
(computes and displays differences in luma value only,
PSNR value is written to stdout):

    ./yv filename width height format diff_file
    ./yv foreman_cif.yuv 352 288 YV12 foreman_filtered_cif.yuv

Supported commands
------------------

    SPACE - Play clip
    RIGHT - Single step 1 frame forward
    LEFT - Single step 1 frame backward
    r - Rewind
    UP - Zoom in
    DOWN - Zoom out
    g - Enable grid-mode
    m - Enable MB-mode, point and click to
        print MB-data to stdout
    h - histogram, 1 per color plane
    F5 - Toggle viewing of Luma data only
    F6 - Toggle viewing of Cb data only
    F7 - Toggle viewing of Cr data only
    F8 - Display all color-planes
    q - Quit
    F1 - MASTER-mode
    F2 - SLAVE-mode
    F3 - NONE-mode, i.e. disable MASTER/SLAVE-mode

Disclaimer
----------

Only verified on a Linux based system...

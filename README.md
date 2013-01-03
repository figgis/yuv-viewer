Simple YCbCr-Viewer (YUV)
=========================

Supports the following formats:

- YV12
- IYUV
- YUVY
- UYVY
- YUY2

Basically, because that's whats SDL supports.
Other YCbCr (YUV) formats are simple to add as long as
they are 4:2:0 or 4:2:2 8-bpp...

Features:
---------

- Play
- Pause
- Rewind
- Single Step Forward
- Single Step Backwards
- Zoom In
- Zoom Out
- Displaya 16x16 grid on top of a frame
- Dump Macro-Block-data to stdout for MB pointed
  to by mouse
- Only display Luma data
- Master/Slave mode that allows two instances of
  the binary to communicate using a message-queue.
  Commands issued in the Master are also executed
  in the Slave. Main usage is to single-step two clips
  side-by-side to compare them. Works regardless of
  format used

Basic Usage
-----------

    ./yv filename width height format
    ./yv foreman_cif.yuv 352 288 YV12

To use MASTER/SLAVE, type the following
command in two different shells or send them to
the background using a '&' at the end:

    ./yv foreman_1.yuv 352 288 YV12
    ./yv foreman_2.yuv 352 288 YV12

In the first window, press F1 (title should be updated
to show the mode. In the second window, press F2
(title should be updated to show the mode).
Commands given in window1 should be executed in window2.

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

    b - Only display Luma data

    q - Quit

    F1 - MASTER-mode

    F2 - SLAVE-mode

    F3 - NONE-mode, i.e. disable MASTER/SLAVE-mode

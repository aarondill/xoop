/*
Copyright (c) 2020, Matt Colligan All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <xcb/shape.h>
#include <xcb/xcb.h>
#include <unistd.h>

#define PROGNAME "xoop"

xcb_connection_t    *conn;
xcb_screen_t	    *screen;
xcb_window_t	     wid;


void set_window_type() {
    xcb_intern_atom_reply_t *wm_window_type,
			    *wm_window_type_dock,
			    *wm_state,
			    *wm_state_above,
			    *wm_desktop;

    wm_window_type = xcb_intern_atom_reply(
	conn,
	xcb_intern_atom(conn, 0, 19, "_NET_WM_WINDOW_TYPE"),
	NULL
    );
    wm_window_type_dock = xcb_intern_atom_reply(
	conn,
	xcb_intern_atom(conn, 0, 24, "_NET_WM_WINDOW_TYPE_DOCK"),
	NULL
    );
    xcb_change_property(
	conn, XCB_PROP_MODE_REPLACE, wid, wm_window_type->atom, XCB_ATOM_ATOM, 32, 1, &wm_window_type_dock->atom
    );

    wm_state = xcb_intern_atom_reply(
	conn,
	xcb_intern_atom(conn, 0, 13, "_NET_WM_STATE"),
	NULL
    );
    wm_state_above = xcb_intern_atom_reply(
	conn,
	xcb_intern_atom(conn, 0, 19, "_NET_WM_STATE_ABOVE"),
	NULL
    );
    xcb_change_property(
	conn, XCB_PROP_MODE_REPLACE, wid, wm_state->atom, XCB_ATOM_ATOM, 32, 1, &wm_state_above->atom
    );

    wm_desktop = xcb_intern_atom_reply(
        conn,
        xcb_intern_atom(conn, 0, 15, "_NET_WM_DESKTOP"),
        NULL
    );
    xcb_change_property(
        conn, XCB_PROP_MODE_REPLACE, wid, wm_desktop->atom, XCB_ATOM_INTEGER, 1, 1, (uint32_t []){0xFFFFFFFF}
    );

    xcb_configure_window(conn, wid, XCB_CONFIG_WINDOW_STACK_MODE, (uint32_t []){XCB_STACK_MODE_ABOVE});

    xcb_change_property(
	conn, XCB_PROP_MODE_REPLACE, wid, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, 4, PROGNAME
    );

    free(wm_window_type);
    free(wm_window_type_dock);
    free(wm_desktop);
}


void set_window_shape()
{
    xcb_pixmap_t pixmap = xcb_generate_id(conn);
    xcb_gcontext_t gc = xcb_generate_id(conn);
    xcb_create_pixmap(
	conn, 1, pixmap, wid, screen->width_in_pixels, screen->height_in_pixels
    );
    xcb_create_gc(
	conn, gc, pixmap, XCB_GC_FOREGROUND, &screen->white_pixel
    );
    xcb_rectangle_t rect = {
	0, 0, screen->width_in_pixels, screen->height_in_pixels
    };
    xcb_poly_fill_rectangle(
	conn, pixmap, gc, 1, &rect
    );
    xcb_shape_mask(
	conn, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_INPUT, wid, 0, 0, pixmap
    );

    xcb_pixmap_t pixmap2 = xcb_generate_id(conn);
    xcb_gcontext_t gc2 = xcb_generate_id(conn);
    xcb_create_pixmap(
	conn, 1, pixmap2, wid, screen->width_in_pixels, screen->height_in_pixels
    );
    xcb_create_gc(
	conn, gc2, pixmap2, XCB_GC_FOREGROUND, &screen->white_pixel
    );
    xcb_rectangle_t rect2 = {
	1, 1, screen->width_in_pixels - 2, screen->height_in_pixels - 2
    };
    xcb_poly_fill_rectangle(
	conn, pixmap2, gc2, 1, &rect2
    );
    xcb_shape_mask(
	conn, XCB_SHAPE_SO_SUBTRACT, XCB_SHAPE_SK_INPUT, wid, 0, 0, pixmap2
    );

#ifdef DEBUG
    xcb_shape_mask(
	conn, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING, wid, 0, 0, pixmap
    );
    xcb_shape_mask(
	conn, XCB_SHAPE_SO_SUBTRACT, XCB_SHAPE_SK_BOUNDING, wid, 0, 0, pixmap2
    );
#endif

    xcb_free_pixmap(conn, pixmap);
    xcb_free_pixmap(conn, pixmap2);
    xcb_free_gc(conn, gc);
    xcb_free_gc(conn, gc2);
}


void setup_window()
{
    wid = xcb_generate_id(conn);

    xcb_create_window(
	conn,
	XCB_COPY_FROM_PARENT,
	wid,
	screen->root,
	0,
	0,
	screen->width_in_pixels,
	screen->height_in_pixels,
	0,
#ifdef DEBUG
	XCB_WINDOW_CLASS_INPUT_OUTPUT,
	XCB_COPY_FROM_PARENT,
	XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
	(uint32_t []){screen->white_pixel, XCB_EVENT_MASK_ENTER_WINDOW}
#else
	XCB_WINDOW_CLASS_INPUT_ONLY,
	XCB_COPY_FROM_PARENT,
	XCB_CW_EVENT_MASK,
	(uint32_t []){XCB_EVENT_MASK_ENTER_WINDOW}
#endif
    );

    set_window_type();
    set_window_shape();

    xcb_map_window(conn, wid);
    xcb_flush(conn);
}


void event_loop()
{
    xcb_generic_event_t *event;
    xcb_enter_notify_event_t *entry;
    int16_t x = 0;
    int16_t y = 0;

    int16_t far_x = screen->width_in_pixels - 1;
    int16_t far_y = screen->height_in_pixels - 1;

    while((event = xcb_wait_for_event(conn))) {
	switch (event->response_type) {

	    case XCB_ENTER_NOTIFY:
	    entry = (xcb_enter_notify_event_t *)event;

#ifdef DEBUG
	    printf("Entry: %d, %d\n", entry->event_x, entry->event_y);
#endif
	    if (entry->event_x == 0) {
		x = far_x;
		y = entry->event_y;
	    } else if (entry->event_y == 0){
		x = entry->event_x;
		y = far_y;
	    } else if (entry->event_x == far_x){
		x = 0;
		y = entry->event_y;
	    } else if (entry->event_y == far_y){
		x = entry->event_x;
		y = 0;
	    };
	    xcb_warp_pointer(
		conn, XCB_NONE, screen->root, 0, 0, 0, 0, x, y
	    );
	    xcb_flush(conn);
#ifdef DEBUG
	    printf("Warp: (%d, %d) to (%d, %d)\n", entry->event_x, entry->event_y, x, y);
#endif
	    break;
	}

    }
    free(event);
    free(entry);
}


void exit_nicely()
{
    xcb_unmap_window(conn, wid);
    xcb_disconnect(conn);
    exit(EXIT_SUCCESS);
}


void print_help()
{
    printf(
	PROGNAME " [-h|-f]\n"
	"\n"
	"    -h    print help\n"
	"    -f    fork\n"
    );
}


int main(int argc, char *argv[])
{
    int opt;
    int to_fork = 0;
    int pid;

    while ((opt = getopt(argc, argv, "hf")) != -1) {
        switch (opt) {
	    case 'h':
		print_help();
		exit(EXIT_SUCCESS);
	    case 'f':
		to_fork = 1;
		break;
        }
    }

    conn = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(conn))
	exit(EXIT_FAILURE);

    screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
    setup_window();

    signal(SIGINT, exit_nicely);
    signal(SIGTERM, exit_nicely);
    signal(SIGHUP, exit_nicely);

    if (to_fork) {
	pid = fork();
	if (pid > 0) {
	    exit(EXIT_SUCCESS);
	} else if (pid < 0) {
	    exit(EXIT_FAILURE);
	}
    }

    event_loop();

    exit_nicely(EXIT_SUCCESS);
    return 0;
}

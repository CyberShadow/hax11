#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>

#include <xcb/xcb.h>
#include <xcb/randr.h>

// Specify the X11 coordinates of your 4K monitor here.
#define TARGET_X 0
#define TARGET_Y 0


xcb_randr_get_crtc_info_reply_t *
xcb_randr_get_crtc_info_reply (xcb_connection_t                  *c  /**< */,
                               xcb_randr_get_crtc_info_cookie_t   cookie  /**< */,
                               xcb_generic_error_t              **e  /**< */) {
	static void* handle = NULL;
	if (!handle) {
		handle = dlopen("/usr/lib/libxcb-randr.so.0", RTLD_LAZY);
	}
	if (!handle) {
		fprintf(stderr, "4khack: Ack, dlopen failed!\n");
		return NULL;
	}

	xcb_randr_get_crtc_info_reply_t* (*original_fun)(xcb_connection_t *c, xcb_randr_get_crtc_info_cookie_t   cookie, xcb_generic_error_t **e);

	original_fun = dlsym(/*RTLD_NEXT*/handle, "xcb_randr_get_crtc_info_reply");
	if (!original_fun) {
		fprintf(stderr, "4khack: Ack, dlsym failed!\n");
		return NULL;
	}

	xcb_randr_get_crtc_info_reply_t* real = original_fun(c, cookie, e);

	if (!real)
		return real;

	if (real->width == 1920 && real->height == 2160) { // Is 4K MST panel?
		if (real->x == TARGET_X) { // Left panel
			real->width = 1920 * 2; // resize
			//real->height = 2160;
		}
		else
		if (real->x == TARGET_X + 1920) { // Right panel
			real->x = real->y = real->width = real->height = 0; // disable
		}
	}

	return real;
}

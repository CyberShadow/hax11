#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>

#include <xcb/xcb.h>
#include <xcb/randr.h>



xcb_randr_get_crtc_info_reply_t *
xcb_randr_get_crtc_info_reply (xcb_connection_t                  *c  /**< */,
                               xcb_randr_get_crtc_info_cookie_t   cookie  /**< */,
                               xcb_generic_error_t              **e  /**< */) {
	xcb_randr_get_crtc_info_reply_t* (*original_fun)(xcb_connection_t *c, xcb_randr_get_crtc_info_cookie_t   cookie, xcb_generic_error_t **e);

	original_fun = dlsym(RTLD_NEXT, "xcb_randr_get_crtc_info_reply");

	xcb_randr_get_crtc_info_reply_t* real = original_fun(c, cookie, e);

	if (real == 0)
		return real;

	if (real->x == 0 && real->width == 1920) {
		real->width = 1920 * 2;
		real->height = 2160;
	}

	if (real->x == 1920) {
		real->x = 0;
		real->y = 0;
		real->width = 0;
		real->height = 0;
	}

	return real;
}

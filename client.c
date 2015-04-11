/*
  This is a simple client to test that the hooks work
 */

#include <xcb/bigreq.h>
#include <xcb/randr.h>
#include <xcb/xcb_aux.h>
#include <xcb/xcb_event.h>
#include <xcb/xinerama.h>
#include <xcb/xtest.h>
#include <xcb/shape.h>

#include <stdio.h>

xcb_connection_t *connection;
int default_screen;
xcb_screen_t *screen;

int main() {
	connection = xcb_connect(":0", &default_screen);
	if (xcb_connection_has_error(connection)) {
		printf("Error during connection, can't open display :0\n");
		return 1;
	}

	printf("Connection established, screen#%i\n",
	       default_screen);

	// TODO: might not be needed
	screen = xcb_aux_get_screen(connection, default_screen);
	xcb_prefetch_extension_data(connection, &xcb_big_requests_id);
	xcb_prefetch_extension_data(connection, &xcb_test_id);
	xcb_prefetch_extension_data(connection, &xcb_randr_id);
	xcb_prefetch_extension_data(connection, &xcb_xinerama_id);
	xcb_prefetch_extension_data(connection, &xcb_shape_id);

	// Getting screen count
	xcb_randr_get_screen_resources_cookie_t
		screen_res_c = xcb_randr_get_screen_resources(connection,
		                                              screen->root);
	xcb_randr_get_screen_resources_reply_t*
		screen_res_r = xcb_randr_get_screen_resources_reply(connection,
		                                                    screen_res_c,
		                                                    NULL);

	printf("Num crts: %i\n", screen_res_r->num_crtcs);

	// Getting geometry
	for (int i = 0; i < screen_res_r->num_crtcs; ++i) {
		xcb_randr_crtc_t *randr_crtcs = xcb_randr_get_screen_resources_crtcs(screen_res_r);
		xcb_randr_get_crtc_info_cookie_t
			crtc_info_c = xcb_randr_get_crtc_info(connection,
			                                      randr_crtcs[i],
			                                      XCB_CURRENT_TIME);
		xcb_randr_get_crtc_info_reply_t*
			crtc_info_r = xcb_randr_get_crtc_info_reply(connection,
			                                            crtc_info_c,
			                                            NULL);
		printf("CRTCS %i, x: %i, y: %i, w: %i, h: %i\n",
		       i,
		       crtc_info_r->x,
		       crtc_info_r->y,
		       crtc_info_r->width,
		       crtc_info_r->height);
	}

	return 0;
}

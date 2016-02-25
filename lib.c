#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdarg.h>

#include <xcb/xcb.h>
#include <xcb/randr.h>

// Specify the X11 coordinates of your 4K monitor here.
#define TARGET_X 1920
#define TARGET_Y 0

static void log_error(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	vfprintf(stderr, fmt, args);

	FILE* f = fopen("/tmp/mst4khack.log", "ab");
	if (f)
	{
		vfprintf(f, fmt, args);
		fclose(f);
	}

	va_end(args);
}

#ifdef DEBUG
#define log_debug log_error
#else
#define log_debug(...) do{}while(0)
#endif

#define NEXT(handle, path, func) ({								\
			static typeof(&func) pfunc = NULL;					\
			if (!pfunc)											\
				pfunc = (typeof(&func))dlsym(RTLD_NEXT, #func);	\
			if (!pfunc) {										\
				if (!handle) {									\
					handle = dlopen(path, RTLD_LAZY);			\
				}												\
				if (!handle) {									\
					log_error("4khack: Ack, dlopen failed!\n");	\
				}												\
				pfunc = (typeof(&func))dlsym(handle, #func);	\
			}													\
			if (!pfunc)											\
				log_error("4khack: Ack, dlsym failed!\n");		\
			pfunc;												\
		})

xcb_randr_get_crtc_info_reply_t *
xcb_randr_get_crtc_info_reply (xcb_connection_t                  *c  /**< */,
                               xcb_randr_get_crtc_info_cookie_t   cookie  /**< */,
                               xcb_generic_error_t              **e  /**< */) {
	static void* handle = NULL;
	xcb_randr_get_crtc_info_reply_t* real =
		NEXT(handle, "/usr/$LIB/libxcb-randr.so.0", xcb_randr_get_crtc_info_reply)
		(c, cookie, e);

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
			real->mode = real->rotation = real->num_outputs = 0;
		}
	} else {
#if 0
		real->x = real->y = real->width = real->height = 0; // disable
		real->mode = real->rotation = real->num_outputs = 0;
#endif
	}

	return real;
}

static void fixSize(unsigned int* width, unsigned int* height)
{
	if (*width == 3840 + TARGET_X)
		*width = 3840;
}

static void fixCoords(int* x, int* y, unsigned int *width, unsigned int *height)
{
	fixSize(width, height);
	if (*width == 3840 && *height == 2160)
	{
		*x = TARGET_X;
		*y = TARGET_Y;
	}
}

#include <X11/Xlib.h>

static void* xlib = NULL;

/*
  Fix for games that create the window of the wrong size or on the wrong monitor.
*/
Window XCreateWindow(display, parent, x, y, width, height, border_width, depth,
	class, visual, valuemask, attributes)

	 Display *display;
	 Window parent;
	 int x, y;
	 unsigned int width, height;
	 unsigned int border_width;
	 int depth;
	 unsigned int class;
	 Visual *visual;
	 unsigned long valuemask;
	 XSetWindowAttributes *attributes;
{
	log_debug("XCreateWindow(%d,%d,%d,%d)\n", x, y, width, height);
	fixCoords(&x, &y, &width, &height);
	log_debug(" -> (%d,%d,%d,%d)\n", x, y, width, height);
	return NEXT(xlib, "/usr/$LIB/libX11.so.6", XCreateWindow)
		(display, parent, x, y, width, height, border_width, depth,
			class, visual, valuemask, attributes);
}

/* ditto */
Window XCreateSimpleWindow(display, parent, x, y, width, height, border_width,
	border, background)
	 Display *display;
	 Window parent;
	 int x, y;
	 unsigned int width, height;
	 unsigned int border_width;
	 unsigned long border;
	 unsigned long background;
{
	log_debug("XCreateSimpleWindow(%d,%d,%d,%d)\n", x, y, width, height);
	fixCoords(&x, &y, &width, &height);
	log_debug(" -> (%d,%d,%d,%d)\n", x, y, width, height);
	return NEXT(xlib, "/usr/$LIB/libX11.so.6", XCreateSimpleWindow)
		(display, parent, x, y, width, height, border_width, border, background);
}

/*
  Fix for games setting their window size based on the total display size
  (which can encompass multiple physical monitors).
*/
Status XGetGeometry(display, d, root_return, x_return, y_return, width_return,
                      height_return, border_width_return, depth_return)
	 Display *display;
	 Drawable d;
	 Window *root_return;
	 int *x_return, *y_return;
	 unsigned int *width_return, *height_return;
	 unsigned int *border_width_return;
	 unsigned int *depth_return;
{
	Status result = NEXT(xlib, "/usr/$LIB/libX11.so.6", XGetGeometry)
		(display, d, root_return, x_return, y_return, width_return,
			height_return, border_width_return, depth_return);
	if (result)
	{
		log_debug("XGetGeometry(%d,%d,%d,%d)\n",
			*x_return, *y_return, *width_return, *height_return);
		fixSize(width_return, height_return);
	}
	return result;
}

/*
  Fix for games setting their window size based on the X root window size
  (which can encompass multiple physical monitors).
*/
Status XGetWindowAttributes(display, w, window_attributes_return)
	 Display *display;
	 Window w;
	 XWindowAttributes *window_attributes_return;
{
	Status result = NEXT(xlib, "/usr/$LIB/libX11.so.6", XGetWindowAttributes)
		(display, w, window_attributes_return);
	if (result)
	{
		log_debug("XGetWindowAttributes(%d,%d,%d,%d)\n",
			window_attributes_return->x, window_attributes_return->y,
			window_attributes_return->width, window_attributes_return->height);
		fixSize(
			(unsigned int*)&window_attributes_return->width,
			(unsigned int*)&window_attributes_return->height);
	}
	return result;
}

static void* sdl = NULL;

struct SDL_Surface;

struct SDL_Surface* SDL_SetVideoMode(int width, int height, int bpp, unsigned int flags)
{
	log_debug("SDL_SetVideomode(%d, %d)\n", width, height);
#if 0
	fixSize((unsigned int*)&width, (unsigned int*)&height);
#endif
	return NEXT(sdl, "/usr/lib/libSDL-1.2.so.0", SDL_SetVideoMode)
		(width, height, bpp, flags);
}

typedef struct{ int16_t x, y; uint16_t w, h; } SDL_Rect;
struct SDL_PixelFormat;

SDL_Rect** SDL_ListModes(struct SDL_PixelFormat* format, uint32_t flags)
{
	SDL_Rect** result = NEXT(sdl, "/usr/lib/libSDL-1.2.so.0", SDL_ListModes)
		(format, flags);

	/* Check if there are any modes available */
	if (result == (SDL_Rect**)0)
		log_debug("SDL_ListModes: No modes available\n");
	else
	if (result == (SDL_Rect**)-1)
		log_debug("SDL_ListModes: All modes available\n");
	else
	{
		for (int i=0; result[i]; ++i)
		{
			log_debug("SDL_ListModes: %d x %d\n", result[i]->w, result[i]->h);

#if 0
			unsigned int w = result[i]->w;
			unsigned int h = result[i]->h;
			fixSize(&w, &h);
			if (w != result[i]->w || h != result[i]->h)
				log_debug(" -> %d x %d\n", w, h);
			result[i]->w = w;
			result[i]->h = h;
#endif
		}
	}

	return result;
}

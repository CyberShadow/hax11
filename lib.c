#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <xcb/xcb.h>
#include <xcb/randr.h>

// ****************************************************************************

static void log_error(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "mst4khack: ");
	vfprintf(stderr, fmt, args);
	va_end(args);

	FILE* f = fopen("/tmp/mst4khack.log", "ab");
	if (f)
	{
		va_start(args, fmt);
		vfprintf(f, fmt, args);
		va_end(args);
		fclose(f);
	}
}

#define log_debug(...) do { if (config.debug) log_error(__VA_ARGS__); } while(0)

// ****************************************************************************

struct Config
{
	int mainX;
	int mainY;
	unsigned int mainW;
	unsigned int mainH;
	unsigned int desktopW;
	unsigned int desktopH;

	char enable;
	char debug;
	char joinMST;
	char maskOtherMonitors;
	char resizeWindows;
	char resizeAll;
	char moveWindows;
};

static struct Config config = {};
static char configLoaded = 0;

static void readConfig(const char* fn)
{
	//log_debug("Reading config from %s\n", fn);
	FILE* f = fopen(fn, "r");
	if (!f)
	{
		// Create empty file if it does not exist
		f = fopen(fn, "w");
		if (f) fclose(f);
		return;
	}

	while (!feof(f))
	{
		char buf[1024];
		if (!fgets(buf, sizeof(buf), f))
			break;
		if (buf[0] == '#')
			continue;
		//log_debug("Got line: %s'\n", buf);
		char *p = strchr(buf, '=');
		if (!p)
			continue;
		*p = 0;
		p++;
		//log_debug("Got line: '%s' = '%s'\n", buf, p);

		#define PARSE_INT(x)						\
			if (!strcasecmp(buf, #x))				\
				config.x = atoi(p);					\
			else

		PARSE_INT(mainX)
		PARSE_INT(mainY)
		PARSE_INT(mainW)
		PARSE_INT(mainH)
		PARSE_INT(desktopW)
		PARSE_INT(desktopH)
		PARSE_INT(enable)
		PARSE_INT(debug)
		PARSE_INT(joinMST)
		PARSE_INT(maskOtherMonitors)
		PARSE_INT(resizeWindows)
		PARSE_INT(resizeAll)
		PARSE_INT(moveWindows)
			log_error("Unknown option: %s\n", buf);
	}
	fclose(f);
	//log_debug("Read config: %d %d %d %d\n", config.joinMST, config.maskOtherMonitors, config.resizeWindows, config.moveWindows);
}

// ****************************************************************************

static void needConfig()
{
	if (configLoaded)
		return;
	configLoaded = 1;

	// Default settings
	config.mainX = 1920;
	config.mainY = 0;
	config.mainW = 3840;
	config.mainH = 2160;
	config.desktopW = 3840+1920;
	config.desktopH = 2160;

	char buf[1024] = {0};
	strncpy(buf, getenv("HOME"), sizeof(buf)-100);
	strcat(buf, "/.config"	); mkdir(buf, 0700); // TODO: XDG_CONFIG_HOME
	strcat(buf, "/mst4khack"); mkdir(buf, 0700);
	strcat(buf, "/profiles"	); mkdir(buf, 0700);
	char *p = buf + strlen(buf);

	strcpy(p, "/default");
	readConfig(buf);

	readlink("/proc/self/exe", p, sizeof(buf) - (p-buf));
	p++;
	for (; *p; p++)
		if (*p == '/')
			*p = '\\';
	readConfig(buf);
}

// ****************************************************************************

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

	needConfig();

	log_debug("xcb_randr_get_crtc_info_reply(%d, %d)\n", real->width, real->height);
	if (real->width == 1920 && real->height == 2160) { // Is 4K MST panel?
		if (config.joinMST) {
			if (real->x == config.mainX) { // Left panel
				real->width = 1920 * 2; // resize
				//real->height = 2160;
			}
			else
			if (real->x == config.mainX + 1920) { // Right panel
				real->x = real->y = real->width = real->height = 0; // disable
				real->mode = real->rotation = real->num_outputs = 0;
			}
		}
	} else {
		if (config.maskOtherMonitors) {
			real->x = real->y = real->width = real->height = 0; // disable
			real->mode = real->rotation = real->num_outputs = 0;
		}
	}
	log_debug(" -> (%d, %d)\n", real->width, real->height);

	return real;
}

static void fixSize(
	unsigned int* width,
	unsigned int* height)
{
	needConfig();
	if (!config.resizeWindows)
		return;

	if (config.resizeAll && *width >= 640 && *height >= 480)
	{
		*width = config.mainW;
		*height = config.mainH;
	}

	// Fix windows spanning multiple monitors
	if (*width == config.desktopW)
		*width = config.mainW;

	// Fix spanning one half of a MST monitor
	if (*width == 1920 && *height == 2160)
		*width = 3840;
}

static void fixCoords(int* x, int* y, unsigned int *width, unsigned int *height)
{
	fixSize(width, height);

	if (!config.moveWindows)
		return;

	if (*width == config.mainW && *height == config.mainH)
	{
		*x = config.mainX;
		*y = config.mainY;
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

#if 0
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
#endif

#include <X11/extensions/xf86vmode.h>

Bool XF86VidModeGetModeLine(
    Display *display,
    int screen,
    int *dotclock_return,
    XF86VidModeModeLine *modeline)
{
	Bool result = NEXT(xlib, "/usr/$LIB/libX11.so.6", XF86VidModeGetModeLine)
		(display, screen, dotclock_return, modeline);
	if (result)
	{
		log_debug("XF86VidModeGetModeLine: %d x %d\n", modeline->hdisplay, modeline->vdisplay);
		unsigned int w = modeline->hdisplay;
		unsigned int h = modeline->vdisplay;
		fixSize(&w, &h);
		if (w != modeline->hdisplay || h != modeline->vdisplay)
			log_debug(" -> %d x %d\n", w, h);
		modeline->hdisplay = w;
		modeline->vdisplay = h;
	}
	return result;
}

#if 0

typedef struct
{
	int server, client;
} X11ConnData;

#include <sys/socket.h>

static void* x11connThreadReadProc(void* dataPtr)
{
	X11ConnData* data = (X11ConnData*)dataPtr;
	while (1)
	{
		static char buf[1024];
		int len = recv(data->client, buf, sizeof(buf), 0);
		if (len > 0)
			send(data->server, buf, len, 0);
		else
			break;
	}
	close(data->server);
	return NULL;
}

static void* x11connThreadWriteProc(void* dataPtr)
{
	X11ConnData* data = (X11ConnData*)dataPtr;
	while (1)
	{
		static char buf[1024];
		int len = recv(data->server, buf, sizeof(buf), 0);
		if (len > 0)
			send(data->client, buf, len, 0);
		else
			break;
	}
	close(data->client);
	return NULL;
}

static void* libc = NULL;

#include <sys/un.h>
#include <pthread.h>

int connect(int socket, const struct sockaddr *address,
	socklen_t address_len)
{
	int result = NEXT(libc, "/usr/lib/libc.so", connect)
		(socket, address, address_len);
	if (result == 0)
	{
		if (address->sa_family == AF_UNIX)
		{
			struct sockaddr_un *ua = (struct sockaddr_un*)address;
			const char* path = ua->sun_path;
			if (!*path)
				path++;
			needConfig();
			log_debug("connect(%s,%d)\n", path, address_len);
			if (!strncmp(path, "/tmp/.X11-unix/X", 16))
			{
				log_debug("Found X connection!\n");
				if (config.enable)
				{
					log_debug("Intercepting X connection!\n");
					int pair[2];
					socketpair(AF_UNIX, SOCK_STREAM, 0, pair);

					X11ConnData* data = malloc(sizeof(X11ConnData));
					data->server = dup(socket);
					data->client = pair[0];
					dup2(pair[1], socket);
					close(pair[1]);

					pthread_t thread;
					pthread_create(&thread, NULL, x11connThreadReadProc, data);
					pthread_create(&thread, NULL, x11connThreadWriteProc, data);
				}
			}
		}
	}
	return result;
}

#if 0

static void handleXSend(void* data, size_t len)
{
	(void)data;(void)len;
	if (len < 4 || len % 4 != 0)
	{
		log_debug("Ack! Invalid request length (%d)\n", len);
		return;
	}
	unsigned short hdrLen = 4 * *(unsigned short*)(data+1);
	if (hdrLen != len)
	{
		log_debug("Ack! Mismatching packet length (%d != %d)\n", hdrLen, len);
		return;
	}
}

static void handleXRecv(void* data, size_t len)
{
	(void)data;(void)len;
}

ssize_t read(int fildes, void *buf, size_t nbyte)
{
	ssize_t result = NEXT(libc, "/usr/lib/libc.so", read)
		(fildes, buf, nbyte);
	if (result > 0 && x11sock && fildes == x11sock)
	{
		log_debug("x11sock read! (%d bytes)\n", result);
		handleXRecv(buf, result);
	}
	return result;
}

ssize_t recv(int socket, void *buffer, size_t length, int flags)
{
	ssize_t result = NEXT(libc, "/usr/lib/libc.so", recv)
		(socket, buffer, length, flags);
	if (result > 0 && x11sock && socket == x11sock)
	{
		log_debug("x11sock recv! (%d bytes)\n", result);
		handleXRecv(buffer, result);
	}
	return result;
}

ssize_t recvmsg(int socket, struct msghdr *message, int flags)
{
	ssize_t result = NEXT(libc, "/usr/lib/libc.so", recvmsg)
		(socket, message, flags);
	if (result > 0 && x11sock && socket == x11sock)
	{
		log_debug("x11sock recvmsg! (%d bytes)\n", result);
		if (message->msg_iovlen != 1)
			log_debug("Ack! Bad msg_iovlen (%d)!\n", message->msg_iovlen);
		handleXRecv(message->msg_iov[0].iov_base, message->msg_iov[0].iov_len);
	}
	return result;
}

ssize_t send(int socket, const void *buffer, size_t length, int flags)
{
	if (x11sock && socket == x11sock)
		handleXSend((void*)buffer, length);
	ssize_t result = NEXT(libc, "/usr/lib/libc.so", send)
		(socket, buffer, length, flags);
	if (result > 0 && x11sock && socket == x11sock)
	{
		log_debug("x11sock send! (%d bytes)\n", result);
		if ((size_t)result != length)
			log_debug("Ack! result != length\n");
	}
	return result;
}

ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
	if (x11sock && sockfd == x11sock)
	{
		if (msg->msg_iovlen != 1)
			log_debug("Ack! Bad msg_iovlen (%d)!\n", msg->msg_iovlen);
		else
			handleXSend(msg->msg_iov[0].iov_base, msg->msg_iov[0].iov_len);
	}

	ssize_t result = NEXT(libc, "/usr/lib/libc.so", sendmsg)
		(sockfd, msg, flags);
	if (result > 0 && x11sock && sockfd == x11sock)
	{
		log_debug("x11sock sendmsg! (%d bytes)\n", result);
		if (msg->msg_iovlen == 1 && msg->msg_iov[0].iov_len != (size_t)result)
			log_debug("Ack! result != length\n");
	}
	return result;
}

ssize_t write(int fildes, const void *buf, size_t nbyte)
{
	if (x11sock && fildes == x11sock)
		handleXSend((void*)buf, nbyte);
	ssize_t result = NEXT(libc, "/usr/lib/libc.so", write)
		(fildes, buf, nbyte);
	if (result > 0 && x11sock && fildes == x11sock)
	{
		log_debug("x11sock write! (%d bytes)\n", result);
		if ((size_t)result != nbyte)
			log_debug("Ack! result != length\n");
	}
	return result;
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
{
	if (x11sock && fd == x11sock)
	{
		for (int i=0; i<iovcnt; i++)
			handleXSend(iov[i].iov_base, iov[i].iov_len);
	}
	ssize_t result = NEXT(libc, "/usr/lib/libc.so", writev)
		(fd, iov, iovcnt);
	if (result > 0 && x11sock && fd == x11sock)
	{
		log_debug("x11sock writev! (%d iovs):\n", iovcnt);
		for (int i=0; i<iovcnt; i++)
			log_debug("\t%d\n", iov[i].iov_len);
		/* for (ssize_t i=0; i<result; i++) */
		/* 	 log_debug("%02X ", ((char*)buffer)[i]); */
		/* log_debug("\n"); */
	}
	return result;
}
#endif
#endif

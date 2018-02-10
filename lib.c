#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <X11/Xproto.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/xf86vmode.h>
#include <X11/extensions/xf86vmproto.h>
#include <X11/extensions/randr.h>
#include <X11/extensions/randrproto.h>
#include <X11/extensions/panoramiXproto.h>

// ****************************************************************************

static void log_error(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "hax11: ");
	vfprintf(stderr, fmt, args);
	va_end(args);

	FILE* f = fopen("/tmp/hax11.log", "ab");
	if (f)
	{
		fprintf(f, "[%d] ", getpid());
		va_start(args, fmt);
		vfprintf(f, fmt, args);
		va_end(args);
		fclose(f);
	}
}

#define log_debug(...) do { if (config.debug >= 1) log_error(__VA_ARGS__); } while(0)
#define log_debug2(...) do { if (config.debug >= 2) log_error(__VA_ARGS__); } while(0)

// ****************************************************************************

struct Config
{
	int mainX;
	int mainY;
	unsigned int mainW;
	unsigned int mainH;
	unsigned int desktopW;
	unsigned int desktopH;
	int actualX;
	int actualY;

	int mst2X;
	int mst2Y;
	unsigned int mst2W;
	unsigned int mst2H;
	int mst3X;
	int mst3Y;
	unsigned int mst3W;
	unsigned int mst3H;
	int mst4X;
	int mst4Y;
	unsigned int mst4W;
	unsigned int mst4H;

	char enable;
	char debug;
	char joinMST;
	char maskOtherMonitors;
	char resizeWindows;
	char resizeAll;
	char moveWindows;
	char fork;
	char filterFocus;
	char noMouseGrab;
	char noKeyboardGrab;
	char noPrimarySelection;
	char dumb; // undocumented - act as a dumb pipe, nothing more
	char confineMouse;
};

static struct Config config = {};
static char configLoaded = 0;

enum { maxMST = 4 };
static          int* mstConfigX[maxMST] = { &config.mainX, &config.mst2X, &config.mst3X, &config.mst4X };
static          int* mstConfigY[maxMST] = { &config.mainY, &config.mst2Y, &config.mst3Y, &config.mst4Y };
static unsigned int* mstConfigW[maxMST] = { &config.mainW, &config.mst2W, &config.mst3W, &config.mst4W };
static unsigned int* mstConfigH[maxMST] = { &config.mainH, &config.mst2H, &config.mst3H, &config.mst4H };

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
		PARSE_INT(actualX)
		PARSE_INT(actualY)

		PARSE_INT(mst2X)
		PARSE_INT(mst2Y)
		PARSE_INT(mst2W)
		PARSE_INT(mst2H)
		PARSE_INT(mst3X)
		PARSE_INT(mst3Y)
		PARSE_INT(mst3W)
		PARSE_INT(mst3H)
		PARSE_INT(mst4X)
		PARSE_INT(mst4Y)
		PARSE_INT(mst4W)
		PARSE_INT(mst4H)

		PARSE_INT(enable)
		PARSE_INT(debug)
		PARSE_INT(joinMST)
		PARSE_INT(maskOtherMonitors)
		PARSE_INT(resizeWindows)
		PARSE_INT(resizeAll)
		PARSE_INT(moveWindows)
		PARSE_INT(fork)
		PARSE_INT(filterFocus)
		PARSE_INT(noMouseGrab)
		PARSE_INT(noKeyboardGrab)
		PARSE_INT(noPrimarySelection)
		PARSE_INT(dumb)
		PARSE_INT(confineMouse)

		/* else */
			log_error("Unknown option: %s\n", buf);

		#undef PARSE_INT
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
	config.mainX = 0;
	config.mainY = 0;
	config.mainW = 3840;
	config.mainH = 2160;
	config.desktopW = 3840;
	config.desktopH = 2160;

	char buf[1024] = {0};
	strncpy(buf, getenv("HOME"), sizeof(buf)-100);
	strcat(buf, "/.config"	); mkdir(buf, 0700); // TODO: XDG_CONFIG_HOME
	strcat(buf, "/hax11"); mkdir(buf, 0700);
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
					log_error("hax11: Ack, dlopen failed!\n");	\
				}												\
				pfunc = (typeof(&func))dlsym(handle, #func);	\
			}													\
			if (!pfunc)											\
				log_error("hax11: Ack, dlsym failed!\n");		\
			pfunc;												\
		})

static void fixSize(
	CARD16* width,
	CARD16* height)
{
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
	if (config.joinMST && *width == config.mainW/2 && *height == config.mainH)
		*width = config.mainW;
}

static void fixCoords(INT16* x, INT16* y, CARD16 *width, CARD16 *height)
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

static void fixMonitor(INT16* x, INT16* y, CARD16 *width, CARD16 *height)
{
	if (config.joinMST)
	{
		for (int n=0; n<maxMST; n++)
			if (*mstConfigW[n])
			{
				if (*width  == *mstConfigW[n] / 2
				 && *height == *mstConfigH[n]
				 && *y      == *mstConfigY[n]) // Is MST panel?
				{
					if (*x == *mstConfigX[n]) // Left panel
					{
						*width = *mstConfigW[n]; // resize
						//*height = 2160;
					}
					else
					if (*x == (INT16)(*mstConfigX[n] + *mstConfigW[n] / 2)) // Right panel
						*x = *y = *width = *height = 0; // disable
				}
			}
	}

	if (config.maskOtherMonitors)
		if (*width != config.mainW || *height != config.mainH)
			*x = *y = *width = *height = 0; // disable
}

static void hexDump(const void* buf, size_t len, char prefix1, char prefix2)
{
	if (config.debug < 3)
		return;

	while (len)
	{
		size_t n = len > 16 ? 16 : len;
		char textbuf[16*3+1];
		char *textptr = textbuf;

		for (size_t i = 0; i < n; i++)
			textptr += sprintf(textptr, " %02X", ((const unsigned char*)buf)[i]);
		log_error("%c%c%s\n", prefix1, prefix2, textbuf);

		buf += n;
		len -= n;
	}
}

#include <sys/socket.h>

#define ANCIL_SIZE 256
struct Connection
{
	int recvfd, sendfd;
	char dir; // for logging

	// Ancillary data buffer.
	// Necessary to pass around file descriptors needed for DRI3.
	char ancilBuf[ANCIL_SIZE];
	size_t ancilRead, ancilWrite;
};

static char sendAll(struct Connection* conn, const void* buf, size_t length)
{
	int remaining = length;
	while (remaining)
	{
		struct iovec iov;
		iov.iov_base = (void*)buf;
		iov.iov_len = remaining;

		struct msghdr msg;
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = conn->ancilBuf + conn->ancilRead;
		msg.msg_controllen = conn->ancilWrite - conn->ancilRead;

		int len = sendmsg(conn->sendfd, &msg, MSG_NOSIGNAL);
		if (len <= 0)
			return 0;

		hexDump(msg.msg_control, msg.msg_controllen, conn->dir, '%');
		conn->ancilRead += msg.msg_controllen;
		if (conn->ancilRead == conn->ancilWrite)
			conn->ancilRead = conn->ancilWrite = 0;

		hexDump(buf, len, conn->dir, '=');
		buf += len;
		remaining -= len;
	}
	return 1;
}

static char recvAll(struct Connection* conn, void* buf, size_t length)
{
	int remaining = length;
	while (remaining)
	{
		struct iovec iov;
		iov.iov_base = buf;
		iov.iov_len = remaining;

		struct msghdr msg;
		msg.msg_name = NULL;
		msg.msg_namelen = 0;
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = conn->ancilBuf + conn->ancilWrite;
		msg.msg_controllen = ANCIL_SIZE - conn->ancilWrite;

		int len = recvmsg(conn->recvfd, &msg, 0);
		if (len < 0)
			return 0;

		hexDump(msg.msg_control, msg.msg_controllen, conn->dir, '*');
		conn->ancilWrite += msg.msg_controllen;

		if (len == 0)
			return 0;

		hexDump(buf, len, conn->dir, '-');
		buf += len;
		remaining -= len;
	}
	return 1;
}

static size_t pad(size_t n)
{
	return (n+3) & ~3;
}

static const char* requestNames[256] =
{
	NULL, // 0
	"CreateWindow",
	"ChangeWindowAttributes",
	"GetWindowAttributes",
	"DestroyWindow",
	"DestroySubwindows",
	"ChangeSaveSet",
	"ReparentWindow",
	"MapWindow",
	"MapSubwindows",
	"UnmapWindow",
	"UnmapSubwindows",
	"ConfigureWindow",
	"CirculateWindow",
	"GetGeometry",
	"QueryTree",
	"InternAtom",
	"GetAtomName",
	"ChangeProperty",
	"DeleteProperty",
	"GetProperty",
	"ListProperties",
	"SetSelectionOwner",
	"GetSelectionOwner",
	"ConvertSelection",
	"SendEvent",
	"GrabPointer",
	"UngrabPointer",
	"GrabButton",
	"UngrabButton",
	"ChangeActivePointerGrab",
	"GrabKeyboard",
	"UngrabKeyboard",
	"GrabKey",
	"UngrabKey",
	"AllowEvents",
	"GrabServer",
	"UngrabServer",
	"QueryPointer",
	"GetMotionEvents",
	"TranslateCoords",
	"WarpPointer",
	"SetInputFocus",
	"GetInputFocus",
	"QueryKeymap",
	"OpenFont",
	"CloseFont",
	"QueryFont",
	"QueryTextExtents",
	"ListFonts",
	"ListFontsWithInfo",
	"SetFontPath",
	"GetFontPath",
	"CreatePixmap",
	"FreePixmap",
	"CreateGC",
	"ChangeGC",
	"CopyGC",
	"SetDashes",
	"SetClipRectangles",
	"FreeGC",
	"ClearArea",
	"CopyArea",
	"CopyPlane",
	"PolyPoint",
	"PolyLine",
	"PolySegment",
	"PolyRectangle",
	"PolyArc",
	"FillPoly",
	"PolyFillRectangle",
	"PolyFillArc",
	"PutImage",
	"GetImage",
	"PolyText8",
	"PolyText16",
	"ImageText8",
	"ImageText16",
	"CreateColormap",
	"FreeColormap",
	"CopyColormapAndFree",
	"InstallColormap",
	"UninstallColormap",
	"ListInstalledColormaps",
	"AllocColor",
	"AllocNamedColor",
	"AllocColorCells",
	"AllocColorPlanes",
	"FreeColors",
	"StoreColors",
	"StoreNamedColor",
	"QueryColors",
	"LookupColor",
	"CreateCursor",
	"CreateGlyphCursor",
	"FreeCursor",
	"RecolorCursor",
	"QueryBestSize",
	"QueryExtension",
	"ListExtensions",
	"ChangeKeyboardMapping",
	"GetKeyboardMapping",
	"ChangeKeyboardControl",
	"GetKeyboardControl",
	"Bell",
	"ChangePointerControl",
	"GetPointerControl",
	"SetScreenSaver",
	"GetScreenSaver",
	"ChangeHosts",
	"ListHosts",
	"SetAccessControl",
	"SetCloseDownMode",
	"KillClient",
	"RotateProperties",
	"ForceScreenSaver",
	"SetPointerMapping",
	"GetPointerMapping",
	"SetModifierMapping",
	"GetModifierMapping",
	NULL, // 120
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"NoOperation",
	NULL,
};

static const char *responseNames[256] =
{
	"Error",
	"Reply",
	"KeyPress",
	"KeyRelease",
	"ButtonPress",
	"ButtonRelease",
	"MotionNotify",
	"EnterNotify",
	"LeaveNotify",
	"FocusIn",
	"FocusOut",
	"KeymapNotify",
	"Expose",
	"GraphicsExpose",
	"NoExpose",
	"VisibilityNotify",
	"CreateNotify",
	"DestroyNotify",
	"UnmapNotify",
	"MapNotify",
	"MapRequest",
	"ReparentNotify",
	"ConfigureNotify",
	"ConfigureRequest",
	"GravityNotify",
	"ResizeRequest",
	"CirculateNotify",
	"CirculateRequest",
	"PropertyNotify",
	"SelectionClear",
	"SelectionRequest",
	"SelectionNotify",
	"ColormapNotify",
	"ClientMessage",
	"MappingNotify",
	"GenericEvent",
	NULL,
};

static void bufSize(unsigned char** ptr, size_t *len, size_t needed)
{
	if (needed > *len)
	{
		*ptr = realloc(*ptr, needed);
		*len = needed;
	}
}

static int strmemcmp(const char* str, const void* mem, size_t meml)
{
	size_t strl = strlen(str);
	if (strl != meml)
		return strl - meml;
	return memcmp(str, mem, meml);
}

typedef struct
{
	/// Number of this X server connection for this process
	int index;

	/// Sockets for the connection to the X server (Xorg) and client (host application)
	int server, client;

	/// Reusable data buffer
	unsigned char *buf;
	size_t bufLen;

	/// Connection prefix received and sent
	bool clientInitialized, serverInitialized;

	/// Notes for correlating replies to their requests (see Note_* enum)
	unsigned char notes[1<<16];

	/// Learned opcodes for X extensions, as returned by QueryExtension
	unsigned char opcode_XFree86_VidModeExtension;
	unsigned char opcode_RANDR;
	unsigned char opcode_Xinerama;
	unsigned char opcode_NV_GLX;

	/// Reply serial tracking and correction
	CARD16 serial, serialLast, serialDelta;
	unsigned char skip[1<<16];

	/// Whether a mouse-grab is still pending (for the ConfineMouse option)
	bool doMouseGrab;
} X11ConnData;

enum
{
	Note_None,
	Note_X_GetGeometry,
	Note_X_GetInputFocus,
	Note_X_InternAtom_Other,
	Note_X_QueryExtension_XFree86_VidModeExtension,
	Note_X_QueryExtension_RANDR,
	Note_X_QueryExtension_Xinerama,
	Note_X_QueryExtension_NV_GLX,
	Note_X_QueryExtension_Other,
	Note_X_XF86VidModeGetModeLine,
	Note_X_XF86VidModeGetAllModeLines,
	Note_X_RRGetScreenInfo,
	Note_X_RRGetScreenResources,
	Note_X_RRGetCrtcInfo,
	Note_X_XineramaQueryScreens,
	Note_NV_GLX,
};

static CARD16 injectRequest(X11ConnData *data, void* buf, size_t size)
{
	struct Connection conn = {};
	conn.recvfd = data->client;
	conn.sendfd = data->server;
	conn.dir = '<';

	const xReq* req = (xReq*)buf;
	sendAll(&conn, req, size);
	CARD16 sequenceNumber = ++data->serial;
	data->skip[sequenceNumber] = true;
	log_debug2("[%d][%d] Injected request %d (%s) with data %d, length %d\n", data->index, sequenceNumber, req->reqType, requestNames[req->reqType], req->data, size);
	return sequenceNumber;
}

static bool handleClientData(X11ConnData* data)
{
	struct Connection conn = {};
	conn.recvfd = data->client;
	conn.sendfd = data->server;
	conn.dir = '<';

	if (config.dumb)
	{
		char c[1];
		if (!recvAll(&conn, c, 1)) return false;
		if (!sendAll(&conn, c, 1)) return false;
		return true;
	}

	if (!data->clientInitialized)
	{
		xConnClientPrefix header;
		if (!recvAll(&conn, &header, sizeof(header))) return false;
		if (header.byteOrder != 'l')
		{
			log_debug("Unsupported byte order %c!\n", header.byteOrder);
			return false;
		}
		if (!sendAll(&conn, &header, sz_xConnClientPrefix)) return false;

		if (!recvAll(&conn, data->buf, pad(header.nbytesAuthProto))) return false;
		if (!sendAll(&conn, data->buf, pad(header.nbytesAuthProto))) return false;
		if (!recvAll(&conn, data->buf, pad(header.nbytesAuthString))) return false;
		if (!sendAll(&conn, data->buf, pad(header.nbytesAuthString))) return false;

		data->clientInitialized = true;
		return true;
	}

	size_t ofs = 0;
	if (!recvAll(&conn, data->buf+ofs, sz_xReq)) return false;
	ofs += sz_xReq;

	const xReq* req = (xReq*)data->buf;
	uint requestLength = req->length * 4;
	if (requestLength == 0) // Big Requests Extension
	{
		recvAll(&conn, data->buf+ofs, 4);
		requestLength = *(uint*)(data->buf+ofs) * 4;
		ofs += 4;
	}
	CARD16 sequenceNumber = ++data->serial;
	log_debug2("[%d][%d] Request %d (%s) with data %d, length %d\n", data->index, sequenceNumber, req->reqType, requestNames[req->reqType], req->data, requestLength);
	bufSize(&data->buf, &data->bufLen, requestLength);
	req = (xReq*)data->buf; // in case bufSize moved buf

	if (!recvAll(&conn, data->buf+ofs, requestLength - ofs)) return false;

	data->notes[sequenceNumber] = Note_None;
	data->skip[sequenceNumber] = false;

	switch (req->reqType)
	{
		// Fix for games that create the window of the wrong size or on the wrong monitor.
		case X_CreateWindow:
		{
			xCreateWindowReq* req = (xCreateWindowReq*)data->buf;
			log_debug2(" XCreateWindow(%dx%d @ %dx%d)\n", req->width, req->height, req->x, req->y);
			fixCoords(&req->x, &req->y, &req->width, &req->height);
			log_debug2(" ->           (%dx%d @ %dx%d)\n", req->width, req->height, req->x, req->y);
			break;
		}

		case X_ConfigureWindow:
		{
			xConfigureWindowReq* req = (xConfigureWindowReq*)data->buf;

			INT16 dummyXY = 0;
			CARD16 dummyW = config.mainW;
			CARD16 dummyH = config.mainH;
			INT16 *x = &dummyXY, *y = &dummyXY;
			CARD16 *w = &dummyW, *h = &dummyH;

			int* ptr = (int*)(data->buf + sz_xConfigureWindowReq);
			if (req->mask & 0x0001) // x
			{
				x = (INT16*)ptr;
				ptr++;
			}
			if (req->mask & 0x0002) // y
			{
				y = (INT16*)ptr;
				ptr++;
			}
			if (req->mask & 0x0004) // width
			{
				w = (CARD16*)ptr;
				ptr++;
			}
			if (req->mask & 0x0008) // height
			{
				h = (CARD16*)ptr;
				ptr++;
			}

			log_debug2(" XConfigureWindow(%dx%d @ %dx%d)\n", *w, *h, *x, *y);
			fixCoords(x, y, w, h);
			log_debug2(" ->              (%dx%d @ %dx%d)\n", *w, *h, *x, *y);
			break;
		}

		// Fix for games setting their window size based on the X root window size
		// (which can encompass multiple physical monitors).
		case X_GetGeometry:
		{
			data->notes[sequenceNumber] = Note_X_GetGeometry;
			break;
		}

		case X_GetInputFocus:
		{
			data->notes[sequenceNumber] = Note_X_GetInputFocus;
			break;
		}

		case X_InternAtom:
		{
			xInternAtomReq* req = (xInternAtomReq*)data->buf;
			const char* name = (const char*)(data->buf + sz_xInternAtomReq);
			log_debug2(" XInternAtom: %.*s\n", req->nbytes, name);
			data->notes[sequenceNumber] = Note_X_InternAtom_Other;
			break;
		}

		case X_ChangeProperty:
		{
			xChangePropertyReq* req = (xChangePropertyReq*)data->buf;
			log_debug2(" XChangeProperty: property=%d type=%d format=%d)\n", req->property, req->type, req->format);
			if (req->type == XA_WM_SIZE_HINTS)
			{
				XSizeHints* hints = (XSizeHints*)(data->buf + sz_xChangePropertyReq);
				fixCoords((INT16*)&hints->x, (INT16*)&hints->y, (CARD16*)&hints->width, (CARD16*)&hints->height);
				fixSize((CARD16*)&hints->max_width, (CARD16*)&hints->max_height);
				fixSize((CARD16*)&hints->base_width, (CARD16*)&hints->base_height);
			}
			break;
		}

		case X_QueryExtension:
		{
			xQueryExtensionReq* req = (xQueryExtensionReq*)data->buf;
			const char* name = (const char*)(data->buf + sz_xQueryExtensionReq);
			log_debug2(" XQueryExtension(%.*s)\n", req->nbytes, name);

			if (!strmemcmp("XFree86-VidModeExtension", name, req->nbytes))
				data->notes[sequenceNumber] = Note_X_QueryExtension_XFree86_VidModeExtension;
			else
			if (!strmemcmp("RANDR", name, req->nbytes))
				data->notes[sequenceNumber] = Note_X_QueryExtension_RANDR;
			else
			if (!strmemcmp("XINERAMA", name, req->nbytes))
				data->notes[sequenceNumber] = Note_X_QueryExtension_Xinerama;
			else
			if (!strmemcmp("NV-GLX", name, req->nbytes))
				data->notes[sequenceNumber] = Note_X_QueryExtension_NV_GLX;
			else
				data->notes[sequenceNumber] = Note_X_QueryExtension_Other;
			break;
		}

		case X_GrabPointer:
			if (config.noMouseGrab)
			{
				log_debug("Clobbering X_GrabPointer event\n");
				// Specify an obviously-invalid value
				xGrabPointerReq* req = (xGrabPointerReq*)data->buf;
				req->time = -1;
			}
			break;

		case X_GrabKeyboard:
			if (config.noKeyboardGrab)
			{
				log_debug("Clobbering X_GrabKeyboard event\n");
				// Specify an obviously-invalid value
				xGrabKeyboardReq* req = (xGrabKeyboardReq*)data->buf;
				req->time = -1;
			}
			break;

		case X_GetSelectionOwner:
		{
			xResourceReq* req = (xResourceReq*)data->buf;
			log_debug(" Selection atom: %d\n", req->id);
			if (config.noPrimarySelection && req->id == XA_PRIMARY)
			{
				req->id = XA_CUT_BUFFER0;
				log_debug(" -> Replaced atom: %d\n", req->id);
			}
			break;
		}
		case X_SetSelectionOwner:
		{
			xSetSelectionOwnerReq* req = (xSetSelectionOwnerReq*)data->buf;
			log_debug(" Selection atom: %d\n", req->selection);
			if (config.noPrimarySelection && req->selection == XA_PRIMARY)
			{
				req->selection = XA_CUT_BUFFER1;
				log_debug(" -> Replaced atom: %d\n", req->selection);
			}
			break;
		}
		case X_ConvertSelection:
		{
			xConvertSelectionReq* req = (xConvertSelectionReq*)data->buf;
			log_debug(" Selection atom: %d\n", req->selection);
			if (config.noPrimarySelection && req->selection == XA_PRIMARY)
			{
				req->selection = XA_CUT_BUFFER2;
				log_debug(" -> Replaced atom: %d\n", req->selection);
			}
			break;
		}

		case 0:
			break;

		default:
		{
			if (req->reqType == data->opcode_XFree86_VidModeExtension)
			{
				xXF86VidModeGetModeLineReq* req = (xXF86VidModeGetModeLineReq*)data->buf;
				log_debug2(" XFree86_VidModeExtension - %d\n", req->xf86vidmodeReqType);
				switch (req->xf86vidmodeReqType)
				{
					case X_XF86VidModeGetModeLine:
						data->notes[sequenceNumber] = Note_X_XF86VidModeGetModeLine;
						break;
					case X_XF86VidModeGetAllModeLines:
						data->notes[sequenceNumber] = Note_X_XF86VidModeGetAllModeLines;
						break;
				}
			}
			else
			if (req->reqType == data->opcode_RANDR)
			{
				log_debug2(" RANDR - %d\n", req->data);
				switch (req->data)
				{
					case X_RRGetScreenInfo:
						data->notes[sequenceNumber] = Note_X_RRGetScreenInfo;
						break;
					case X_RRGetScreenResources:
						data->notes[sequenceNumber] = Note_X_RRGetScreenResources;
						break;
					case X_RRGetCrtcInfo:
						data->notes[sequenceNumber] = Note_X_RRGetCrtcInfo;
						break;
				}
			}
			else
			if (req->reqType == data->opcode_Xinerama)
			{
				log_debug2(" Xinerama - %d\n", req->data);
				switch (req->data)
				{
					case X_XineramaQueryScreens:
						data->notes[sequenceNumber] = Note_X_XineramaQueryScreens;
						break;
				}
			}
			else
			if (req->reqType == data->opcode_NV_GLX)
			{
#if 0
				char fn[256];
				sprintf(fn, "/tmp/hax11-NV-%d-req", sequenceNumber);
				FILE* f = fopen(fn, "wb");
				fwrite(data->buf, 1, requestLength, f);
				fclose(f);
#endif
				data->notes[sequenceNumber] = Note_NV_GLX;
			}
			break;
		}
	}

	if (config.debug >= 2 && config.actualX && config.actualY && memmem(data->buf, requestLength, &config.actualX, 2) && memmem(data->buf, requestLength, &config.actualY, 2))
		log_debug2("   Found actualW/H in input! ----------------------------------------------------------------------------------------------\n");

	if (!sendAll(&conn, data->buf, requestLength)) return false;

	return true;
}

static bool handleServerData(X11ConnData* data)
{
	struct Connection conn = {};
	conn.recvfd = data->server;
	conn.sendfd = data->client;
	conn.dir = '>';

	if (config.dumb)
	{
		char c[1];
		if (!recvAll(&conn, c, 1)) return false;
		if (!sendAll(&conn, c, 1)) return false;
		return true;
	}

	if (!data->serverInitialized)
	{
		xConnSetupPrefix header;
		if (!recvAll(&conn, &header, sz_xConnSetupPrefix)) return false;
		if (!sendAll(&conn, &header, sz_xConnSetupPrefix)) return false;

		log_debug("Server connection setup reply: %d\n", header.success);

		size_t dataLength = header.length * 4;
		bufSize(&data->buf, &data->bufLen, dataLength);
		if (!recvAll(&conn, data->buf, dataLength)) return false;
		if (!sendAll(&conn, data->buf, dataLength)) return false;

		data->serverInitialized = true;
		return true;
	}

	if (!recvAll(&conn, data->buf, sz_xReply)) return false;
	size_t ofs = sz_xReply;
	xReply* reply = (xReply*)data->buf;

	if (reply->generic.type == X_Reply || reply->generic.type == GenericEvent)
	{
		size_t dataLength = reply->generic.length * 4;
		bufSize(&data->buf, &data->bufLen, ofs + dataLength);
		reply = (xReply*)data->buf; // in case bufSize moved buf
		if (!recvAll(&conn, data->buf+ofs, dataLength)) return false;
		ofs += dataLength;
	}
	log_debug2(" [%d]Response: %d (%s) sequenceNumber=%d length=%d\n",
		data->index, reply->generic.type, responseNames[reply->generic.type], reply->generic.sequenceNumber, ofs);

	switch (reply->generic.type)
	{
		case X_Error:
		{
			xError* err = (xError*)data->buf;
			log_debug2(" [%d] Error - code=%d resourceID=0x%x minorCode=%d majorCode=%d (%s)\n",
				data->index, err->errorCode, err->resourceID, err->minorCode, err->majorCode, requestNames[err->majorCode]);
			break;
		}

		case X_Reply:
		{
			switch (data->notes[reply->generic.sequenceNumber])
			{
				case Note_X_GetGeometry:
				{
					xGetGeometryReply* reply = (xGetGeometryReply*)data->buf;
					log_debug2("  XGetGeometry(%d,%d,%d,%d)\n", reply->x, reply->y, reply->width, reply->height);
					fixCoords(&reply->x, &reply->y, &reply->width, &reply->height);
					log_debug2("  ->          (%d,%d,%d,%d)\n", reply->x, reply->y, reply->width, reply->height);
					break;
				}

				case Note_X_GetInputFocus:
				{
					if (data->doMouseGrab)
					{
						xGetInputFocusReply* reply = (xGetInputFocusReply*)data->buf;
						log_debug2("  XGetInputFocus(0x%x, %d)\n", reply->focus, reply->revertTo);

						log_debug("Acquiring mouse grab\n");

						xGrabPointerReq req;
						req.reqType = X_GrabPointer;
						req.ownerEvents = true; // ?
						req.length = sizeof(req)/4;
						req.grabWindow = reply->focus;
						req.eventMask = ~0xFFFF8003;
						req.pointerMode = 1 /* Asynchronous */;
						req.keyboardMode = 1 /* Asynchronous */;
						req.confineTo = reply->focus;
						req.cursor = None;
						req.time = CurrentTime;
						injectRequest(data, &req, sizeof(req));

						data->doMouseGrab = false;
					}
					break;
				}

				case Note_X_InternAtom_Other:
				{
					xInternAtomReply* reply = (xInternAtomReply*)data->buf;
					log_debug2("  X_InternAtom: atom=%d\n", reply->atom);
					break;
				}

				case Note_X_QueryExtension_XFree86_VidModeExtension:
				{
					xQueryExtensionReply* reply = (xQueryExtensionReply*)data->buf;
					log_debug2("  X_QueryExtension (XFree86-VidModeExtension): present=%d major_opcode=%d first_event=%d first_error=%d\n",
						reply->present, reply->major_opcode, reply->first_event, reply->first_error);
					if (reply->present)
						data->opcode_XFree86_VidModeExtension = reply->major_opcode;
					break;
				}

				case Note_X_QueryExtension_RANDR:
				{
					xQueryExtensionReply* reply = (xQueryExtensionReply*)data->buf;
					log_debug2("  X_QueryExtension (RANDR): present=%d major_opcode=%d first_event=%d first_error=%d\n",
						reply->present, reply->major_opcode, reply->first_event, reply->first_error);
					if (reply->present)
						data->opcode_RANDR = reply->major_opcode;
					break;
				}

				case Note_X_QueryExtension_Xinerama:
				{
					xQueryExtensionReply* reply = (xQueryExtensionReply*)data->buf;
					log_debug2("  X_QueryExtension (XINERAMA): present=%d major_opcode=%d first_event=%d first_error=%d\n",
						reply->present, reply->major_opcode, reply->first_event, reply->first_error);
					if (reply->present)
						data->opcode_Xinerama = reply->major_opcode;
					break;
				}

				case Note_X_QueryExtension_NV_GLX:
				{
					xQueryExtensionReply* reply = (xQueryExtensionReply*)data->buf;
					log_debug2("  X_QueryExtension (NV-GLX): present=%d major_opcode=%d first_event=%d first_error=%d\n",
						reply->present, reply->major_opcode, reply->first_event, reply->first_error);
					if (reply->present)
						data->opcode_NV_GLX = reply->major_opcode;
					break;
				}

				case Note_X_QueryExtension_Other:
				{
					xQueryExtensionReply* reply = (xQueryExtensionReply*)data->buf;
					log_debug2("  X_QueryExtension: present=%d major_opcode=%d first_event=%d first_error=%d\n",
						reply->present, reply->major_opcode, reply->first_event, reply->first_error);
					break;
				}

				case Note_X_XF86VidModeGetModeLine:
				{
					xXF86VidModeGetModeLineReply* reply = (xXF86VidModeGetModeLineReply*)data->buf;
					log_debug2("  X_XF86VidModeGetModeLine(%d x %d)\n", reply->hdisplay, reply->vdisplay);
					fixSize(&reply->hdisplay, &reply->vdisplay);
					log_debug2("  ->                      (%d x %d)\n", reply->hdisplay, reply->vdisplay);
					break;
				}

				case Note_X_XF86VidModeGetAllModeLines:
				{
					xXF86VidModeGetAllModeLinesReply* reply = (xXF86VidModeGetAllModeLinesReply*)data->buf;
					xXF86VidModeModeInfo* modeInfos = (xXF86VidModeModeInfo*)(data->buf + sz_xXF86VidModeGetAllModeLinesReply);
					for (size_t i=0; i<reply->modecount; i++)
					{
						xXF86VidModeModeInfo* modeInfo = modeInfos + i;
						log_debug2("  X_XF86VidModeGetAllModeLines[%d] = %d x %d\n", i, modeInfo->hdisplay, modeInfo->vdisplay);
						fixSize(&modeInfo->hdisplay, &modeInfo->vdisplay);
						log_debug2("  ->                                %d x %d\n",    modeInfo->hdisplay, modeInfo->vdisplay);
					}
					break;
				}

				case Note_X_RRGetScreenInfo:
				{
					xRRGetScreenInfoReply* reply = (xRRGetScreenInfoReply*)data->buf;
					xScreenSizes* sizes = (xScreenSizes*)(data->buf+sz_xRRGetScreenInfoReply);
					for (size_t i=0; i<reply->nSizes; i++)
					{
						xScreenSizes* size = sizes+i;
						log_debug2("  X_RRGetScreenInfo[%d] = %d x %d\n", i, size->widthInPixels, size->heightInPixels);
						fixSize(&size->widthInPixels, &size->heightInPixels);
						log_debug2("  ->                      %d x %d\n",    size->widthInPixels, size->heightInPixels);
					}
					break;
				}

				case Note_X_RRGetScreenResources:
				{
					xRRGetScreenResourcesReply* reply = (xRRGetScreenResourcesReply*)data->buf;
					void* ptr = data->buf+sz_xRRGetScreenResourcesReply;
					ptr += reply->nCrtcs * sizeof(CARD32);
					ptr += reply->nOutputs * sizeof(CARD32);
					for (size_t i=0; i<reply->nModes; i++)
					{
						xRRModeInfo* modeInfo = (xRRModeInfo*)ptr;
						log_debug2("  X_RRGetScreenResources[%d] = %d x %d\n", i, modeInfo->width, modeInfo->height);
						fixSize(&modeInfo->width, &modeInfo->height);
						log_debug2("  ->                           %d x %d\n",    modeInfo->width, modeInfo->height);
						ptr += sz_xRRModeInfo;
					}
					break;
				}

				case Note_X_RRGetCrtcInfo:
				{
					xRRGetCrtcInfoReply* reply = (xRRGetCrtcInfoReply*)data->buf;
					log_debug2("  X_RRGetCrtcInfo = %dx%d @ %dx%d\n", reply->width, reply->height, reply->x, reply->y);
					if (reply->mode != None)
					{
						fixMonitor(&reply->x, &reply->y, &reply->width, &reply->height);
						if (!reply->width || !reply->height)
						{
							reply->x = reply->y = reply->width = reply->height = 0;
							reply->mode = None;
							reply->rotation = reply->rotations = RR_Rotate_0;
							reply->nOutput = reply->nPossibleOutput = 0;
						}
					}
					log_debug2("  ->                %dx%d @ %dx%d\n", reply->width, reply->height, reply->x, reply->y);
					break;
				}

				case Note_X_XineramaQueryScreens:
				{
					xXineramaQueryScreensReply* reply = (xXineramaQueryScreensReply*)data->buf;
					xXineramaScreenInfo* screens = (xXineramaScreenInfo*)(data->buf+sz_XineramaQueryScreensReply);
					for (size_t i=0; i<reply->number; i++)
					{
						xXineramaScreenInfo* screen = screens+i;
						log_debug2("  X_XineramaQueryScreens[%d] = %dx%d @ %dx%d\n", i, screen->width, screen->height, screen->x_org, screen->y_org);
						fixCoords(&screen->x_org, &screen->y_org, &screen->width, &screen->height);
						log_debug2("  ->                           %dx%d @ %dx%d\n",    screen->width, screen->height, screen->x_org, screen->y_org);
					}
					break;
				}

				case Note_NV_GLX:
				{
#if 0
					char fn[256];
					static int counter = 0;
					sprintf(fn, "/tmp/hax11-NV-%d-rsp-%d", reply->generic.sequenceNumber, counter++);
					FILE* f = fopen(fn, "wb");
					fwrite(data->buf, 1, ofs, f);
					fclose(f);
#endif
					break;
				}
			}
			break;
		}

		case FocusIn:
			if (config.confineMouse)
			{
				if (!data->doMouseGrab)
				{
					xReq req;
					req.reqType = X_GetInputFocus;
					req.length = sizeof(req)/4;
					CARD16 sequenceNumber = injectRequest(data, &req, sizeof(req));
					data->notes[sequenceNumber] = Note_X_GetInputFocus;

					data->doMouseGrab = true;
				}
			}
			break;

		case FocusOut:
			if (config.confineMouse)
			{
				log_debug("Releasing mouse grab\n");

				xResourceReq req;
				req.reqType = X_UngrabPointer;
				req.length = sizeof(req)/4;
				req.id = CurrentTime;
				injectRequest(data, &req, sizeof(req));

				data->doMouseGrab = false;
			}

			if (config.filterFocus)
			{
				log_debug("Filtering out FocusOut event\n");
				return true;
			}
			break;
	}

	if (config.debug >= 2 && config.actualX && config.actualY && memmem(data->buf, ofs, &config.actualX, 2) && memmem(data->buf, ofs, &config.actualY, 2))
		log_debug2("   Found actualW/H in output! ----------------------------------------------------------------------------------------------\n");

	while (data->serialLast != reply->generic.sequenceNumber)
	{
		data->skip[data->serialLast] = false;
		data->serialLast++;
		if (data->skip[data->serialLast])
		{
			data->serialDelta++;
			log_debug2("  Incrementing serialDelta for injected reply (now at %d)\n", data->serialDelta);
		}
	}

	if (reply->generic.type < 2 && // reply or error only, not event
		data->skip[reply->generic.sequenceNumber])
	{
		log_debug2("  Skipping this reply\n");
		return true;
	}

	reply->generic.sequenceNumber -= data->serialDelta;

	if (!sendAll(&conn, data->buf, ofs)) return false;

	return true;
}

static void* workThreadProc(void* dataPtr)
{
	X11ConnData* data = (X11ConnData*)dataPtr;

	bufSize(&data->buf, &data->bufLen, 1<<16);

	fd_set readSet, errorSet;
	FD_ZERO(&readSet);
	FD_ZERO(&errorSet);

	if (data->server >= FD_SETSIZE || data->client >= FD_SETSIZE)
	{
		log_error("Too many file descriptors for FD_SETSIZE!");
		return NULL;
	}

	while (true)
    {
	    FD_SET(data->client, &readSet );
	    FD_SET(data->server, &readSet );
	    FD_SET(data->client, &errorSet);
	    FD_SET(data->server, &errorSet);

	    if (select(FD_SETSIZE, &readSet, NULL, &errorSet, NULL) < 0)
	    {
		    log_error("select() failed");
		    break;
	    }

	    if (FD_ISSET (data->client, &errorSet))
	    {
		    log_debug("Error on client socket");
		    break;
	    }
	    if (FD_ISSET (data->server, &errorSet))
	    {
		    log_debug("Error on server socket");
		    break;
	    }

	    if (FD_ISSET (data->client, &readSet))
		    if (!handleClientData(data))
			    break;
	    if (FD_ISSET (data->server, &readSet))
		    if (!handleServerData(data))
			    break;
    }

	log_debug("Exiting work thread.\n");
	shutdown(data->client, SHUT_RDWR);
	shutdown(data->server, SHUT_RDWR);
	close(data->client);
	close(data->server);
	return NULL;
}

static void* libc = NULL;
static void* pthread = NULL;

#include <sys/un.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sys/wait.h>

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

					X11ConnData* data = calloc(1, sizeof(X11ConnData));
					static int index = 0;
					data->index = index++;
					data->server = dup(socket);
					data->client = pair[0];
					dup2(pair[1], socket);
					close(pair[1]);

					if (config.fork)
					{
						log_debug("Forking...\n");

						pid_t pid = fork();
						if (pid == 0)
						{
							log_debug("In child, forking again\n");
							pid = getpid();
							pid_t pid2 = fork();
							if (pid2)
							{
								log_debug("Second fork is %d\n", pid2);
								exit(0);
							}
							log_debug("In child, double-fork OK\n");
							waitpid(pid, NULL, 0);
							log_debug("Parent exited, proceeding\n");
							struct rlimit r;
							getrlimit(RLIMIT_NOFILE, &r);

							for (int n=3; n<(int)r.rlim_cur; n++)
								if (n != data->server && n != data->client)
									close(n);
						}
						else
						{
							close(data->server);
							close(data->client);
							free(data);

							if (pid > 0)
							{
								log_debug("In parent! Child is %d\n", pid);
								waitpid(pid, NULL, 0);
								return result;
							}
							else
							{
								log_debug("Fork failed!\n");
								return pid;
							}
						}
					}

					pthread_attr_t attr;
					NEXT(pthread, "/usr/lib/libpthread.so", pthread_attr_init)(&attr);
					if (!config.fork)
					{
						NEXT(pthread, "/usr/lib/libpthread.so", pthread_attr_setdetachstate)(&attr, PTHREAD_CREATE_DETACHED);
					}

					pthread_t workThread;
					NEXT(pthread, "/usr/lib/libpthread.so", pthread_create)
						(& workThread, &attr, workThreadProc, data);
					NEXT(pthread, "/usr/lib/libpthread.so", pthread_attr_destroy)(&attr);

					if (config.fork)
					{
						log_debug("Joining work thread...\n");
						NEXT(pthread, "/usr/lib/libpthread.so", pthread_join)(workThread, NULL);
						log_debug("Fork is exiting.\n");
						NEXT(pthread, "/usr/lib/libpthread.so", pthread_exit)(0);
						exit(0);
					}
				}
			}
		}
	}
	return result;
}

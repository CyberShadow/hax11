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
#include <ctype.h>
#include <sys/stat.h>
#include <poll.h>
#include <inttypes.h>

#include <gnu/lib-names.h>

#include <X11/Xproto.h>
#include <X11/Xatom.h>
#include <X11/X.h>
#include <X11/extensions/xf86vmproto.h>
#include <X11/extensions/randr.h>
#include <X11/extensions/randrproto.h>
#include <X11/extensions/panoramiXproto.h>

// ****************************************************************************

static void log_error(const char *fmt, ...)
	__attribute__ ((format (printf, 1, 2)));

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

// Mirrors CARD32/CARD64 defines in /usr/include/X11/Xmd.h:
#ifdef LONG64
#define PRIuCARD32 "u"
#define PRIuCARD64 "lu"
#define PRIxCARD32 "x"
#define PRIxCARD64 "lx"
#else
#define PRIuCARD32 "lu"
#define PRIuCARD64 "llu"
#define PRIxCARD32 "lx"
#define PRIxCARD64 "llx"
#endif

// ****************************************************************************

enum
{
	MAP_KIND_KEY,
	MAP_KIND_BUTTON,
};

//typedef CARD8 KEYCODE;

struct MapInput
{
	unsigned char kind;
	unsigned int code;
};

struct MapConfig
{
	struct MapInput from, to;
	struct MapConfig *next;
};

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
	char noMinSize;
	char noMaxSize;
	char dumb; // undocumented - act as a dumb pipe, nothing more
	char confineMouse;
	char noResolutionChange;

	struct MapConfig *maps;
};

static struct Config config = {};
static char configLoaded = 0;

enum { maxMST = 4 };
static          int* mstConfigX[maxMST] = { &config.mainX, &config.mst2X, &config.mst3X, &config.mst4X };
static          int* mstConfigY[maxMST] = { &config.mainY, &config.mst2Y, &config.mst3Y, &config.mst4Y };
static unsigned int* mstConfigW[maxMST] = { &config.mainW, &config.mst2W, &config.mst3W, &config.mst4W };
static unsigned int* mstConfigH[maxMST] = { &config.mainH, &config.mst2H, &config.mst3H, &config.mst4H };

int parseInt(const char *s)
{
	char *endptr = 0;
	long int result = strtol(s, &endptr, 0);
	if (!endptr)
		log_error("Bad number: %s\n", s);
	return result;
}

bool parseInput(struct MapInput *input, const char *s)
{
	if (tolower(*s) == 'k')
		input->kind = MAP_KIND_KEY;
	else
	if (tolower(*s) == 'b')
		input->kind = MAP_KIND_BUTTON;
	else
	{
		log_error("Bad map kind: %s\n", s);
		return false;
	}
	input->code = parseInt(s + 1);
	return true;
}

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

		if (strncasecmp("Map", buf, 3) == 0)
		{
			struct MapConfig *map = malloc(sizeof(struct MapConfig));
			map->next = config.maps;
			if (parseInput(&map->from, buf + 3) &&
				parseInput(&map->to, p))
				config.maps = map;
			continue;
		}

		#define PARSE_INT(x)						\
			if (!strcasecmp(buf, #x))				\
				config.x = parseInt(p);				\
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
		PARSE_INT(noMinSize)
		PARSE_INT(noMaxSize)
		PARSE_INT(dumb)
		PARSE_INT(confineMouse)
		PARSE_INT(noResolutionChange)

		/* else */
			log_error("Unknown option: %s\n", buf);

		#undef PARSE_INT
	}
	fclose(f);
	//log_debug("Read config: %d %d %d %d\n", config.joinMST, config.maskOtherMonitors, config.resizeWindows, config.moveWindows);
}

// ****************************************************************************

static void getProfileName(char *buf, size_t size);

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
	char *home = getenv("HOME");
	if (!home)
		return;
	strncpy(buf, home, sizeof(buf)-100);
	strcat(buf, "/.config"	); mkdir(buf, 0700); // TODO: XDG_CONFIG_HOME
	strcat(buf, "/hax11"); mkdir(buf, 0700);
	strcat(buf, "/profiles"	); mkdir(buf, 0700);
	char *p = buf + strlen(buf);

	strcpy(p, "/default");
	readConfig(buf);

	getProfileName(p + 1, sizeof(buf) - (p-buf));
	readConfig(buf);
}

// ****************************************************************************

static void fixSize(
	CARD16* width,
	CARD16* height)
{
	if (config.resizeAll && *width >= 640 && *height >= 480)
	{
		*width = config.mainW;
		*height = config.mainH;
	}

	// Fix windows spanning multiple monitors
	if (config.resizeWindows && *width == config.desktopW)
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

static bool fixMonitor(INT16* x, INT16* y, CARD16 *width, CARD16 *height)
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
		{
			// return false;
			*x = config.mainX;
			*y = config.mainY;
			*width = config.mainW;
			*height = config.mainH;
		}

	return true;
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

		/*
		  Hex dump arrow legend:
		  - Char 1:
		    < - request (client (application) to server (Xorg))
		    > - reply   (server (Xorg) to client (application))
			{ - request (synthesized by hax11)
			} - reply   (synthesized by hax11)
		  - Char 2:
		    - - data, incoming into hax11
		    = - data, outgoing from hax11
		    * - metadata, incoming into hax11
		    % - metadata, outgoing from hax11
		 */

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
			log_debug("%c sendmsg returned %d\n", conn->dir, len);
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
		if (len <= 0)
			log_debug("%c recvmsg returned %d\n", conn->dir, len);
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

static const char* focusModes[] = {
	"Normal",
	"Grab",
	"Ungrab",
	"WhileGrabbed",
};

static const char* focusDetail[] = {
	"Ancestor",
	"Virtual",
	"Inferior",
	"Nonlinear",
	"NonlinearVirtual",
	"Pointer",
	"PointerRoot",
	"DetailNone",
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
	CARD16 serial; // The serial of the last sent request (as seen by the server)
	CARD16 serialLast; // The serial of the last received reply
	CARD16 serialDelta;
	unsigned char skip[1<<16];
	Window grabWindow;
} X11ConnData;

enum
{
	Note_None,
	Note_X_GetGeometry,
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
	Note_X_RRGetScreenResourcesCurrent,
	Note_X_XineramaQueryScreens,
	Note_X_GrabPointer,
	Note_NV_GLX,
};

// definition stolen from libX11/src/Xatomtype.h
typedef struct {
    CARD32 flags;
    INT32 x, y, width, height;
    INT32 minWidth, minHeight;
    INT32 maxWidth, maxHeight;
    INT32 widthInc, heightInc;
    INT32 minAspectX, minAspectY;
    INT32 maxAspectX, maxAspectY;
    INT32 baseWidth,baseHeight;
    CARD32 winGravity;
} xPropSizeHints;

#define PMinSize	(1L << 4) /* program specified minimum size */
#define PMaxSize	(1L << 5) /* program specified maximum size */

static void debugPropSizeHints(xPropSizeHints* hints) {
	log_debug2(
		"PropSizeHints: flags=0x%"PRIxCARD32" pos=%"PRIuCARD32"x%"PRIuCARD32" size=%"PRIuCARD32"x%"PRIuCARD32
		" min_size=%"PRIuCARD32"x%"PRIuCARD32" base_size=%"PRIuCARD32"x%"PRIuCARD32
		" max_size=%"PRIuCARD32"x%"PRIuCARD32" size_inc=%"PRIuCARD32"x%"PRIuCARD32
		" min_aspect=%"PRIuCARD32"x%"PRIuCARD32" max_aspect=%"PRIuCARD32"x%"PRIuCARD32" win_gravity=%"PRIuCARD32"\n",
		hints->flags,
		hints->x, hints->y,
		hints->width, hints->height,
		hints->minWidth, hints->minHeight,
		hints->baseWidth, hints->baseHeight,
		hints->maxWidth, hints->maxHeight,
		hints->widthInc, hints->heightInc,
		hints->minAspectX, hints->minAspectY,
		hints->maxAspectX, hints->maxAspectY,
		hints->winGravity
	);
}

static CARD16 injectRequest(X11ConnData *data, void* buf, size_t size)
{
	struct Connection conn = {};
	conn.recvfd = data->client;
	conn.sendfd = data->server;
	conn.dir = '{';

	const xReq* req = (xReq*)buf;
	sendAll(&conn, req, size);
	CARD16 sequenceNumber = ++data->serial;
	data->skip[sequenceNumber] = true;
	log_debug2("[%d][%d] Injected request %d (%s) with data %d, length %zu\n", data->index, sequenceNumber, req->reqType, requestNames[req->reqType], req->data, size);
	return sequenceNumber;
}

static CARD16 injectReply(X11ConnData *data, void* buf, size_t size)
{
	struct Connection conn = {};
	conn.recvfd = data->server;
	conn.sendfd = data->client;
	conn.dir = '}';

	xReply* reply = (xReply*)buf;
	reply->generic.sequenceNumber = data->serial - data->serialDelta--;
	reply->generic.length = ((size < sz_xReply ? sz_xReply : size) - sz_xReply + 3) / 4;
	sendAll(&conn, reply, size);
	log_debug2("[%d][%d] Injected reply %d (%s) with data %d, length %zu\n",
		data->index, reply->generic.sequenceNumber, reply->generic.type, responseNames[reply->generic.type], reply->generic.data1, size);
	return reply->generic.sequenceNumber;
}

static void injectEvent(X11ConnData *data, xEvent* event)
{
	struct Connection conn = {};
	conn.recvfd = data->server;
	conn.sendfd = data->client;
	conn.dir = '}';

	sendAll(&conn, event, sizeof(xEvent));
	log_debug2("[%d] Injected event %d (%s) with data %d\n",
		data->index, event->u.u.type, responseNames[event->u.u.type], event->u.u.detail);
}

static void grabPointer(X11ConnData* data, Window window)
{
	xGrabPointerReq req;
	req.reqType = X_GrabPointer;
	req.ownerEvents = true; // ?
	req.length = sizeof(req)/4;
	req.grabWindow = window;
	req.eventMask = ~0xFFFF8003;
	req.pointerMode = 1 /* Asynchronous */;
	req.keyboardMode = 1 /* Asynchronous */;
	req.confineTo = window;
	req.cursor = None;
	req.time = CurrentTime;
	CARD16 serial = injectRequest(data, &req, sizeof(req));
	data->notes[serial] = Note_X_GrabPointer;
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

	xReq* req = (xReq*)data->buf;
	uint requestLength = req->length * 4;
	if (requestLength == 0) // Big Requests Extension
	{
		recvAll(&conn, data->buf+ofs, 4);
		requestLength = *(uint*)(data->buf+ofs) * 4;
		ofs += 4;
	}
	CARD16 sequenceNumber = ++data->serial;
	log_debug2("[%d][%d] Request %d (%s) with data %d, length %d\n", data->index, sequenceNumber, req->reqType, requestNames[req->reqType], req->data, requestLength);
	/* log_debug2("  [server: %d] <- [client: %d]\n", sequenceNumber, sequenceNumber - data->serialDelta); */
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

			log_debug2(" XConfigureWindow(%dx%d @ %dx%d mask=0x%04X )\n", *w, *h, *x, *y, req->mask);
			fixCoords(x, y, w, h);
			log_debug2(" ->              (%dx%d @ %dx%d)\n", *w, *h, *x, *y);

			if ((req->mask & 0x000C) == 0x000C && *w == 0 && *h == 0)
				__builtin_trap();

			break;
		}

		// Fix for games setting their window size based on the X root window size
		// (which can encompass multiple physical monitors).
		case X_GetGeometry:
		{
			data->notes[sequenceNumber] = Note_X_GetGeometry;
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
			log_debug2(" XChangeProperty: property=%"PRIuCARD32" type=%"PRIuCARD32" format=%d)\n", req->property, req->type, req->format);
			if (req->type == XA_WM_SIZE_HINTS)
			{
				xPropSizeHints* hints = (xPropSizeHints*)(data->buf + sz_xChangePropertyReq);

				debugPropSizeHints(hints);
				fixCoords((INT16*)&hints->x, (INT16*)&hints->y, (CARD16*)&hints->width, (CARD16*)&hints->height);

				fixSize((CARD16*)&hints->maxWidth, (CARD16*)&hints->maxHeight);
				fixSize((CARD16*)&hints->baseWidth, (CARD16*)&hints->baseHeight);
				if (config.noMinSize) {
					hints->flags &= ~PMinSize;
					hints->minWidth = 0;
					hints->minHeight = 0;
				}
				if (config.noMaxSize) {
					hints->flags &= ~PMaxSize;
					hints->maxWidth = 0;
					hints->maxHeight = 0;
				}
				debugPropSizeHints(hints);
			}
			break;
		}

		case X_UngrabPointer:
		{
			if (config.confineMouse)
			{
				log_debug2(" X_UngrabPointer: Stubbing client request\n");
				req->reqType = X_NoOperation;
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
			if (config.noPrimarySelection)
			{
				xResourceReq* req = (xResourceReq*)data->buf;
				log_debug(" Selection atom: %"PRIuCARD32"\n", req->id);
				if (req->id == XA_PRIMARY)
				{
					req->id = XA_CUT_BUFFER0;
					log_debug(" -> Replaced atom: %"PRIuCARD32"\n", req->id);
				}
			}
			break;
		}
		case X_SetSelectionOwner:
		{
			if (config.noPrimarySelection)
			{
				xSetSelectionOwnerReq* req = (xSetSelectionOwnerReq*)data->buf;
				log_debug(" Selection atom: %"PRIuCARD32"\n", req->selection);
				if (req->selection == XA_PRIMARY)
				{
					req->selection = XA_CUT_BUFFER1;
					log_debug(" -> Replaced atom: %"PRIuCARD32"\n", req->selection);
				}
			}
			break;
		}
		case X_ConvertSelection:
		{
			if (config.noPrimarySelection)
			{
				xConvertSelectionReq* req = (xConvertSelectionReq*)data->buf;
				log_debug(" Selection atom: %"PRIuCARD32"\n", req->selection);
				if (req->selection == XA_PRIMARY)
				{
					req->selection = XA_CUT_BUFFER2;
					log_debug(" -> Replaced atom: %"PRIuCARD32"\n", req->selection);
				}
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
					case X_RRSetScreenConfig:
						if (config.noResolutionChange) // TODO: X_RRSetScreenSize, X_RRSetCrtcConfig
						{
							xRRSetScreenConfigReq* req = (xRRSetScreenConfigReq*)data->buf;
						//	req->sizeID = -1;
							xRRSetScreenConfigReply reply = {0};
							reply.type = X_Reply;
							reply.status = 0; // Success
							reply.newTimestamp = req->timestamp;
							reply.newConfigTimestamp = req->configTimestamp;
							reply.root = req->drawable;
							injectReply(data, &reply, sizeof(reply));
							return true;
						}
						break;
					case X_RRGetScreenInfo:
						data->notes[sequenceNumber] = Note_X_RRGetScreenInfo;
						break;
					case X_RRGetScreenResources:
						data->notes[sequenceNumber] = Note_X_RRGetScreenResources;
						break;
					case X_RRGetCrtcInfo:
						data->notes[sequenceNumber] = Note_X_RRGetCrtcInfo;
						break;
					case X_RRGetScreenResourcesCurrent:
						data->notes[sequenceNumber] = Note_X_RRGetScreenResourcesCurrent;
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
	log_debug2(" [%d]Response: %d (%s) sequenceNumber=%d length=%zu\n",
		data->index, reply->generic.type, responseNames[reply->generic.type], reply->generic.sequenceNumber, ofs);

	bool serialIsValid = true;

	switch (reply->generic.type)
	{
		case X_Error:
		{
			xError* err = (xError*)data->buf;
			log_debug2(" [%d] Error - code=%d resourceID=0x%"PRIxCARD32" minorCode=%d majorCode=%d (%s)\n",
				data->index, err->errorCode, err->resourceID, err->minorCode, err->majorCode, requestNames[err->majorCode]);
			break;
		}

		case X_Reply:
		{
			switch (data->notes[reply->generic.sequenceNumber])
			{
				case Note_X_GetGeometry:
				{
					xGetGeometryReply* r = &reply->geom;
					log_debug2("  XGetGeometry(%d,%d,%d,%d)\n", r->x, r->y, r->width, r->height);
					fixCoords(&r->x, &r->y, &r->width, &r->height);
					log_debug2("  ->          (%d,%d,%d,%d)\n", r->x, r->y, r->width, r->height);
					break;
				}

				case Note_X_InternAtom_Other:
				{
					xInternAtomReply* r = &reply->atom;
					log_debug2("  X_InternAtom: atom=%"PRIuCARD32"\n", r->atom);
					break;
				}

				case Note_X_QueryExtension_XFree86_VidModeExtension:
				{
					xQueryExtensionReply* r = &reply->extension;
					log_debug2("  X_QueryExtension (XFree86-VidModeExtension): present=%d major_opcode=%d first_event=%d first_error=%d\n",
						r->present, r->major_opcode, r->first_event, r->first_error);
					if (r->present)
						data->opcode_XFree86_VidModeExtension = r->major_opcode;
					break;
				}

				case Note_X_QueryExtension_RANDR:
				{
					xQueryExtensionReply* r = &reply->extension;
					log_debug2("  X_QueryExtension (RANDR): present=%d major_opcode=%d first_event=%d first_error=%d\n",
						r->present, r->major_opcode, r->first_event, r->first_error);
					if (r->present)
						data->opcode_RANDR = r->major_opcode;
					break;
				}

				case Note_X_QueryExtension_Xinerama:
				{
					xQueryExtensionReply* r = &reply->extension;
					log_debug2("  X_QueryExtension (XINERAMA): present=%d major_opcode=%d first_event=%d first_error=%d\n",
						r->present, r->major_opcode, r->first_event, r->first_error);
					if (r->present)
						data->opcode_Xinerama = r->major_opcode;
					break;
				}

				case Note_X_QueryExtension_NV_GLX:
				{
					xQueryExtensionReply* r = &reply->extension;
					log_debug2("  X_QueryExtension (NV-GLX): present=%d major_opcode=%d first_event=%d first_error=%d\n",
						r->present, r->major_opcode, r->first_event, r->first_error);
					if (r->present)
						data->opcode_NV_GLX = r->major_opcode;
					break;
				}

				case Note_X_QueryExtension_Other:
				{
					xQueryExtensionReply* r = &reply->extension;
					log_debug2("  X_QueryExtension: present=%d major_opcode=%d first_event=%d first_error=%d\n",
						r->present, r->major_opcode, r->first_event, r->first_error);
					break;
				}

				case Note_X_XF86VidModeGetModeLine:
				{
					xXF86VidModeGetModeLineReply* r = (xXF86VidModeGetModeLineReply*)reply;
					log_debug2("  X_XF86VidModeGetModeLine(%d x %d)\n", r->hdisplay, r->vdisplay);
					fixSize(&r->hdisplay, &r->vdisplay);
					log_debug2("  ->                      (%d x %d)\n", r->hdisplay, r->vdisplay);
					break;
				}

				case Note_X_XF86VidModeGetAllModeLines:
				{
					xXF86VidModeGetAllModeLinesReply* r = (xXF86VidModeGetAllModeLinesReply*)reply;
					xXF86VidModeModeInfo* modeInfos = (xXF86VidModeModeInfo*)(data->buf + sz_xXF86VidModeGetAllModeLinesReply);
					for (size_t i=0; i<r->modecount; i++)
					{
						xXF86VidModeModeInfo* modeInfo = modeInfos + i;
						log_debug2("  X_XF86VidModeGetAllModeLines[%zu] = %d x %d\n", i, modeInfo->hdisplay, modeInfo->vdisplay);
						fixSize(&modeInfo->hdisplay, &modeInfo->vdisplay);
						log_debug2("  ->                                %d x %d\n",    modeInfo->hdisplay, modeInfo->vdisplay);
					}
					break;
				}

				case Note_X_RRGetScreenInfo:
				{
					xRRGetScreenInfoReply* r = (xRRGetScreenInfoReply*)reply;
					xScreenSizes* sizes = (xScreenSizes*)(data->buf+sz_xRRGetScreenInfoReply);
					for (size_t i=0; i<r->nSizes; i++)
					{
						xScreenSizes* size = sizes+i;
						log_debug2("  X_RRGetScreenInfo[%zu] = %d x %d\n", i, size->widthInPixels, size->heightInPixels);
						fixSize(&size->widthInPixels, &size->heightInPixels);
						log_debug2("  ->                      %d x %d\n",    size->widthInPixels, size->heightInPixels);
					}
					break;
				}

				case Note_X_RRGetScreenResources:
				{
					xRRGetScreenResourcesReply* r = (xRRGetScreenResourcesReply*)reply;
					void* ptr = data->buf+sz_xRRGetScreenResourcesReply;
					ptr += r->nCrtcs * sizeof(CARD32);
					ptr += r->nOutputs * sizeof(CARD32);
					for (size_t i=0; i<r->nModes; i++)
					{
						xRRModeInfo* modeInfo = (xRRModeInfo*)ptr;
						log_debug2("  X_RRGetScreenResources[%zu] = %d x %d\n", i, modeInfo->width, modeInfo->height);
						fixSize(&modeInfo->width, &modeInfo->height);
						log_debug2("  ->                           %d x %d\n",    modeInfo->width, modeInfo->height);
						ptr += sz_xRRModeInfo;
					}
					break;
				}

				case Note_X_RRGetCrtcInfo:
				{
					xRRGetCrtcInfoReply* r = (xRRGetCrtcInfoReply*)reply;
					log_debug2("  X_RRGetCrtcInfo = %dx%d @ %dx%d\n", r->width, r->height, r->x, r->y);
					if (r->mode != None)
					{
						if (!fixMonitor(&r->x, &r->y, &r->width, &r->height))
						{
							r->x = r->y = r->width = r->height = 0;
							r->mode = None;
							r->rotation = r->rotations = RR_Rotate_0;
							r->nOutput = r->nPossibleOutput = 0;
						}
					}
					log_debug2("  ->                %dx%d @ %dx%d\n", r->width, r->height, r->x, r->y);
					break;
				}

				case Note_X_RRGetScreenResourcesCurrent: // Note: identical to RRGetScreenResources
				{
					xRRGetScreenResourcesCurrentReply* r = (xRRGetScreenResourcesCurrentReply*)reply;
					void* ptr = data->buf+sz_xRRGetScreenResourcesCurrentReply;
					ptr += r->nCrtcs * sizeof(CARD32);
					ptr += r->nOutputs * sizeof(CARD32);
					for (size_t i=0; i<r->nModes; i++)
					{
						xRRModeInfo* modeInfo = (xRRModeInfo*)ptr;
						log_debug2("  X_RRGetScreenResourcesCurrent[%zu] = %d x %d\n", i, modeInfo->width, modeInfo->height);
						fixSize(&modeInfo->width, &modeInfo->height);
						log_debug2("  ->                           %d x %d\n",    modeInfo->width, modeInfo->height);
						ptr += sz_xRRModeInfo;
					}
					break;
				}

				case Note_X_XineramaQueryScreens:
				{
					xXineramaQueryScreensReply* r = (xXineramaQueryScreensReply*)reply;
					xXineramaScreenInfo* screens = (xXineramaScreenInfo*)(data->buf+sz_XineramaQueryScreensReply);
					for (size_t i=0; i<r->number; i++)
					{
						xXineramaScreenInfo* screen = screens+i;
						log_debug2("  X_XineramaQueryScreens[%zu] = %dx%d @ %dx%d\n", i, screen->width, screen->height, screen->x_org, screen->y_org);
						fixCoords(&screen->x_org, &screen->y_org, &screen->width, &screen->height);
						log_debug2("  ->                           %dx%d @ %dx%d\n",    screen->width, screen->height, screen->x_org, screen->y_org);
					}
					break;
				}

				case Note_X_GrabPointer:
				{
					xGrabPointerReply* r = (xGrabPointerReply*)reply;
					log_debug2("  X_GrabPointer: status=%d\n", r->status);
					if (r->status == AlreadyGrabbed && data->grabWindow != 0)
					{
						log_debug("Retrying mouse grab\n");
						grabPointer(data, data->grabWindow);
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

		case KeymapNotify:
			serialIsValid = false;
			break;

		case FocusIn:
			log_debug2("FocusIn: window=%"PRIxCARD32" mode=%s detail=%s\n",
				reply->event.u.focus.window,
				focusModes[reply->event.u.focus.mode],
				focusDetail[reply->event.u.u.detail]);
			if (config.confineMouse
				&& reply->event.u.focus.mode == NotifyNormal
				&& reply->event.u.u.detail == NotifyNonlinear)
			{
				log_debug("Grabbing mouse grab\n");
				grabPointer(data, reply->event.u.focus.window);
				data->grabWindow = reply->event.u.focus.window;
			}
			break;

		case FocusOut:
			log_debug2("FocusOut: window=%"PRIxCARD32" mode=%s detail=%s\n",
				reply->event.u.focus.window,
				focusModes[reply->event.u.focus.mode],
				focusDetail[reply->event.u.u.detail]);
			if (config.confineMouse
				&& (reply->event.u.focus.mode == NotifyNormal
				 || reply->event.u.focus.mode == NotifyWhileGrabbed)
				&& reply->event.u.u.detail == NotifyNonlinear)
			{
				log_debug("Releasing mouse grab\n");

				xResourceReq req;
				req.reqType = X_UngrabPointer;
				req.length = sizeof(req)/4;
				req.id = CurrentTime;
				injectRequest(data, &req, sizeof(req));

				data->grabWindow = 0;
			}

			if (config.filterFocus)
			{
				log_debug("Filtering out FocusOut event\n");
				return true;
			}
			break;

		case KeyPress:
		case KeyRelease:
		case ButtonPress:
		case ButtonRelease:
			if (config.maps)
			{
				bool isPress = reply->generic.type == KeyPress || reply->generic.type == ButtonPress;
				bool isButton = reply->generic.type == ButtonPress || reply->generic.type == ButtonRelease;
				int kind = isButton ? MAP_KIND_BUTTON : MAP_KIND_KEY;

				bool found = false;
				for (struct MapConfig *map = config.maps; map; map = map->next)
					if (map->from.kind == kind && map->from.code == reply->event.u.u.detail)
					{
						static const char *kindNames[] = { "key", "button" };
						log_debug("Mapping %s %u to %s %u\n",
							kindNames[map->from.kind], map->from.code,
							kindNames[map->to.kind], map->to.code);

						xEvent injected = reply->event;
						injected.u.u.type = isPress
							? map->to.kind == MAP_KIND_BUTTON ? ButtonPress : KeyPress
							: map->to.kind == MAP_KIND_BUTTON ? ButtonRelease : KeyRelease;
						injected.u.u.detail = map->to.code;
						injectEvent(data, &injected);
						found = true;
					}

				if (found)
				{
					log_debug("Filtering out mapped input event\n");
					return true;
				}
			}
			break;
	}

	if (config.debug >= 2 && config.actualX && config.actualY && memmem(data->buf, ofs, &config.actualX, 2) && memmem(data->buf, ofs, &config.actualY, 2))
		log_debug2("   Found actualW/H in output! ----------------------------------------------------------------------------------------------\n");

	if (serialIsValid)
	{
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

		/* CARD16 oldSerial = reply->generic.sequenceNumber; */
		reply->generic.sequenceNumber -= data->serialDelta;
		/* log_debug2("  [server: %d] -> [client: %d]\n", oldSerial, reply->generic.sequenceNumber); */
	}

	if (!sendAll(&conn, data->buf, ofs)) return false;

	return true;
}

static void* workThreadProc(void* dataPtr)
{
	X11ConnData* data = (X11ConnData*)dataPtr;

	bufSize(&data->buf, &data->bufLen, 1<<16);

	fd_set readSet;
	FD_ZERO(&readSet);

	struct pollfd fds[2] = {
		{
			.fd = data->client,
			.events = POLLRDNORM,
		},
		{
			.fd = data->server,
			.events = POLLRDNORM,
		},
	};

	while (true)
	{
		if (poll(fds, 2, -1) < 0)
		{
			log_error("select() failed");
			break;
		}

		if (fds[0].revents & (POLLERR|POLLHUP|POLLNVAL))
		{
			log_debug("Error on client socket\n");
			break;
		}
		if (fds[1].revents & (POLLERR|POLLHUP|POLLNVAL))
		{
			log_debug("Error on server socket\n");
			break;
		}

		if (fds[0].revents)
			if (!handleClientData(data))
			{
				log_debug("End of client data\n");
				break;
			}
		if (fds[1].revents)
			if (!handleServerData(data))
			{
				log_debug("End of server data\n");
				break;
			}
	}

	log_debug("Exiting work thread.\n");
	shutdown(data->client, SHUT_RDWR);
	shutdown(data->server, SHUT_RDWR);
	close(data->client);
	close(data->server);
	return NULL;
}

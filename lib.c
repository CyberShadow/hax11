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

	char enable;
	char debug;
	char joinMST;
	char maskOtherMonitors;
	char resizeWindows;
	char resizeAll;
	char moveWindows;
	char fork;
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
		PARSE_INT(actualX)
		PARSE_INT(actualY)
		PARSE_INT(enable)
		PARSE_INT(debug)
		PARSE_INT(joinMST)
		PARSE_INT(maskOtherMonitors)
		PARSE_INT(resizeWindows)
		PARSE_INT(resizeAll)
		PARSE_INT(moveWindows)
		PARSE_INT(fork)
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
	if (*width == 1920 && *height == 2160) // Is 4K MST panel?
	{
		if (config.joinMST)
		{
			if (*x == config.mainX) // Left panel
			{
				*width = 1920 * 2; // resize
				//*height = 2160;
			}
			else
			if (*x == config.mainX + 1920) // Right panel
				*x = *y = *width = *height = 0; // disable
		}
	}
	else
	{
		if (config.maskOtherMonitors)
			*x = *y = *width = *height = 0; // disable
	}
}

#include <sys/socket.h>

static char sendAll(int fd, const void* buf, size_t length)
{
	int remaining = length;
	while (remaining)
	{
		int len = send(fd, buf, remaining, MSG_NOSIGNAL);
		if (len <= 0)
			return 0;
		buf += len;
		remaining -= len;
	}
	return 1;
}

static char recvAll(int fd, void* buf, size_t length)
{
	int remaining = length;
	while (remaining)
	{
		int len = recv(fd, buf, remaining, 0);
		if (len <= 0)
			return 0;
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
	"DestroySubWindows",
	"ChangeSaveSet",
	"ReparentWindow",
	"MapWindow",
	"MapSubwindows",
	"UnmapWindow", // 10
	"UnmapSubwindows",
	"ConfigureWindow",
	"CirculateWindow",
	"GetGeometry",
	"QueryTree",
	"InternAtom",
	"GetAtomName",
	"ChangeProperty",
	NULL,
	"GetProperty", // 20
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL, // 30
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"QueryPointer",
	NULL,
	NULL, // 40
	NULL,
	NULL,
	"GetInputFocus",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL, // 50
	NULL,
	NULL,
	NULL,
	NULL,
	"CreateGC",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL, // 60
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL, // 70
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL, // 80
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL, // 90
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"QueryExtension",
	NULL,
	NULL, // 100
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL, // 110
	NULL,
	NULL,
	NULL,
	NULL,
	"ForceScreenSaver",
	NULL,
	NULL,
	NULL,
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
	NULL,
	NULL, // 130
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL, // 140
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL, // 150
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL, // 160
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL, // 170
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL, // 180
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL, // 190
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL, // 200
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
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

int strmemcmp(const char* str, const void* mem, size_t meml)
{
	size_t strl = strlen(str);
	if (strl != meml)
		return strl - meml;
	return memcmp(str, mem, meml);
}

typedef struct
{
	int index;
	int server, client;
	char exiting;
	unsigned char notes[1<<16];
	unsigned char opcode_XFree86_VidModeExtension;
	unsigned char opcode_RANDR;
	unsigned char opcode_Xinerama;
	unsigned char opcode_NV_GLX;
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
	Note_X_XineramaQueryScreens,
	Note_NV_GLX,
};

static void* x11connThreadReadProc(void* dataPtr)
{
	X11ConnData* data = (X11ConnData*)dataPtr;
	unsigned char *buf = NULL;
	size_t bufLen = 0;
	bufSize(&buf, &bufLen, 1<<16);

	xConnClientPrefix header;
	if (!recvAll(data->client, &header, sizeof(header))) goto done;
	if (header.byteOrder != 'l')
	{
		log_debug("Unsupported byte order %c!\n", header.byteOrder);
		goto done;
	}
	if (!sendAll(data->server, &header, sz_xConnClientPrefix)) goto done;

	if (!recvAll(data->client, buf, pad(header.nbytesAuthProto))) goto done;
	if (!sendAll(data->server, buf, pad(header.nbytesAuthProto))) goto done;
	if (!recvAll(data->client, buf, pad(header.nbytesAuthString))) goto done;
	if (!sendAll(data->server, buf, pad(header.nbytesAuthString))) goto done;

	unsigned short sequenceNumber = 0;

	while (!data->exiting)
	{
		sequenceNumber++;

		size_t ofs = 0;
		if (!recvAll(data->client, buf+ofs, sz_xReq)) goto done;
		ofs += sz_xReq;

		const xReq* req = (xReq*)buf;
		uint requestLength = req->length * 4;
		if (requestLength == 0) // Big Requests Extension
		{
			recvAll(data->client, buf+ofs, 4);
			requestLength = *(uint*)(buf+ofs) * 4;
			ofs += 4;
		}
		log_debug2("[%d][%d] Request %d (%s) with data %d, length %d\n", data->index, sequenceNumber, req->reqType, requestNames[req->reqType], req->data, requestLength);
		bufSize(&buf, &bufLen, requestLength);
		req = (xReq*)buf; // in case bufSize moved buf

		if (!recvAll(data->client, buf+ofs, requestLength - ofs)) goto done;

		data->notes[sequenceNumber] = Note_None;
		switch (req->reqType)
		{
			// Fix for games that create the window of the wrong size or on the wrong monitor.
			case X_CreateWindow:
			{
				xCreateWindowReq* req = (xCreateWindowReq*)buf;
				log_debug2(" XCreateWindow(%dx%d @ %dx%d)\n", req->width, req->height, req->x, req->y);
				fixCoords(&req->x, &req->y, &req->width, &req->height);
				log_debug2(" ->           (%dx%d @ %dx%d)\n", req->width, req->height, req->x, req->y);
				break;
			}

			case X_ConfigureWindow:
			{
				xConfigureWindowReq* req = (xConfigureWindowReq*)buf;

				INT16 dummyXY = 0;
				CARD16 dummyW = config.mainW;
				CARD16 dummyH = config.mainH;
				INT16 *x = &dummyXY, *y = &dummyXY;
				CARD16 *w = &dummyW, *h = &dummyH;

				int* ptr = (int*)(buf + sz_xConfigureWindowReq);
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

			case X_InternAtom:
			{
				xInternAtomReq* req = (xInternAtomReq*)buf;
				const char* name = (const char*)(buf + sz_xInternAtomReq);
				log_debug2(" XInternAtom: %.*s\n", req->nbytes, name);
				data->notes[sequenceNumber] = Note_X_InternAtom_Other;
				break;
			}

			case X_ChangeProperty:
			{
				xChangePropertyReq* req = (xChangePropertyReq*)buf;
				log_debug2(" XChangeProperty: property=%d type=%d format=%d)\n", req->property, req->type, req->format);
				if (req->type == XA_WM_SIZE_HINTS)
				{
					XSizeHints* data = (XSizeHints*)(buf + sz_xChangePropertyReq);
					fixCoords((INT16*)&data->x, (INT16*)&data->y, (CARD16*)&data->width, (CARD16*)&data->height);
					fixSize((CARD16*)&data->max_width, (CARD16*)&data->max_height);
					fixSize((CARD16*)&data->base_width, (CARD16*)&data->base_height);
				}
				break;
			}

			case X_QueryExtension:
			{
				xQueryExtensionReq* req = (xQueryExtensionReq*)buf;
				const char* name = (const char*)(buf + sz_xQueryExtensionReq);
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
			}

			case 0:
				break;

			default:
			{
				if (req->reqType == data->opcode_XFree86_VidModeExtension)
				{
					xXF86VidModeGetModeLineReq* req = (xXF86VidModeGetModeLineReq*)buf;
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
					fwrite(buf, 1, requestLength, f);
					fclose(f);
#endif
					data->notes[sequenceNumber] = Note_NV_GLX;
				}
				break;
			}
		}

		if (config.debug >= 2 && config.actualX && config.actualY && memmem(buf, requestLength, &config.actualX, 2) && memmem(buf, requestLength, &config.actualY, 2))
			log_debug2("   Found actualW/H in input! ----------------------------------------------------------------------------------------------\n");

		if (!sendAll(data->server, buf, requestLength)) goto done;
	}
done:
	log_debug("Exiting read thread.\n");
	data->exiting = 1;
	close(data->client);
	close(data->server);
	return NULL;
}

static void* x11connThreadWriteProc(void* dataPtr)
{
	X11ConnData* data = (X11ConnData*)dataPtr;

	unsigned char *buf = NULL;
	size_t bufLen = 0;

	{
		xConnSetupPrefix header;
		if (!recvAll(data->server, &header, sz_xConnSetupPrefix)) goto done;
		if (!sendAll(data->client, &header, sz_xConnSetupPrefix)) goto done;

		log_debug("Server connection setup reply: %d\n", header.success);

		size_t dataLength = header.length * 4;
		bufSize(&buf, &bufLen, dataLength);
		if (!recvAll(data->server, buf, dataLength)) goto done;
		if (!sendAll(data->client, buf, dataLength)) goto done;
	}

	bufSize(&buf, &bufLen, sz_xReply);
	while (!data->exiting)
	{
		if (!recvAll(data->server, buf, sz_xReply)) goto done;
		size_t ofs = sz_xReply;
		const xReply* reply = (xReply*)buf;

		if (reply->generic.type == X_Reply || reply->generic.type == GenericEvent)
		{
			size_t dataLength = reply->generic.length * 4;
			bufSize(&buf, &bufLen, ofs + dataLength);
			reply = (xReply*)buf; // in case bufSize moved buf
			if (!recvAll(data->server, buf+ofs, dataLength)) goto done;
			ofs += dataLength;
		}
		log_debug2(" [%d]Response: %d sequenceNumber=%d length=%d\n", data->index, reply->generic.type, reply->generic.sequenceNumber, ofs);

		if (reply->generic.type == X_Reply)
		{
			switch (data->notes[reply->generic.sequenceNumber])
			{
				case Note_X_GetGeometry:
				{
					xGetGeometryReply* reply = (xGetGeometryReply*)buf;
					log_debug2("  XGetGeometry(%d,%d,%d,%d)\n", reply->x, reply->y, reply->width, reply->height);
					fixCoords(&reply->x, &reply->y, &reply->width, &reply->height);
					log_debug2("  ->          (%d,%d,%d,%d)\n", reply->x, reply->y, reply->width, reply->height);
					break;
				}

				case Note_X_InternAtom_Other:
				{
					xInternAtomReply* reply = (xInternAtomReply*)buf;
					log_debug2("  X_InternAtom: atom=%d\n", reply->atom);
					break;
				}

				case Note_X_QueryExtension_XFree86_VidModeExtension:
				{
					xQueryExtensionReply* reply = (xQueryExtensionReply*)buf;
					log_debug2("  X_QueryExtension (XFree86-VidModeExtension): present=%d major_opcode=%d first_event=%d first_error=%d\n",
						reply->present, reply->major_opcode, reply->first_event, reply->first_error);
					if (reply->present)
						data->opcode_XFree86_VidModeExtension = reply->major_opcode;
					break;
				}

				case Note_X_QueryExtension_RANDR:
				{
					xQueryExtensionReply* reply = (xQueryExtensionReply*)buf;
					log_debug2("  X_QueryExtension (RANDR): present=%d major_opcode=%d first_event=%d first_error=%d\n",
						reply->present, reply->major_opcode, reply->first_event, reply->first_error);
					if (reply->present)
						data->opcode_RANDR = reply->major_opcode;
					break;
				}

				case Note_X_QueryExtension_Xinerama:
				{
					xQueryExtensionReply* reply = (xQueryExtensionReply*)buf;
					log_debug2("  X_QueryExtension (XINERAMA): present=%d major_opcode=%d first_event=%d first_error=%d\n",
						reply->present, reply->major_opcode, reply->first_event, reply->first_error);
					if (reply->present)
						data->opcode_Xinerama = reply->major_opcode;
					break;
				}

				case Note_X_QueryExtension_NV_GLX:
				{
					xQueryExtensionReply* reply = (xQueryExtensionReply*)buf;
					log_debug2("  X_QueryExtension (NV-GLX): present=%d major_opcode=%d first_event=%d first_error=%d\n",
						reply->present, reply->major_opcode, reply->first_event, reply->first_error);
					if (reply->present)
						data->opcode_NV_GLX = reply->major_opcode;
					break;
				}

				case Note_X_QueryExtension_Other:
				{
					xQueryExtensionReply* reply = (xQueryExtensionReply*)buf;
					log_debug2("  X_QueryExtension: present=%d major_opcode=%d first_event=%d first_error=%d\n",
						reply->present, reply->major_opcode, reply->first_event, reply->first_error);
					break;
				}

				case Note_X_XF86VidModeGetModeLine:
				{
					xXF86VidModeGetModeLineReply* reply = (xXF86VidModeGetModeLineReply*)buf;
					log_debug2("  X_XF86VidModeGetModeLine(%d x %d)\n", reply->hdisplay, reply->vdisplay);
					fixSize(&reply->hdisplay, &reply->vdisplay);
					log_debug2("  ->                      (%d x %d)\n", reply->hdisplay, reply->vdisplay);
					break;
				}

				case Note_X_XF86VidModeGetAllModeLines:
				{
					xXF86VidModeGetAllModeLinesReply* reply = (xXF86VidModeGetAllModeLinesReply*)buf;
					xXF86VidModeModeInfo* modeInfos = (xXF86VidModeModeInfo*)(buf + sz_xXF86VidModeGetAllModeLinesReply);
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
					xRRGetScreenInfoReply* reply = (xRRGetScreenInfoReply*)buf;
					xScreenSizes* sizes = (xScreenSizes*)(buf+sz_xRRGetScreenInfoReply);
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
					xRRGetScreenResourcesReply* reply = (xRRGetScreenResourcesReply*)buf;
					void* ptr = buf+sz_xRRGetScreenResourcesReply;
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
					xRRGetCrtcInfoReply* reply = (xRRGetCrtcInfoReply*)buf;
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
					xXineramaQueryScreensReply* reply = (xXineramaQueryScreensReply*)buf;
					xXineramaScreenInfo* screens = (xXineramaScreenInfo*)(buf+sz_XineramaQueryScreensReply);
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
					fwrite(buf, 1, ofs, f);
					fclose(f);
#endif
					break;
				}
			}
		}

		if (config.debug >= 2 && config.actualX && config.actualY && memmem(buf, ofs, &config.actualX, 2) && memmem(buf, ofs, &config.actualY, 2))
			log_debug2("   Found actualW/H in output! ----------------------------------------------------------------------------------------------\n");

		if (!sendAll(data->client, buf, ofs)) goto done;
	}
done:
	log_debug("Exiting write thread.\n");
	data->exiting = 1;
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

					pthread_t readThread, writeThread;
					NEXT(pthread, "/usr/lib/libpthread.so", pthread_create)
						(& readThread, &attr, x11connThreadReadProc, data);
					NEXT(pthread, "/usr/lib/libpthread.so", pthread_create)
						(&writeThread, NULL, x11connThreadWriteProc, data);
					NEXT(pthread, "/usr/lib/libpthread.so", pthread_attr_destroy)(&attr);

					if (config.fork)
					{
						log_debug("Joining read thread...\n");
						NEXT(pthread, "/usr/lib/libpthread.so", pthread_join)( readThread, NULL);
						// log_debug("Joining write thread...\n");
						// NEXT(pthread, "/usr/lib/libpthread.so", pthread_join)(writeThread, NULL);
						log_debug("Fork is exiting.\n");
						NEXT(pthread, "/usr/lib/libpthread.so", pthread_exit)(0);
						log_debug("I said, fork is exiting.\n");
						exit(0);
						log_debug("WTF?\n");
					}
				}
			}
		}
	}
	return result;
}

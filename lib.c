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

typedef struct
{
	int server, client;
} X11ConnData;

#include <sys/socket.h>

static char sendAll(int fd, const void* buf, size_t length)
{
	int remaining = length;
	while (remaining)
	{
		int len = send(fd, buf, remaining, 0);
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
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"ChangeProperty",
	NULL,
	NULL, // 20
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
	NULL,
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
	NULL,
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
	NULL,
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
	NULL,
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

	while (1)
	{
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
		//log_debug("Request %d (%s) with length %d\n", req->reqType, requestNames[req->reqType], requestLength);
		bufSize(&buf, &bufLen, requestLength);

		if (!recvAll(data->client, buf+ofs, requestLength - ofs)) goto done;

		switch (req->reqType)
		{
			case X_CreateWindow:
			{
				xCreateWindowReq* req = (xCreateWindowReq*)buf;
				fixCoords(&req->x, &req->y, &req->width, &req->height);
				break;
			}
		}

		if (!sendAll(data->server, buf, requestLength)) goto done;
	}
done:
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
	while (1)
	{
		if (!recvAll(data->server, buf, sz_xReply)) goto done;
		size_t ofs = sz_xReply;
		const xReply* reply = (xReply*)buf;

		if (reply->generic.type == X_Reply)
		{
			size_t dataLength = reply->generic.length * 4;
			bufSize(&buf, &bufLen, ofs + dataLength);
			if (!recvAll(data->server, buf+ofs, dataLength)) goto done;
			ofs += dataLength;
		}
		//log_debug(" Response: %d\n", reply->generic.type);
		if (!sendAll(data->client, buf, ofs)) goto done;
	}
done:
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

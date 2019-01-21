#include "common.c"

static const char *profile_name;

static void getProfileName(char *p, size_t size)
{
	strncpy(p, profile_name, size);
}

#include <sys/un.h>
#include <pthread.h>
#include <errno.h>

static void fail(int err, const char* func)
{
	log_error("%s failed (%d / %s)!\n",
		func, err, strerror(err));
	exit(1);
}

#define CHECKRET(expr, cond, err, func) ({ \
			int ret = (expr);              \
			if (!(cond))                   \
				fail((err), (func));       \
			ret;						   \
		})

void handleConnection(int client_socket)
{
	struct sockaddr_un address;
	int  socket_fd;

	needConfig();

	socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
	CHECKRET(socket_fd, ret >= 0, errno, "socket");

	memset(&address, 0, sizeof(struct sockaddr_un));

	address.sun_family = AF_UNIX;
	sprintf(address.sun_path, "/tmp/.X11-unix/X%d", 0);

	CHECKRET(connect(socket_fd,
            (struct sockaddr *) &address,
            sizeof(struct sockaddr_un)),
		ret == 0, errno, "connect");

	X11ConnData* data = calloc(1, sizeof(X11ConnData));
	static int index = 0;
	data->index = index++;
	data->server = socket_fd;
	data->client = client_socket;

	pthread_attr_t attr = {};
	CHECKRET(pthread_attr_init(&attr),
		ret == 0, ret, "pthread_attr_init");
	CHECKRET(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED),
		ret == 0, ret, "pthread_attr_setdetachstate");

	pthread_t workThread;
	CHECKRET(pthread_create
		(&workThread, &attr, workThreadProc, data),
		ret == 0, ret, "pthread_create");
	pthread_attr_destroy(&attr);
}

int main(int argc, const char **argv)
{
	if (argc != 3)
	{
		log_error("usage: %s PROFILE-NAME LISTEN-PATH\n", argv[0]);
		exit(1);
	}

	profile_name = argv[1];
	const char *socket_path = argv[2];

	struct sockaddr_un address;
	int socket_fd, connection_fd;
	socklen_t address_length;

	socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
	CHECKRET(socket_fd, ret >= 0, errno, "socket");

	unlink(socket_path);

	memset(&address, 0, sizeof(struct sockaddr_un));

	address.sun_family = AF_UNIX;
	strcpy(address.sun_path, socket_path);

	CHECKRET(bind(socket_fd,
			(struct sockaddr *) &address,
			sizeof(struct sockaddr_un)),
		ret == 0, errno, "bind");

	CHECKRET(listen(socket_fd, 5),
		ret == 0, errno, "listen");

	while (true)
	{
		connection_fd = accept(socket_fd,
			(struct sockaddr *) &address,
			&address_length);
		CHECKRET(connection_fd, ret >= 0, errno, "accept");
		handleConnection(connection_fd);
	}

	close(socket_fd);
	unlink(socket_path);
	return 0;
}

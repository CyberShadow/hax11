#include "common.c"

static void getProfileName(char *p, size_t size)
{
	readlink("/proc/self/exe", p, size);
	char *end = p + size;
	// Remove leading '/', replace remaining '/' to '\'
	for (; *p && p<end; p++)
		p[0] = p[1] == '/' ? '\\' : p[1];
}

#include <sys/un.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <errno.h>

#define NEXT(handle, path, func) ({										\
			static typeof(&func) pfunc = NULL;                           \
			if (!pfunc)                                                  \
				pfunc = (typeof(&func))dlsym(RTLD_NEXT, #func);          \
			if (!pfunc) {                                                \
				if (!handle) {                                           \
					handle = dlopen(path, RTLD_LAZY);                    \
				}                                                        \
				if (!handle) {                                           \
					needConfig();                                        \
					log_error("Ack, dlopen of '%s' failed: %s\n",        \
						path, dlerror());                                \
				}                                                        \
				pfunc = (typeof(&func))dlsym(handle, #func);             \
			}                                                            \
			if (!pfunc) {                                                \
				needConfig();                                            \
				log_error("Ack, dlsym of '%s' (from '%s') failed: %s\n", \
					#func, path, dlerror());                             \
			}                                                            \
			pfunc;                                                       \
		})

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

static void* libc = NULL;
static void* pthread = NULL;

int connect(int socket, const struct sockaddr *address,
	socklen_t address_len)
{
	int connect_result = NEXT(libc, LIBC_SO, connect)
		(socket, address, address_len);
	if (connect_result == 0)
	{
		if (address->sa_family == AF_UNIX)
		{
			struct sockaddr_un *ua = (struct sockaddr_un*)address;
			const char* path = ua->sun_path;
			if (!*path)
				path++;
			//log_debug("connect(%s,%d)\n", path, address_len);
			if (!strncmp(path, "/tmp/.X11-unix/X", 16))
			{
				needConfig();
				log_debug("Found X connection!\n");
				if (config.enable)
				{
					log_debug("Intercepting X connection!\n");

					int pair[2];
					CHECKRET(socketpair(AF_UNIX, SOCK_STREAM, 0, pair),
						ret == 0, ret, "socketpair");

					X11ConnData* data = calloc(1, sizeof(X11ConnData));
					static int index = 0;
					data->index = index++;
					data->server = dup(socket);
					data->client = pair[0];
					CHECKRET(dup2(pair[1], socket),
						ret >= 0, errno, "dup2");
					CHECKRET(close(pair[1]),
						ret == 0, errno, "close");

					if (config.fork)
					{
						log_debug("Forking...\n");

						pid_t pid = fork();
						CHECKRET(pid, ret >= 0, errno, "fork");
						if (pid == 0)
						{
							pid = getpid();
							log_debug("In child (%d), forking again\n", pid);
							pid_t pid2 = fork();
							CHECKRET(pid2, ret >= 0, errno, "fork");
							if (pid2)
							{
								log_debug("Second fork is %d\n", pid2);
								exit(0);
							}
							log_debug("In child, double-fork OK\n");
							struct rlimit r;
							CHECKRET(getrlimit(RLIMIT_NOFILE, &r),
								ret >= 0, errno, "getrlimit");

							for (int n=3; n<(int)r.rlim_cur; n++)
								if (n != data->server && n != data->client)
									close(n); // Ignore error

							log_debug("Running main loop.\n");
							workThreadProc(data);

							log_debug("Fork is exiting.\n");
							exit(0);
						}
						else
						{
							close(data->server);
							close(data->client);
							free(data);

							log_debug("In parent! Child is %d\n", pid);
							CHECKRET(waitpid(pid, NULL, 0),
								ret >= 0, errno, "waitpid");
							return connect_result;
						}
					}

					pthread_attr_t attr = {};
					CHECKRET(NEXT(pthread, LIBPTHREAD_SO, pthread_attr_init)(&attr),
						ret == 0, ret, "pthread_attr_init");
					CHECKRET(NEXT(pthread, LIBPTHREAD_SO, pthread_attr_setdetachstate)(&attr, PTHREAD_CREATE_DETACHED),
						ret == 0, ret, "pthread_attr_setdetachstate");

					pthread_t workThread;
					CHECKRET(NEXT(pthread, LIBPTHREAD_SO, pthread_create)
						(&workThread, &attr, workThreadProc, data),
						ret == 0, ret, "pthread_create");
					NEXT(pthread, LIBPTHREAD_SO, pthread_attr_destroy)(&attr);
				}
			}
		}
	}
	return connect_result;
}

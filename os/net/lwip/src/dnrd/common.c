/*
 * common.c - includes global variables and functions.
 */

#include <config.h>
#include <FreeRTOS.h>
#include <task.h>
#include "platform_opts.h"

#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "lwip/api.h"
#include "lwip/sys.h"
#include "osdep_service.h"

#include <stdarg.h>
//#include <unistd.h>
#include <stdio.h>
//#include <sys/socket.h>
//#include <sys/stat.h>
//#include <signal.h>
//#include <sys/types.h>
#include <stdlib.h>
//#include <assert.h>
#include <string.h>
//#include <errno.h>
#include "common.h"
#include "lib.h"

#ifdef DEBUG
#define OPT_DEBUG 2
#else
#define OPT_DEBUG 0
#endif /* DEBUG */

rtos_mutex_t dns_server_entry_lock;
int     isDNRDRunning = -1;
/*
 * These are all the global variables.
 */
int                 opt_debug = OPT_DEBUG;
//int                 opt_serv = 0;
//const char*         progname = 0;
//#if defined(__sun__)
//const char*         pid_file = "/var/tmp/dnrd.pid";
//#else
//const char*         pid_file = "/var/run/dnrd.pid";
//#endif
int                 isock = -1;

#ifdef ENABLE_TCP
int                 tcpsock = -1;
#endif
struct dnssrv_t     dns_srv[MAX_SERV];
int                 serv_act = 0;
unsigned int        serv_cnt = 0;
//uid_t               daemonuid = 0;
//gid_t               daemongid = 0;
const char         *dnrd_version = VERSION;
//int                 gotterminal = 1; /* 1 if attached to a terminal */
//sem_t               dnrd_sem;  /* Used for all thread synchronization */

/*
 * This is the address we listen on.  It gets initialized to INADDR_ANY,
 * which means we listen on all local addresses.  Both the address and
 * the port can be changed through command-line options.
 */

#if defined(__sun__)
struct sockaddr_in recv_addr = {0};
#else
struct sockaddr_in recv_addr = {0};
#endif




#if 0

/* check if a pid is running
 * from the unix faq
 * http://www.erlenstar.demon.co.uk/unix/faq_2.html#SEC18
 */

int isrunning(int pid)
{
	if (kill(pid, 0)) {
		if (errno == EPERM) {
			return 1;
		} else {
			return 0;
		}
	} else {
		return 1;
	}
}

/* wait_for_exit()
 *
 * In: pid     - the process id to wait for
 *     timeout - maximum time to wait in 1/100 secs
 *
 * Returns: 1 if it died in before timeout
 *
 * Abstract: Check if a process is running and wait til it does not
 */
int wait_for_exit(int pid, int timeout)
{
	while (timeout--) {
		if (! isrunning(pid)) {
			return 1;
		}
		usleep(10000);
	}
	/* ouch... we timed out */
	return 0;
}

/*
 * kill_current()
 *
 * Returns: 1 if a currently running dnrd was found & killed, 0 otherwise.
 *
 * Abstract: This function sees if pid_file already exists and, if it does,
 *           will kill the current dnrd process and remove the file.
 */
int kill_current(void)
{
	int         pid;
	int         retn;
	struct stat finfo;
	FILE       *filep;

	if (stat(pid_file, &finfo) != 0) {
		return 0;
	}

	filep = fopen(pid_file, "r");
	if (!filep) {
		log_msg(LOG_ERR, "%s: Can't open %s\n", progname, pid_file);
		exit(-1);
	}
	if ((retn = (fscanf(filep, "%i%*s", &pid) == 1))) {
		if (kill(pid, SIGTERM)) {
//	    log_msg(LOG_ERR, "Couldn't kill dnrd: %s", strerror(errno));
		}
		/* dnrd gets 4 seconds to die or we give up */
		if (!wait_for_exit(pid, 400)) {
			log_msg(LOG_ERR, "The dnrd process didn't die within 4 seconds");
		}
	}
	fclose(filep);
	unlink(pid_file);
	return retn;
}
#endif
/*
 * log_msg()
 *
 * In:      type - a syslog priority
 *          fmt  - a formatting string, ala printf.
 *          ...  - other printf-style arguments.
 *
 * Sends a message to stdout or stderr if attached to a terminal, otherwise
 * it sends a message to syslog.
 */
void log_msg(const char *fmt, ...)
{
#if 0
	va_list ap;

	va_start(ap, fmt);

	if (gotterminal) {
		const char *typestr;
		switch (type) {
		case LOG_EMERG:
			typestr = "EMERG: ";
			break;
		case LOG_ALERT:
			typestr = "ALERT: ";
			break;
		case LOG_CRIT:
			typestr = "CRIT:  ";
			break;
		case LOG_ERR:
			typestr = "ERROR: ";
			break;
		case LOG_WARNING:
			typestr = "Warning: ";
			break;
		case LOG_NOTICE:
			typestr = "Notice: ";
			break;
		case LOG_INFO:
			typestr = "Info:  ";
			break;
		case LOG_DEBUG:
			typestr = "Debug: ";
			break;
		default:
			typestr = "";
			break;
		}
		fprintf(stderr, typestr);
		vfprintf(stderr, fmt, ap);
		if (fmt[strlen(fmt) - 1] != '\n') {
			fprintf(stderr, "\n");
		}
	} else {
		vsyslog(type, fmt, ap);
	}
	va_end(ap);
#endif

	if (opt_debug < 2) {
		return;
	}
	// Print the data.
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);

	// Terminate the line.
	printf("\n\r");


}

/*
 * log_debug()
 *
 * In:      fmt - a formatting string, ala printf.
 *          ... - other printf-style arguments.
 *
 * Abstract: If debugging is turned on, this will send the message
 *           to syslog with LOG_DEBUG priority.
 */
void log_debug(const char *fmt, ...)
{
#if 0
	va_list ap;

	if (!opt_debug) {
		return;
	}

	va_start(ap, fmt);
	if (gotterminal) {
		fprintf(stderr, "Debug: ");
		vfprintf(stderr, fmt, ap);
		if (fmt[strlen(fmt) - 1] != '\n') {
			fprintf(stderr, "\n");
		}
	} else {
		vsyslog(LOG_DEBUG, fmt, ap);
	}
	va_end(ap);
#endif
// Print the data.
	if (opt_debug < 2) {
		return;
	}

	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);

	// Terminate the line.
	printf("\n\r");
}
#if 0
/*
 * cleanexit()
 *
 * In:      status - the exit code.
 *
 * Abstract: This closes our sockets, removes /var/run/dnrd.pid,
 *           and then calls exit.
 */
void cleanexit(int status)
{
	unsigned int i;

	/* Only let one process run this code) */
	//  sem_wait(&dnrd_sem);

	log_debug("Shutting down...\n");
	if (isock >= 0) {
		close(isock);
	}
#ifdef ENABLE_TCP
	if (tcpsock >= 0) {
		close(tcpsock);
	}
#endif
	for (i = 0; i < serv_cnt; i++) {
		close(dns_srv[i].sock);
	}
	exit(status);
}
#endif
/*
 * make_cname()
 *
 * In:       text - human readable domain name string
 *
 * Returns:  Pointer to the allocated, filled in character string on success,
 *           NULL on failure.
 *
 * Abstract: converts the human-readable domain name to the DNS CNAME
 *           form, where each node has a length byte followed by the
 *           text characters, and ends in a null byte.  The space for
 *           this new representation is allocated by this function.
 */
char *make_cname(const char *text)
{
	const char *tptr = text;
	const char *end = text;
	char *cname = (char *)malloc(strlen(text) + 2);
	char *cptr = cname;
	while (*end != 0) {
		size_t diff;
		end = strchr(tptr, '.');
		if (end == NULL) {
			end = text + strlen(text);
		}
		if (end < tptr + 2) {
			free(cname);
			return NULL;
		}
		diff = end - tptr;
		*cptr++ = diff;
		memcpy(cptr, tptr, diff);
		cptr += diff;
		tptr = end + 1;
	}
	*cptr = 0;
	//assert((unsigned)(cptr - cname) == strlen(text) + 1);
	return cname;
}

void sprintf_cname(const char *cname, int namesize, char *buf, int bufsize)
{
	const char *s = cname; /*source pointer */
	char *d = buf; /* destination pointer */

	if (cname == NULL) {
		return;
	}

	if ((strnlen(cname, namesize) + 1) > (unsigned)bufsize) {
		if (bufsize > 11) {
			sprintf(buf, "(too long)");
		} else {
			buf[0] = 0;
		}
		return;
	}

	/* extract the pascal style strings */
	while (*s) {
		int i;
		int size = *s;

		/* Let us see if we are bypassing end of buffer.  Also remember
		 * that we need space for an ending \0
		 */
		if ((s + *s - cname) >= (bufsize)) {
			if (bufsize > 15) {
				sprintf(buf, "(malformatted)");
			} else {
				buf[0] = 0;
			}
			return;
		}

		/* delimit the labels with . */
		if (s++ != cname) {
			sprintf(d++, ".");
		}

		for (i = 0; i < size; i++) {
			*d++ = *s++;
		}
		*d = 0;
	}
}


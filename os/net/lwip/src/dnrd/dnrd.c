/*
 * main.c - contains the main() function
 */

#include <FreeRTOS.h>
#include <task.h>
#include "platform_opts.h"

#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/inet.h"
//#include <sys/types.h>
#include "relay.h"
//#include "cache.h"
#include "common.h"
//#include "args.h"
//#include "sig.h"
#include "master.h"
#include <stdio.h>
#include <stdlib.h>
//#include <sys/stat.h>
#include "lwip/sockets.h"
#include "lwip/api.h"
#include "lwip/sys.h"
#include "lwip/dns.h"
#include "lwip/ip_addr.h"
#include "rtw_wifi_constants.h"
#include "lwip_netconf.h"
#include "wifi_conf.h"
#include <netdb.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>
//#include <unistd.h>
#include <string.h>
#include <errno.h>
//#include <grp.h>
//#include <pwd.h>
//#include <dirent.h>

#define DNRD_SERVICE_PRIORITY 3

extern rtw_mode_t wifi_mode;

#if defined(LINUX_OS)
/*
 * main() - startup the program.
 *
 * In:      argc - number of command-line arguments.
 *          argv - string array containing command-line arguments.
 *
 * Returns: 0 on exit, -1 on error.
 *
 * Abstract: We set up the signal handler, parse arguments,
 *           turn into a daemon, write our pid to /var/run/dnrd.pid,
 *           setup our sockets, and then parse packets until we die.
 */
int main(int argc, char *argv[])
{
	int                i;
	//FILE              *filep;
	//struct servent    *servent;   /* Let's be good and find the port numbers the right way */

	//struct passwd     *pwent;
	//DIR               *dirp;
	//struct dirent     *direntry;
	//struct stat        st;
	//int                rslt;
	/*
	  * Setup signal handlers.
	  */
	signal(SIGINT,  sig_handler);
	signal(SIGQUIT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGUSR1, sig_handler);
	/*
	 * Handling TCP requests is done by forking a child.  When they terminate
	 * they send SIGCHLDs to the parent.  This will eventually interrupt
	 * some system calls.  Because I don't know if this is handled it's better
	 * to ignore them -- 14OCT99wzk
	 */
	signal(SIGCHLD, SIG_IGN);

	/*
	 * Initialization in common.h of recv_addr is broken, causing at
	 * least the '-a' switch not to work.  Instead of assuming
	 * positions of fields in the struct across platforms I thought it
	 * safer to do a standard initialization in main().
	 */
	memset(&recv_addr, 0, sizeof(recv_addr));
	recv_addr.sin_family = AF_INET;
	recv_addr.sin_port = htons(53);

	/*
	 * Parse the command line.
	 */
	parse_args(argc, argv);

	openlog(progname, LOG_PID, LOG_DAEMON);

	/*
	 * Kill any currently running copies of the program.
	 */
	kill_current();

	/*
	 * Setup the thread synchronization semaphore
	 */
	/*
	if (sem_init(&dnrd_sem, 0, 1) == -1) {
	log_msg(LOG_ERR, "Couldn't initialize semaphore");
	cleanexit(-1);
	}
	*/

	/*
	 * Write our pid to the appropriate file.
	 * Just open the file here.  We'll write to it after we fork.
	 */
	filep = fopen(pid_file, "w");
	if (!filep) {
		log_msg(LOG_ERR, "can't write to %s.  "
				"Check that dnrd was started by root.", pid_file);
		exit(-1);
	}

	/*
	 * Pretend we don't know that we want port 53
	 */
	servent = getservbyname("domain", "udp");
	if (servent != getservbyname("domain", "tcp")) {
		log_msg(LOG_ERR, "domain ports for udp & tcp differ.  "
				"Check /etc/services");
		exit(-1);
	}
	recv_addr.sin_port = servent ? servent->s_port : htons(53);

	/*
	 * Setup our DNS query reception socket.
	 */
	if ((isock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		//log_msg(LOG_ERR, "isock: Couldn't open socket");
		//cleanexit(-1);
	} else {
		int opt = 1;
		setsockopt(isock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	}

	if (bind(isock, (struct sockaddr *)&recv_addr, sizeof(recv_addr)) < 0) {
		//log_msg(LOG_ERR, "isock: Couldn't bind local address");
		//cleanexit(-1);
	}
#ifdef ENABLE_TCP
	/*
	 * Setup our DNS tcp proxy socket.
	 */
	if ((tcpsock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		//log_msg(LOG_ERR, "tcpsock: Couldn't open socket");
		//cleanexit(-1);
	} else {
		int opt = 1;
		setsockopt(tcpsock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	}
	if (bind(tcpsock, (struct sockaddr *)&recv_addr, sizeof(recv_addr)) < 0) {
		//log_msg(LOG_ERR, "tcpsock: Couldn't bind local address");
		//cleanexit(-1);
	}
	if (listen(tcpsock, 5) != 0) {
		//log_msg(LOG_ERR, "tcpsock: Can't listen");
		//cleanexit(-1);
	}
#endif
	/* Initialise our cache */
	cache_init();

	/* Initialise out master DNS */
	master_init();



	pwent = getpwnam("root");


	/*
	 * Change our root and current working directories to /etc/dnrd.
	 * Also, so some sanity checking on that directory first.
	 */
	dirp = opendir("/etc/dnrd");
	if (!dirp) {
		log_msg(LOG_ERR, "The directory /etc/dnrd must be created before "
				"dnrd will run");
	}

	rslt = stat("/etc/dnrd", &st);
	if (st.st_uid != 0) {
		log_msg(LOG_ERR, "The /etc/dnrd directory must be owned by root");
		cleanexit(-1);
	}
	if ((st.st_mode & (S_IWGRP | S_IWOTH)) != 0) {
		log_msg(LOG_ERR,
				"The /etc/dnrd directory should only be user writable");
		cleanexit(-1);
	}

	while ((direntry = readdir(dirp)) != NULL) {

		if (!strcmp(direntry->d_name, ".") ||
			!strcmp(direntry->d_name, "..")) {
			continue;
		}

		rslt = stat(direntry->d_name, &st);

		if (rslt) {
			continue;
		}
		if (S_ISDIR(st.st_mode)) {
			log_msg(LOG_ERR, "The /etc/dnrd directory must not contain "
					"subdirectories");
			cleanexit(-1);
		}
		if ((st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH | S_IWGRP | S_IWOTH)) != 0) {
			log_msg(LOG_ERR, "A file in /etc/dnrd has either execute "
					"permissions or non-user write permission.  Please do a "
					"\"chmod a-x,go-w\" on all files in this directory");
			cleanexit(-1);
		}
		if (st.st_uid != 0) {
			log_msg(LOG_ERR, "All files in /etc/dnrd must be owned by root");
			cleanexit(-1);
		}
	}
	closedir(dirp);


	if (chdir("/etc/dnrd")) {
		log_msg(LOG_ERR, "couldn't chdir to %s, %s",
				"/etc/dnrd", strerror(errno));
		cleanexit(-1);
	}

	if (chroot("/etc/dnrd")) {
		log_msg(LOG_ERR, "couldn't chroot to %s, %s",
				"/etc/dnrd", strerror(errno));
		cleanexit(-1);
	}

	if (chdir("/")) {
		log_msg(LOG_ERR, "couldn't chdir to %s, %s",
				"/", strerror(errno));
		printf("couldn't chdir to %s, %s", "/", strerror(errno));
		cleanexit(-1);
	}
	if (chroot("/")) {
		log_msg(LOG_ERR, "couldn't chroot to %s, %s",
				"/", strerror(errno));
		printf("couldn't chroot to %s, %s", "/", strerror(errno));
		cleanexit(-1);
	}
	/*
	 * Change uid/gid to something other than root.
	 */

	/* drop supplementary groups */
	if (setgroups(0, NULL) < 0) {
		log_msg(LOG_ERR, "can't drop supplementary groups");
		cleanexit(-1);
	}

	/*
	 * Switch uid/gid to something safer than root if requested.
	 * By default, attempt to switch to user & group id 65534.
	 */

	if (daemongid != 0) {
		if (setgid(daemongid) < 0) {
			log_msg(LOG_ERR, "couldn't switch to gid %i", daemongid);
			cleanexit(-1);
		}
	} else if (!pwent) {
		log_msg(LOG_ERR, "Couldn't become the \"nobody\" user.  Please use "
				"the \"-uid\" option.\n"
				"       dnrd must become a non-root process.");
		cleanexit(-1);
	} else if (setgid(pwent->pw_gid) < 0) {
		log_msg(LOG_ERR, "couldn't switch to gid %i", pwent->pw_gid);
		cleanexit(-1);
	}



	if (daemonuid != 0) {
		if (setuid(daemonuid) < 0) {
			log_msg(LOG_ERR, "couldn't switch to uid %i", daemonuid);
			cleanexit(-1);
		}
	} else if (!pwent) {
		log_msg(LOG_ERR, "Couldn't become the \"nobody\" user.  Please use "
				"the \"-uid\" option.\n"
				"       dnrd must become a non-root process.");
		cleanexit(-1);
	} else if (setuid(pwent->pw_uid) < 0) {
		log_msg(LOG_ERR, "couldn't switch to uid %i", pwent->pw_uid);
		cleanexit(-1);
	}




	/*
	 * Setup our DNS query forwarding socket.
	 */
	for (i = 0; i < serv_cnt; i++) {
		if ((dns_srv[i].sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			//log_msg(LOG_ERR, "osock: Couldn't open socket");
			//cleanexit(-1);
		}

		dns_srv[i].addr.sin_family = AF_INET;
		dns_srv[i].addr.sin_port   = htons(53);
	}

	/*
	 * Now it's time to become a daemon.
	 */
	if (!opt_debug) {
		pid_t pid = fork();
		if (pid < 0) {
			log_msg(LOG_ERR, "%s: Couldn't fork\n", progname);
			exit(-1);
		}
		if (pid != 0) {
			exit(0);
		}
		gotterminal = 0;
		setsid();
		chdir("/");
		umask(077);
		fclose(stdin);
		fclose(stdout);
		fclose(stderr);
	}

	/*
	 * Write our pid to the appropriate file.
	 * Now we actually write to it and close it.
	 */
	fprintf(filep, "%i\n", (int)getpid());
	fclose(filep);
	/*
	 * Run forever.
	 */
	run();

//    exit(0); /* to make compiler happy */
}
#endif //defined(LINUX_OS)
void ip_nat_sync_dns_serever_data(void)
{
	int i;
	const ip_addr_t *pdns_server_ip;
	ip_addr_t dns_server_ip;
	if (isDNRDRunning == 1) {
		rtos_mutex_take(dns_server_entry_lock, MUTEX_WAIT_TIMEOUT);
		for (i = 0; i < MAX_SERV; i++) {
			dns_srv[i].addr.sin_addr.s_addr = 0x00;
		}

		serv_cnt = 0;
		for (i = 0; i < MAX_SERV; i++) {
			printf("\n%s - %d i %d\n",__FUNCTION__,__LINE__,i);
			pdns_server_ip = dns_getserver(i);
			if (pdns_server_ip->addr == 0x00) {
				printf("\n\r %s %d get end of dns server data!!!", __FUNCTION__, __LINE__);
				break;
			} else {

				memcpy(&dns_server_ip.addr, &pdns_server_ip->addr, 4);
				//printf("\n\r %s %d get dns server IP=%08X!!!", __FUNCTION__, __LINE__, dns_server_ip.addr);
				dns_srv[i].addr.sin_addr.s_addr = (u32_t)dns_server_ip.addr;
				log_msg("DNRD set DNS Server: %s", inet_ntoa(dns_srv[i].addr.sin_addr));
			}
			serv_cnt = i + 1;
		}
		rtos_mutex_give(dns_server_entry_lock);
	}
}

static int wifi_is_up(rtw_interface_t interface)
{
	switch (interface) {
	case RTW_AP_INTERFACE:
		switch (wifi_mode) {
		case RTW_MODE_AP:
			return wifi_is_running(WLAN1_IDX);
		case RTW_MODE_STA:
			return 0;
		default:
			return wifi_is_running(WLAN0_IDX);
		}
	case RTW_STA_INTERFACE:
		switch (wifi_mode) {
		case RTW_MODE_AP:
			return 0;
		default:
			return wifi_is_running(WLAN0_IDX);
		}
	default:
		return 0;
	}
}

static int wifi_is_ready_to_transceive(rtw_interface_t interface)
{
	u8 *ip = LwIP_GetIP(interface);

	switch (interface) {
	case RTW_AP_INTERFACE:
		return (wifi_is_up(interface) == TRUE) ? RTW_SUCCESS : RTW_ERROR;

	case RTW_STA_INTERFACE:
		if (wifi_is_connected_to_ap() != RTW_SUCCESS && ip[0] == 0) {
			return RTW_ERROR;
		} else {
			return RTW_SUCCESS;
		}
	default:
		return RTW_ERROR;
	}
}

void wait_wifi_ready(void)
{
	uint32_t wifi_wait_count = 0;
	uint32_t max_wifi_wait_time = 500;
	int timeout = 60;
	extern char *rptssid;
	while (1) {
		rtw_wifi_setting_t setting;
		wifi_get_setting(SOFTAP_WLAN_INDEX, &setting);
		if (strlen((const char *)setting.ssid) > 0) {
			if (strcmp((const char *) setting.ssid, (const char *)rptssid) == 0) {
				printf("\n\rDNRD %s AP started\n", rptssid);
				break;
			}
		}
		if (timeout == 0) {
			printf("\n\r DNRD ERROR: Start AP timeout\n");
			return;
		}
		sleep(1 * 1000);
		timeout --;
	}

	while (wifi_is_ready_to_transceive(RTW_STA_INTERFACE) != RTW_SUCCESS) {
		sleep(10);
		wifi_wait_count++;
		if (wifi_wait_count == max_wifi_wait_time) {
			printf("\r\nuse ATW0, ATW1, ATWC to make wifi connection\r\n");
			printf("wait for wifi connection...\r\n");
		}
	}
}


void dns_relay_service_running_start(void *param)
{
	(void) param;
	unsigned int i;

	const ip_addr_t *pdns_server_ip;
	ip_addr_t dns_server_ip;

	printf("\n\r");
	printf("\n\r%s(%d), Available heap %d", __FUNCTION__, __LINE__, xPortGetFreeHeapSize());
	printf("\n\r");

	wait_wifi_ready();
	sleep(1000);


	rtos_mutex_create(&dns_server_entry_lock);
	memset(&recv_addr, 0, sizeof(recv_addr));
	recv_addr.sin_family = AF_INET;
	recv_addr.sin_port = htons(53);
	/*
	* Setup our DNS query reception socket.
	*/
	if ((isock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("\n\r isock: Couldn't open socket");
	} else {
		int opt = 1;
		setsockopt(isock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	}
	if (bind(isock, (struct sockaddr *)&recv_addr, sizeof(recv_addr)) < 0) {
		printf("\n\r isock: Couldn't bind local address");
	}
#if 0
	/* Initialise our cache */
	cache_init();
#endif
	/* Initialise out master DNS */
	master_init();

	rtos_mutex_take(dns_server_entry_lock, MUTEX_WAIT_TIMEOUT);
	for (i = 0; i < MAX_SERV; i++) {
		pdns_server_ip = dns_getserver(i);
		if (pdns_server_ip->addr == 0x00) {
			printf("\nno server break\n");
			break;
		} else {

			memcpy(&dns_server_ip.addr, &pdns_server_ip->addr, 4);
			dns_srv[i].addr.sin_addr.s_addr = (u32_t)dns_server_ip.addr;
			log_msg("DNRD set DNS Server: %s", inet_ntoa(dns_srv[i].addr.sin_addr));
		}
		serv_cnt = i + 1;
	}
	rtos_mutex_give(dns_server_entry_lock);


	/*
	* Setup our DNS query forwarding socket.
	*/
	for (i = 0; i < serv_cnt; i++) {
		if ((dns_srv[i].sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			printf("\n\r %s %d Can not open socket", __FUNCTION__, __LINE__);
		}

		dns_srv[i].addr.sin_family = AF_INET;
		dns_srv[i].addr.sin_port   = htons(53);
	}


	/*
	* Run forever.
	*/
	printf("\n\r");
	printf("\n\r%s(%d), Available heap %d", __FUNCTION__, __LINE__, xPortGetFreeHeapSize());
	printf("\n\r");
	isDNRDRunning = 1;
	run();



}
struct task_struct ipc_msgQ_wlan_task;

void dns_relay_service_init(void)
{
    if (rtw_create_task(&ipc_msgQ_wlan_task, (const char *const)"dns_relay_service_running_start", 1024, (0 + 1), (void*)dns_relay_service_running_start, NULL) != 1) {
        DBG_8195A("Create inic_ipc_msg_q_task Err!!\n");
    }   
}





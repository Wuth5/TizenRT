/*
 * udp.c - handle upd connections
 */
#include <FreeRTOS.h>
#include <task.h>
#include "platform_opts.h"

#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "lwip/api.h"
#include "lwip/sys.h"



#include <errno.h>
//#include <sys/types.h>
//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>
#include <string.h>
#include "common.h"
#include "relay.h"
//#include "cache.h"
#include "query.h"
#include "master.h"

/*
 * dnssend()						22OCT99wzk
 *
 * Abstract: A small wrapper for send()/sendto().  If an error occurs a
 *           message is written to syslog.
 *
 * Returns:  The return code from sendto().
 */
static int dnssend(int k, void *msg, int len)
{
	int	rc;

	rc = sendto(dns_srv[k].sock, msg, len, 0,
				(const struct sockaddr *) &dns_srv[k].addr,
				sizeof(struct sockaddr_in));
	if (rc != len) {
		log_msg("sendto error: %s: %m",
				inet_ntoa(dns_srv[k].addr.sin_addr));
		return (rc);
	}

	return (rc);
}

/*
 * handle_udprequest()
 *
 * This function handles udp DNS requests by either replying to them (if we
 * know the correct reply via master, caching, etc.), or forwarding them to
 * an appropriate DNS server.
 */
void handle_udprequest(void)
{
	unsigned           addr_len;
	int                len;
	const int          maxsize = 512; /* According to RFC 1035 */
	unsigned char      msg[512 + 4]; /* Do we really want this on the stack?*/
	struct sockaddr_in from_addr;
	int                fwd, srvr;
	int	               i, thisdns, processed;

	/* Read in the message */
	addr_len = sizeof(struct sockaddr_in);
	len = recvfrom(isock, msg, maxsize, 0,
				   (struct sockaddr *)&from_addr, &addr_len);
	if (len < 0) {
		log_debug("recvfrom error");
		return;
	}
	if (len > maxsize) {
		log_msg("Received message is too big to process");
		return;
	}

	/* Determine how query should be handled */
	if ((fwd = handle_query(&from_addr, (char *)msg, &len, (unsigned int *)&srvr)) < 0) {
		return;    /* if its bogus, just ignore it */
	}

	/* If we already know the answer, send it and we're done */
	if (fwd == 0) {
		if (sendto(isock, msg, len, 0, (const struct sockaddr *)&from_addr,
				   addr_len) != len) {
			log_debug("sendto error ");
		}
		return;
	}

	/* If we have domains associated with our servers, send it to the
	   appropriate server as determined by srvr */
	if (dns_srv[0].domain != NULL) {
		dnssend(srvr, msg, len);
		return;
	}

	/* 1 or more redundant servers.  Cycle through them as needed */
	processed = 0;
	thisdns   = serv_act;
	for (i = 0; (unsigned int)i < serv_cnt; i++) {
		if (dnssend(serv_act, msg, len) == len) {
			if (i != 0) {
				log_debug("switched to DNS server %s",
						  inet_ntoa(dns_srv[thisdns].addr.sin_addr));
			}
			processed = 1;
			//printf("\n\r");
			//printf("\n\r %s %d processed = 1",__FUNCTION__, __LINE__);
			//printf("\n\r");
			break;
		}
		thisdns = (thisdns + 1) % serv_cnt;
	}

	if (processed == 0) {
		int	packetlen;
		unsigned char	packet[512 + 4];

		/*
		 * If we couldn't send the packet to our DNS servers,
		 * perhaps the `network is unreachable', we tell the
		 * client that we are unable to process his request
		 * now.  This will show a `No address (etc.) records
		 * available for host' in nslookup.  With this the
		 * client won't wait hang around till he gets his
		 * timeout.
		 * For this feature dnrd has to run on the gateway
		 * machine.
		 */
		packetlen = master_dontknow(msg, len, packet);
		if (packetlen > 0) {
			if (!dnsquery_find((char *)msg, &from_addr)) {
				log_debug("Query ERROR: couldn't find the original query");
				return;
			}
			if (sendto(isock, msg, len, 0, (const struct sockaddr *)&from_addr,
					   addr_len) != len) {
				log_debug("sendto error ");
				return;
			}
		}
	}
}

/*
 * dnsrecv()							22OCT99wzk
 *
 * Abstract: A small wrapper for recv()/recvfrom() with output of an
 *           error message if needed.
 *
 * Returns:  A positove number indicating of the bytes received, -1 on a
 *           recvfrom error and 0 if the received message is too large.
 */
static int dnsrecv(int k, void *msg, int len)
{
	int	rc;
	struct sockaddr_in from;
	u32_t fromlen;
	fromlen = sizeof(struct sockaddr_in);
	rc = recvfrom(dns_srv[k].sock, msg, len, 0,
				  (struct sockaddr *) &from, &fromlen);

	if (rc == -1) {
		log_msg("recvfrom error: %s: %m",
				inet_ntoa(dns_srv[k].addr.sin_addr));
		return (-1);
	} else if (rc > len) {
		log_msg("packet too large: %s",
				inet_ntoa(dns_srv[k].addr.sin_addr));
		return (0);
	} else if (memcmp(&from.sin_addr, &dns_srv[k].addr.sin_addr,
					  sizeof(from.sin_addr)) != 0) {
		log_msg("unexpected server: %s",
				inet_ntoa(from.sin_addr));
		return (0);
	}

	return (rc);
}

/*
 * handle_udpreply()
 *
 * This function handles udp DNS requests by either replying to them (if we
 * know the correct reply via master, caching, etc.), or forwarding them to
 * an appropriate DNS server.
 */
void handle_udpreply(int srvidx)
{
	const int          maxsize = 512; /* According to RFC 1035 */
	char      msg[512 + 4]; /* Do we really want this on the stack?*/
	int                len;
	struct sockaddr_in from_addr;
	unsigned           addr_len;

	len = dnsrecv(srvidx, msg, maxsize);
	if (1) {
		char buf[80];
		sprintf_cname(&msg[12], len - 12, buf, 80);
		log_debug("Received DNS reply for \"%s\"", buf);
	}
	if (len > 0) {
		dump_dnspacket("reply", msg, len);
		//cache_dnspacket(msg, len);
		//log_debug("Forwarding the reply to the host");
		addr_len = sizeof(struct sockaddr_in);
		if (!dnsquery_find(msg, &from_addr)) {
			log_debug("Reply : May Reply Last!!!");
		} else {

			log_debug("Forwarding the reply to the host %s", inet_ntoa(from_addr.sin_addr));
			//printf("\n\r");
			//printf("\n\r Forwarding the reply to the host %s", inet_ntoa(from_addr.sin_addr));
			//printf("\n\r");
			if (sendto(isock, msg, len, 0,
					   (const struct sockaddr *)&from_addr,
					   addr_len) != len) {
				log_debug("sendto error ");
			}
		}
	}
}

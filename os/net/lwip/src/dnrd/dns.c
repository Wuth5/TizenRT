
/*

    File: dns.c


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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//#include <ctype.h>

//#include <netdb.h>
//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>

#include "dns.h"
#include "lib.h"
#include "common.h"
#if 0
/*
static int get_objectname(unsigned char *msg, unsigned const char *limit,
			  unsigned char **here, char *string, int strlen,
			  int k);
*/

static int get_objname(unsigned char buf[], const int bufsize, int *here,
					   char name[], const int namelen)
{
	int count = 1000;
	if (*here > bufsize) {
		return 0;
	}

}
#endif
int free_packet(dnsheader_t *x)
{
	free(x->packet);
	free(x);
	return (0);
}

static dnsheader_t *alloc_packet(void *packet, int len)
{
	dnsheader_t *x;

	x = malloc(sizeof(dnsheader_t));
	memset(x, 0, sizeof(dnsheader_t));

	x->packet = malloc(len + 2);
	x->len    = len;
	memcpy(x->packet, packet, len);

	return (x);
}

static dnsheader_t *decode_header(void *packet, int len)
{
	unsigned short int *p;
	dnsheader_t *x;

	x = alloc_packet(packet, len);
	p = (unsigned short int *) x->packet;

	x->id      = ntohs(p[0]);
	x->u       = ntohs(p[1]);
	x->qdcount = ntohs(p[2]);
	x->ancount = ntohs(p[3]);
	x->nscount = ntohs(p[4]);
	x->arcount = ntohs(p[5]);

	x->here    = (char *) &x->packet[12];
	return (x);
}

static int raw_dump(dnsheader_t *x)
{
	unsigned int c;
	//int start;
	int	 i, j;

	//start = x->here - x->packet;
	for (i = 0; i < x->len; i += 16) {
		printf("%03X -", i);
		//printf("\n\r");
		for (j = i; j < x->len  &&  j < i + 16; j++) {
			printf(" %02X", ((unsigned int) x->packet[j]) & 0XFF);
		}
		for (; j < i + 16; j++) {
			printf("   ");
		}

		printf("  ");
		for (j = i; j < x->len  &&  j < i + 16; j++) {
			c = ((unsigned int) x->packet[j]) & 0XFF;
			printf("%c", (c >= 32  &&  c < 127) ? c : '.');
		}

		printf("\n\r");
	}

	printf("\n\r");
	return (0);
}

/*
static int get_objname(unsigned char buf[], const int bufsize, int *here,
		      char name[], const int namelen) {
  int i,p=*here, count=1000;
  unsigned int len, offs;
  if (p > bufsize) return 0;
  while (len = buf[p]) {

    while (len & 0x0c) {
      if (++p > bufsize) return 0;
      offs = lenbuf[p]

}
*/


static int get_objectname(unsigned char *msg, unsigned const char *limit,
						  unsigned char **here, char *string, unsigned int strlen,
						  int k)
{
	unsigned int len;
	unsigned int i;

	if ((*here >= limit) || (k >= (int)strlen)) {
		return (-1);
	}
	while ((len = **here) != 0) {

		*here += 1;
		if (*here >= limit) {
			return (-1);
		}
		/* If the name is compressed (see 4.1.4 in rfc1035)  */
		if (len & 0xC0) {
			unsigned offset;
			unsigned char *p;

			offset = ((len & ~0xc0) << 8) + **here;
			if ((p = &msg[offset]) >= limit) {
				return (-1);
			}
			if (p == *here - 1) {
				log_debug("looping ptr");
				return (-2);
			}

			if ((k = get_objectname(msg, limit, &p, string, RR_NAMESIZE, k)) < 0) {
				return (-1); /* if we cross the limit, bail out */
			}
			break;
		} else if (len < 64) {
			/* check so we dont pass the limits */
			if (((*here + len) > limit) || (len + k + 2 > strlen)) {
				return (-1);
			}
			for (i = 0; i < len; i++) {
				string[k++] = **here;
				*here += 1;
			}

			string[k++] = '.';
		}
	}

	*here += 1;
	string[k] = 0;

	return (k);
}


static unsigned char *read_record(dnsheader_t *x, rr_t *y,
								  unsigned char *here, int question,
								  unsigned const char *limit)
{
	int	k, len;
	unsigned short int conv;
	unsigned short check_len = 0;
	/*
	 * Read the name of the resource ...
	 */

	k = get_objectname((unsigned char *)x->packet, limit, &here, y->name, sizeof(y->name), 0);
	if (k < 0) {
		return (NULL);
	}
	y->name[k] = 0;

	/* safe to read TYPE and CLASS? */
	if ((here + 4) > limit) {
		return (NULL);
	}
	/*
	 * ... the type of data ...
	 */

	memcpy(&conv, here, sizeof(unsigned short int));
	y->type = ntohs(conv);
	here += 2;

	/*
	 * ... and the class type.
	 */

	memcpy(&conv, here, sizeof(unsigned short int));
	y->class = ntohs(conv);
	here += 2;

	/*
	 * Question blocks stop here ...
	 */

	if (question != 0) {
		return (here);
	}


	/*
	 * ... while answer blocks carry a TTL and the actual data.
	 */

	/* safe to read TTL and RDLENGTH? */
	if ((here + 6) > limit) {
		return (NULL);
	}
	memcpy(&y->ttl, here, sizeof(unsigned long int));
	y->ttl = ntohl(y->ttl);
	here += 4;

	/*
	 * Fetch the resource data.
	 */
#if 1
	//memcpy(&y->len, here, sizeof(unsigned short int));
	//printf("\n\r");
	// printf("\n\r %s %d %02X %02X %02X %02X %02X %02X limit=%02X", __FUNCTION__, __LINE__, *(here),*(here+1),*(here+2),*(here+3),*(here+4),*(here+5),*limit);
	// printf("\n\r");

	memcpy(&check_len, here, sizeof(unsigned short));
	//printf("the check len=%04X %d\n", check_len, ntohs(check_len));
	//printf("A the len =%X\n", y->len);
	y->len = ntohs(check_len);
#else
	memcpy(&y->len, here, sizeof(unsigned short int));
#endif
	len = y->len;
	here += 2;

	/* safe to read RDATA? */
	if ((here + y->len) > limit) {
		return (NULL);
	}

	if ((unsigned int)y->len > sizeof(y->data) - 4) {
		y->len = sizeof(y->data) - 4;
	}

	memcpy(y->data, here, y->len);
	here += len;
	y->data[y->len] = 0;

	return (here);
}


int dump_dnspacket(char *type, char *packet, int len)
{
	int	i, j;
	rr_t	y;
	dnsheader_t *x;
	unsigned char *limit;

	if (opt_debug < 2) {
		return 0;
	}

	if ((x = decode_header(packet, len)) == NULL) {
		return (-1);
	}
	limit = (unsigned char *)x->packet + len;

	if (x->u & MASK_Z) {
		log_debug("Z is set");
	}

	printf("\n\r");
	printf("\n\r- -- %s", type);
	printf("\n\r");
	raw_dump(x);

	printf("\n\r");
	printf("id= %d, q= %d, opc= %d, aa= %d, wr/ra= %d/%d, "
		   "trunc= %d, rcode= %d [%04X]\n",
		   x->id, GET_QR(x->u), GET_OPCODE(x->u), GET_AA(x->u),
		   GET_RD(x->u), GET_RA(x->u), GET_TC(x->u), GET_RCODE(x->u), x->u);

	printf("qd= %d\n", x->qdcount);

	if ((x->here = (char *)read_record(x, &y, (unsigned char *)x->here, 1, limit)) == NULL) {
		free_packet(x);
		return (-1);
	}

	printf("  name= %s, type= %d, class= %d\n\r", y.name, y.type, y.class);

	printf("ans= %d\n\r", x->ancount);
	for (i = 0; i < x->ancount; i++) {
		if ((x->here = (char *)read_record(x, &y, (unsigned char *)x->here, 0, limit)) == NULL) {
			printf("\n\r %s %d return -1", __FUNCTION__, __LINE__);
			free_packet(x);
			return (-1);
		}
		printf("  name= %s, type= %d, class= %d, ttl= %ld\n\r", y.name, y.type, y.class, y.ttl);
		printf("\n\r Len=%d Data:", y.len);
		for (j = 0; j < y.len; j++) {
			printf("%02X ", (unsigned char)y.data[j]);
		}
		printf("\n\r");
	}

	printf("ns= %d\n", x->nscount);
	for (i = 0; i < x->nscount; i++) {
		if ((x->here = (char *)read_record(x, &y, (unsigned char *)x->here, 0, limit)) == NULL) {
			free_packet(x);
			return (-1);
		}
		printf("  name= %s, type= %d, class= %d, ttl= %ld\n\r", y.name, y.type, y.class, y.ttl);
		printf("\n\r Len=%d Data:", y.len);
		for (j = 0; j < y.len; j++) {
			printf("%02X ", (unsigned char)y.data[j]);
		}
		printf("\n\r");
	}

	printf("ar= %d\n\r", x->arcount);
	for (i = 0; i < x->arcount; i++) {
		if ((x->here = (char *)read_record(x, &y, (unsigned char *)x->here, 0, limit)) == NULL) {
			free_packet(x);
			return (-1);
		}
		printf("  name= %s, type= %d, class= %d, ttl= %ld\n\r", y.name, y.type, y.class, y.ttl);
		printf("\n\r Len=%d Data:", y.len);
		for (j = 0; j < y.len; j++) {
			printf("%02X ", (unsigned char)y.data[j]);
		}
		printf("\n\r");
	}

	printf("\n\r");
	free_packet(x);

	return 0;
}



dnsheader_t *parse_packet(unsigned char *packet, int len)
{
	dnsheader_t *x;

	x = decode_header(packet, len);
	return (x);
}

/*
int get_dnsquery(dnsheader_t *x, rr_t *query)
{
    char	*here;

    if (x->qdcount == 0) {
	return (1);
    }

    here = &x->packet[12];
    read_record(x, query, here, 1);

    return (0);
}
*/

/*
 * parse_query()
 *
 * The function get_dnsquery() returns us the query part of an
 * DNS packet.  For this we must already have a dnsheader_t
 * packet which is additional work.  To speed things a little
 * bit up (we use it often) parse_query() gets the query direct.
 */
unsigned char *parse_query(rr_t *y, unsigned char *msg, int len)
{
	int	k;
	unsigned char *here;
	unsigned short int conv;
	unsigned const char *limit = msg + len;

	/* If QDCOUNT, the number of entries in the question section,
	 * is zero, we just give up */

	if (ntohs(((dnsheader_t *)msg)->qdcount) == 0) {
		log_debug("QDCOUNT was zero");
		return (0);
	}

	/*
	if (ntohs(((dnsheader_t *)msg)->u) & MASK_QR ) {
	  log_debug("QR bit set. This is a reponse?");
	  return(0);
	}
	*/

	y->flags = ntohs(((short int *) msg)[1]);

	here = &msg[PACKET_DATABEGIN];
	if ((k = get_objectname(msg, limit, &here, y->name, sizeof(y->name), 0)) < 0) {
		return (0);
	}
	y->name[k] = 0;

	/* check that there is type and class */
	if (here + 4 > limit) {
		return (0);
	}
	memcpy(&conv, here, sizeof(unsigned short int));
	y->type = ntohs(conv);
	here += 2;

	memcpy(&conv, here, sizeof(unsigned short int));
	y->class = ntohs(conv);
	here += 2;

	/* those strings should have been checked in get_objectname */
	k = strlen(y->name);
	if (k > 0  &&  y->name[k - 1] == '.') {
		y->name[k - 1] = '\0';
	}

	/* should we really convert the name to lowercase?
	 * rfc1035 2.3.3
	 */
	strlwr(y->name);

	return (here);
}



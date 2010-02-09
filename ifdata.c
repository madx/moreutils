#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/ioctl.h>

#if defined(__linux__)
	#include <linux/sockios.h>
	#include <linux/if.h>
#endif

#if defined(__FreeBSD_kernel__)
	#include <net/if.h>
#endif

#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

enum {
	DO_EXISTS = 1,
	DO_PEXISTS,
	DO_PADDRESS,
	DO_PMASK,
	DO_PMTU,
	DO_PCAST,
	DO_PALL,
	DO_PFLAGS,
	DO_SINPACKETS,
	DO_SINBYTES,
	DO_SINERRORS,
	DO_SINDROPS,
	DO_SINALL,
	DO_SINFIFO,
	DO_SINFRAME,
	DO_SINCOMPRESSES,
	DO_SINMULTICAST,
	DO_SOUTALL,
	DO_SOUTBYTES,
	DO_SOUTPACKETS,
	DO_SOUTERRORS,
	DO_SOUTDROPS,
	DO_SOUTFIFO,
	DO_SOUTCOLLS,
	DO_SOUTCARRIER,
	DO_SOUTMULTICAST,
	DO_PNETWORK,
	DO_PHWADDRESS,
	DO_BIPS,
	DO_BOPS
};

struct if_stat {
	unsigned long long in_packets, in_bytes, in_errors, in_drops;
	unsigned long long in_fifo, in_frame, in_compress, in_multicast;
	unsigned long long out_bytes, out_packets, out_errors, out_drops;
	unsigned long long out_fifo, out_colls, out_carrier, out_multicast;
};


void print_quad_ipv4(in_addr_t i) {
	i = ntohl(i);
	printf("%d.%d.%d.%d",
		(i & 0xff000000) >> 24,
		(i & 0x00ff0000) >> 16,
		(i & 0x0000ff00) >>  8,
		(i & 0x000000ff));
}

void print_quad_ipv6(uint16_t *a) {
	printf("%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x",
		a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
}

void print_quad(struct sockaddr *adr) {
	switch (adr->sa_family) {
		case AF_INET:
			print_quad_ipv4(((struct sockaddr_in*)adr)->sin_addr.s_addr);
		break;
		case AF_INET6:
			print_quad_ipv6(((struct sockaddr_in6*)adr)->sin6_addr.s6_addr16);
		break;
	default:
		printf("NON-IP");
		break;
	}
}

enum print_error_enum {
	PRINT_ERROR,
	PRINT_NO_ERROR,
};

/**
 * return 0 success
 *        1 error
 */
static int do_socket_ioctl(const char *ifname, const unsigned long int request,
                           struct ifreq *req, int *ioctl_errno,
                           const enum print_error_enum print_error) {
	int sock, res;

	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP)) == -1)
		return 1;
	strncpy(req->ifr_name, ifname, IFNAMSIZ);
	req->ifr_name[IFNAMSIZ - 1] = 0;

	if ((res = ioctl(sock, request, req)) == -1) {
		if (ioctl_errno)
			*ioctl_errno = errno;
		if (print_error == PRINT_ERROR)
			fprintf(stderr, "ioctl on %s: %s\n", ifname, strerror(errno));
		close(sock);
		return 1;
	}

	close(sock);

	return 0;
}

int if_exists(const char *iface) {
	struct ifreq r;
	return !do_socket_ioctl(iface, SIOCGIFFLAGS, &r, NULL, PRINT_NO_ERROR);
}

#if defined(__linux__)

void if_flags(const char *iface) {
	struct ifreq r;
	unsigned int i;
	const struct {
		unsigned int flag;
		char *name;
	} flags[] = {
		{ IFF_UP,          "Up" },
		{ IFF_BROADCAST,   "Broadcast" },
		{ IFF_DEBUG,       "Debugging" },
		{ IFF_LOOPBACK,    "Loopback" },
		{ IFF_POINTOPOINT, "Ppp" },
		{ IFF_NOTRAILERS,  "No-trailers" },
		{ IFF_RUNNING,     "Running" },
		{ IFF_NOARP,       "No-arp" },
		{ IFF_PROMISC,     "Promiscuous" },
		{ IFF_ALLMULTI,    "All-multicast" },
		{ IFF_MASTER,      "Load-master" },
		{ IFF_SLAVE,       "Load-slave" },
		{ IFF_MULTICAST,   "Multicast" },
		{ IFF_PORTSEL,     "Port-select" },
		{ IFF_AUTOMEDIA,   "Auto-detect" },
		{ IFF_DYNAMIC,     "Dynaddr" },
		{ 0xffff0000,      "Unknown-flags" },
	};

	if (do_socket_ioctl(iface, SIOCGIFFLAGS, &r, NULL, PRINT_ERROR))
		return;

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++)
		printf("%s%s%s", (r.ifr_flags & flags[i].flag) ? "On  " : "Off ",
		       flags[i].name,
		       sizeof(flags) / sizeof(flags[0]) - 1 == i ? "" : "\n");
}

void if_hwaddr(const char *iface) {
	struct ifreq r;
	unsigned char *hwaddr;

	if (do_socket_ioctl(iface, SIOCGIFHWADDR, &r, NULL, PRINT_ERROR))
		return;

	hwaddr = (unsigned char *)r.ifr_hwaddr.sa_data;
	printf("%02X:%02X:%02X:%02X:%02X:%02X",
	       hwaddr[0], hwaddr[1], hwaddr[2], hwaddr[3], hwaddr[4], hwaddr[5]);
}

#endif

static struct sockaddr *if_addr_value(const char *iface, struct ifreq *r, 
                                      unsigned long int request) {
	int e;

	if (do_socket_ioctl(iface, request, r, &e, PRINT_NO_ERROR)) {
		if (e == EADDRNOTAVAIL)
			return &r->ifr_addr;
		return NULL;
	}
	return &r->ifr_addr;
}

struct sockaddr *if_addr(const char *iface, struct ifreq *r) {
	return if_addr_value(iface, r, SIOCGIFADDR);
}

struct sockaddr *if_mask(const char *iface, struct ifreq *r) {
	return if_addr_value(iface, r, SIOCGIFNETMASK);
}

struct sockaddr *if_bcast(const char *iface, struct ifreq *r) {
	return if_addr_value(iface, r, SIOCGIFBRDADDR);
}

struct sockaddr *if_network(const char *iface) {
	struct sockaddr *saddr;
	static struct ifreq req;
	unsigned int mask;

	if (!(saddr = if_mask(iface, &req)))
		return NULL;

	mask  = ((struct sockaddr_in*)saddr)->sin_addr.s_addr;

	if (!(saddr = if_addr(iface, &req)))
		return NULL;

	((struct sockaddr_in*)saddr)->sin_addr.s_addr &= mask;
	return saddr;
}

int if_mtu(const char *iface) {
	static struct ifreq req;

	if (do_socket_ioctl(iface, SIOCGIFMTU, &req, NULL, PRINT_ERROR))
		return 0;

	return req.ifr_mtu;
}

#if defined(__linux__)

static void skipline(FILE *fd) {
	int ch;
	do {
		ch = getc(fd);
	} while (ch != '\n' && ch != EOF);
}

struct if_stat *get_stats(const char *iface) {
	FILE *fd;
	struct if_stat *ifstat;
	char name[10];

	if (!(ifstat = malloc(sizeof(struct if_stat)))) {
		perror("malloc");
		return NULL;
	}

	if ((fd = fopen("/proc/net/dev", "r")) == NULL) {
		perror("fopen(\"/proc/net/dev\")");
		free(ifstat);
		return NULL;
	}

	/* Skip header */
	skipline(fd);
	skipline(fd);

	do {
		int items = fscanf(fd,
			" %20[^:]:%llu %llu %llu %llu %llu %llu %llu %llu "
			"%llu %llu %llu %llu %llu %llu %llu %llu",
			name,
			&ifstat->in_bytes,    &ifstat->in_packets,
			&ifstat->in_errors,   &ifstat->in_drops,
			&ifstat->in_fifo,     &ifstat->in_frame,
			&ifstat->in_compress, &ifstat->in_multicast,
			&ifstat->out_bytes,   &ifstat->out_packets,
			&ifstat->out_errors,  &ifstat->out_drops,
			&ifstat->out_fifo,    &ifstat->out_colls,
			&ifstat->out_carrier, &ifstat->out_carrier
		);
		
		if (items == -1)
			break;
		if (items != 17) {
			fprintf(stderr, "Invalid data read, check!\n");
			break;
		}

		if (!strncmp(name, iface, sizeof(name))) {
			fclose(fd);
			return ifstat;
		}
	} while (!feof(fd));

	fclose(fd);
	free(ifstat);
	return NULL;
}

#endif

const struct {
	char *option;
	unsigned int flag;
	unsigned int is_stat;
	char *description;
} options[] = {
	{ "-e",   DO_EXISTS,        0, "Reports interface existence via return code" },
	{ "-p",   DO_PALL,          0, "Print out the whole config of iface" },
	{ "-pe",  DO_PEXISTS,       0, "Print out yes or no according to existence" },
	{ "-pa",  DO_PADDRESS,      0, "Print out the address" },
	{ "-pn",  DO_PMASK,         0, "Print netmask" },
	{ "-pN",  DO_PNETWORK,      0, "Print network address" },
	{ "-pb",  DO_PCAST,         0, "Print broadcast" },
	{ "-pm",  DO_PMTU,          0, "Print mtu" },
#if defined(__linux__)
	{ "-ph",  DO_PHWADDRESS,    0, "Print out the hardware address" },
	{ "-pf",  DO_PFLAGS,        0, "Print flags" },
	{ "-si",  DO_SINALL,        1, "Print all statistics on input" },
	{ "-sip", DO_SINPACKETS,    1, "Print # of in packets" },
	{ "-sib", DO_SINBYTES,      1, "Print # of in bytes" },
	{ "-sie", DO_SINERRORS,     1, "Print # of in errors" },
	{ "-sid", DO_SINDROPS,      1, "Print # of in drops" },
	{ "-sif", DO_SINFIFO,       1, "Print # of in fifo overruns" },
	{ "-sic", DO_SINCOMPRESSES, 1, "Print # of in compress" },
	{ "-sim", DO_SINMULTICAST,  1, "Print # of in multicast" },
	{ "-so",  DO_SOUTALL,       1, "Print all statistics on output" },
	{ "-sop", DO_SOUTPACKETS,   1, "Print # of out packets" },
	{ "-sob", DO_SOUTBYTES,     1, "Print # of out bytes" },
	{ "-soe", DO_SOUTERRORS,    1, "Print # of out errors" },
	{ "-sod", DO_SOUTDROPS,     1, "Print # of out drops" },
	{ "-sof", DO_SOUTFIFO,      1, "Print # of out fifo overruns" },
	{ "-sox", DO_SOUTCOLLS,     1, "Print # of out collisions" },
	{ "-soc", DO_SOUTCARRIER,   1, "Print # of out carrier loss" },
	{ "-som", DO_SOUTMULTICAST, 1, "Print # of out multicast" },
	{ "-bips",DO_BIPS,          1, "Print # of incoming bytes per second" },
	{ "-bops",DO_BOPS,          1, "Print # of outgoing bytes per second" },
#endif
};

void usage(const char *name) {
	unsigned int i;

	fprintf(stderr, "Usage: %s [options] iface\n", name);
	for (i = 0; i < sizeof(options) / sizeof(options[0]); i++) {
		fprintf(stderr, "  %5s   %s\n",
			options[i].option, options[i].description);
	}
}

void add_do(int *ndo, int **todo, int act) {
	*todo = realloc(*todo, (*ndo+1) * sizeof(int));
	(*todo)[*ndo] = act;
	*ndo += 1;
}

static void print_addr(struct sockaddr *sadr) {
	if (!sadr) {
		fprintf(stderr, "Error\n");
		exit(1);
	}
	print_quad(sadr);
}

struct if_stat *ifstats, *ifstats2 = NULL;

void please_do(int ndo, int *todo, const char *ifname) {
	int i;
	static struct ifreq req;
	if (!ndo) return;
	// printf("I have %d items in my queue.\n",ndo);
	for (i=0; i<ndo; i++) {
		switch (todo[i]) {
			case DO_EXISTS:
				exit(!if_exists(ifname));
			case DO_PEXISTS:
				printf("%s", if_exists(ifname) ? "yes" : "no");
				break;
			case DO_PADDRESS:
				print_addr(if_addr(ifname, &req));
				break;
#if defined(__linux__)
			case DO_PHWADDRESS:
				if_hwaddr(ifname);
				break;
			case DO_PFLAGS:
				if_flags(ifname);
				break;
#endif
			case DO_PMASK:
				print_addr(if_mask(ifname, &req));
				break;
			case DO_PCAST:
				print_addr(if_bcast(ifname, &req));
				break;
			case DO_PMTU:
				printf("%d", if_mtu(ifname));
				break;
			case DO_PNETWORK:
				print_addr(if_network(ifname));
				break;
			case DO_PALL:
				print_addr(if_addr(ifname, &req));
				printf(" ");
				print_addr(if_mask(ifname, &req));
				printf(" ");
				print_addr(if_bcast(ifname, &req));
				printf(" ");
				printf("%d", if_mtu(ifname));
				break;
#if defined(__linux__)
			case DO_SINPACKETS:
				printf("%llu",ifstats->in_packets);
				break;
			case DO_SINBYTES:
				printf("%llu",ifstats->in_bytes);
				break;
			case DO_SINERRORS:
				printf("%llu",ifstats->in_errors);
				break;
			case DO_SINDROPS:
				printf("%llu",ifstats->in_drops);
				break;
			case DO_SINFIFO:
				printf("%llu",ifstats->in_fifo);
				break;
			case DO_SINFRAME:
				printf("%llu",ifstats->in_frame);
				break;
			case DO_SINCOMPRESSES:
				printf("%llu",ifstats->in_compress);
				break;
			case DO_SINMULTICAST:
				printf("%llu",ifstats->in_multicast);
				break;
			case DO_SINALL:
				printf("%llu %llu %llu %llu %llu %llu %llu %llu",
					ifstats->in_bytes, ifstats->in_packets,
					ifstats->in_errors, ifstats->in_drops,
					ifstats->in_fifo, ifstats->in_frame,
					ifstats->in_compress, ifstats->in_multicast);
				break;
			case DO_SOUTBYTES:
				printf("%llu",ifstats->out_bytes);
				break;
			case DO_SOUTPACKETS:
				printf("%llu",ifstats->out_packets);
				break;
			case DO_SOUTERRORS:
				printf("%llu",ifstats->out_errors);
				break;
			case DO_SOUTDROPS:
				printf("%llu",ifstats->out_drops);
				break;
			case DO_SOUTFIFO:
				printf("%llu",ifstats->out_fifo);
				break;
			case DO_SOUTCOLLS:
				printf("%llu",ifstats->out_colls);
				break;
			case DO_SOUTCARRIER:
				printf("%llu",ifstats->out_carrier);
				break;
			case DO_SOUTMULTICAST:
				printf("%llu",ifstats->out_multicast);
				break;
			case DO_BIPS:
				if (ifstats2 == NULL) {
					sleep(1);
					ifstats2 = get_stats(ifname);
				}
				printf("%llu", ifstats2->in_bytes-ifstats->in_bytes);
				break;
			case DO_BOPS:
				if (ifstats2 == NULL) {
					sleep(1);
					ifstats2 = get_stats(ifname);
				}
				printf("%llu", ifstats2->out_bytes-ifstats->out_bytes);
				break;
			case DO_SOUTALL:
				printf("%llu %llu %llu %llu %llu %llu %llu %llu",
					ifstats->out_bytes, ifstats->out_packets,
					ifstats->out_errors, ifstats->out_drops,
					ifstats->out_fifo, ifstats->out_colls,
					ifstats->out_carrier, ifstats->out_multicast);
				break;
#endif
			default:
				printf("Unknown command: %d", todo[i]);
				break;
		}
		printf("\n");
	}
}

int main(int argc, char *argv[]) {
	int ndo=0;
	int *todo=NULL;
	char *ifname=NULL;
	int narg = 0;
	int do_stats = 0;
	unsigned int i, found;

	if (argc == 1) {
		usage(*argv);
		return 1;
	}

	while (narg < argc - 1) {
		narg++;

		found = 0;

		for (i = 0; i < sizeof(options) / sizeof(options[0]); i++) {
			if (!strcmp(argv[narg], options[i].option)) {
				add_do(&ndo, &todo, options[i].flag);
				do_stats |= options[i].is_stat;
				found = 1;
				break;
			}
		}

		if (found)
			continue;

		if (argv[narg][0] == '-') {
			usage(*argv);
			return 1;
		}
		else {
			ifname = argv[narg];
			break;
		}
	}

	if (narg + 1 < argc || !ifname) {
		usage(*argv);
		return 1;
	}

#if defined(__linux__)
	if (do_stats && (ifstats = get_stats(ifname)) == NULL) {
		fprintf(stderr, "Error getting statistics for %s\n", ifname);
		return 1;
	}
#endif

	please_do(ndo, todo, ifname);

	return 0;
}

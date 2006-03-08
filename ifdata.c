#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <linux/if.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
//#include <linux/socket.h>

#define DO_EXISTS 1
#define DO_PEXISTS 2
#define DO_PADDRESS 3
#define DO_PMASK 4
#define DO_PMTU 5
#define DO_PCAST 6
#define DO_PALL 7
#define DO_PFLAGS 8
#define DO_SINPACKETS 9
#define DO_SINBYTES 10
#define DO_SINERRORS 11
#define DO_SINDROPS 12
#define DO_SINALL 13
#define DO_SINFIFO 14
#define DO_SINFRAME 15
#define DO_SINCOMPRESSES 16
#define DO_SINMULTICAST 17
#define DO_SOUTALL 18
#define DO_SOUTBYTES 19
#define DO_SOUTPACKETS 20
#define DO_SOUTERRORS 21
#define DO_SOUTDROPS 22
#define DO_SOUTFIFO 23 
#define DO_SOUTCOLLS 24 
#define DO_SOUTCARRIER 25
#define DO_SOUTMULTICAST 26
#define DO_PNETWORK 27

struct if_stat {
	unsigned long long int in_packets;
	unsigned long long int in_bytes;
	unsigned long long int in_errors;
	unsigned long long int in_drops;
	unsigned long long int in_fifo;
	unsigned long long int in_frame;
	unsigned long long int in_compress;
	unsigned long long int in_multicast;
	unsigned long long int out_bytes;
	unsigned long long int out_packets;
	unsigned long long int out_errors;
	unsigned long long int out_drops;
	unsigned long long int out_fifo;
	unsigned long long int out_colls;
	unsigned long long int out_carrier;
	unsigned long long int out_multicast;
};


void print_quad_ipv4(unsigned int i) {
	printf("%d.%d.%d.%d",
			i&0xff,
			(i&0xff00)>>8,
			(i&0xff0000)>>16,
			(i&0xff000000)>>24);
}

void print_quad_ipv6(uint16_t *a) {
	int i;
	for (i=0; i<7; i++) {
		printf("%04x:",a[i]);
	}
	printf("%04x",a[i]);
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

#define PREPARE_SOCK(iface)	int sock; \
				static struct ifreq req; \
				int res; \
				sock=socket(PF_INET,SOCK_DGRAM,IPPROTO_IP); \
				strcpy(req.ifr_name,iface)
#define CALL_IOCTL(call)	res=ioctl(sock,call,&req)
#define END_SOCK		close(sock)
#define CALL_ERROR(todo)	if (res==-1) { perror("ioctl"); close(sock); todo; }

int if_exists(char *iface) {
	PREPARE_SOCK(iface);
	CALL_IOCTL(SIOCGIFFLAGS);
	if (res==-1 && errno==ENODEV) {
		END_SOCK;
		return 0;
	}
	CALL_ERROR(return 0);
	END_SOCK;
	return 1;
}

#define PRINT_IF(cond,tell) if (req.ifr_flags & cond) printf(tell); else printf("No "tell)

void if_flags(char *iface) {
	PREPARE_SOCK(iface);
	CALL_IOCTL(SIOCGIFFLAGS);
	CALL_ERROR(return);
	PRINT_IF(IFF_UP,"Up\n");
	PRINT_IF(IFF_BROADCAST,"Broadcast\n");
	PRINT_IF(IFF_DEBUG,"Debugging\n");
	PRINT_IF(IFF_LOOPBACK,"Loopback\n");
	PRINT_IF(IFF_POINTOPOINT,"Ppp\n");
	PRINT_IF(IFF_NOTRAILERS,"No-trailers\n");
	PRINT_IF(IFF_RUNNING,"Running\n");
	PRINT_IF(IFF_NOARP,"No-arp\n");
	PRINT_IF(IFF_PROMISC,"Promiscuous\n");
	PRINT_IF(IFF_ALLMULTI,"All-multicast\n");
	PRINT_IF(IFF_MASTER,"Load-master\n");
	PRINT_IF(IFF_SLAVE,"Load-slave\n");
	PRINT_IF(IFF_MULTICAST,"Multicast\n");
	PRINT_IF(IFF_PORTSEL,"Port-select\n");
	PRINT_IF(IFF_AUTOMEDIA,"Auto-detect\n");
	PRINT_IF(IFF_DYNAMIC,"Dynaddr\n");
	PRINT_IF(0xffff0000,"Unknown-flags");
}

struct sockaddr *if_addr(char *iface) {
	PREPARE_SOCK(iface);
	CALL_IOCTL(SIOCGIFADDR);
	if (res==-1 && errno==EADDRNOTAVAIL) {
		return &req.ifr_addr;
	}
	CALL_ERROR(return NULL);
	END_SOCK;
	return &req.ifr_addr;
}

struct sockaddr *if_mask(char *iface) {
	PREPARE_SOCK(iface);
	CALL_IOCTL(SIOCGIFNETMASK);
	if (res==-1 && errno==EADDRNOTAVAIL) {
		return &req.ifr_addr;
	}
	CALL_ERROR(return NULL);
	END_SOCK;
	return &req.ifr_addr;
}

struct sockaddr *if_network(char *iface) {
	struct sockaddr *res;
	res=if_mask(iface);
	long int mask=((struct sockaddr_in*)res)->sin_addr.s_addr;
	res=if_addr(iface);
	((struct sockaddr_in*)res)->sin_addr.s_addr &= mask;
	return res;
}

struct sockaddr *if_bcast(char *iface) {
	PREPARE_SOCK(iface);
	CALL_IOCTL(SIOCGIFBRDADDR);
	if (res==-1 && errno==EADDRNOTAVAIL) {
		return &req.ifr_addr;
	}
	CALL_ERROR(return NULL);
	END_SOCK;
	return &req.ifr_addr;
}

int if_mtu(char *iface) {
	PREPARE_SOCK(iface);
	CALL_IOCTL(SIOCGIFMTU);
	CALL_ERROR(return 0);
	END_SOCK;
	return req.ifr_mtu;
}

#define START 1
#define SKIP_LINE 2
#define START_LINE 3
#define START_IFNAME 4
#define IFACE_FOUND 5
#define RX_BYTES 6
#define WAIT_RX_PACKETS 7
#define RX_PACKETS 8
#define WAIT_RX_ERRORS 9
#define RX_ERRORS 10
#define WAIT_RX_DROPS 11
#define RX_DROPS 12
#define WAIT_RX_FIFO 13
#define RX_FIFO 14
#define WAIT_RX_FRAME 15
#define RX_FRAME 16
#define WAIT_RX_COMPRESS 17
#define RX_COMPRESS 18
#define WAIT_RX_MULTICAST 19
#define RX_MULTICAST 20
#define WAIT_TX_BYTES 21
#define TX_BYTES 22
#define WAIT_TX_PACKETS 23
#define TX_PACKETS 24
#define WAIT_TX_ERRORS 25
#define TX_ERRORS 26
#define WAIT_TX_DROPS 27
#define TX_DROPS 28
#define WAIT_TX_FIFO 29
#define TX_FIFO 30
#define WAIT_TX_COLLS 31
#define TX_COLLS 32
#define WAIT_TX_CARRIER 33
#define TX_CARRIER 34
#define WAIT_TX_MULTICAST 35
#define TX_MULTICAST 36

#define FIRST_DIGIT(val,digit) do {val=digit-'0'; } while(0)
#define NEXT_DIGIT(val,digit) do {val*=10; val+=digit-'0'; } while(0)

#define READ_INT(cas,val,next)	\
	case WAIT_##cas: \
		if (isdigit(buffer[i])) { \
			state=cas; \
			FIRST_DIGIT(val,buffer[i]); \
		} \
		break; \
	case cas: \
		if (isdigit(buffer[i])) \
			NEXT_DIGIT(val,buffer[i]); \
		else \
			state=next; \
		break;


//#define FIRST_DIGIT(val,digit) do {val=digit-'0'; printf(#val " = %d\n",val); } while(0)
//#define NEXT_DIGIT(val,digit) do {val*=10; val+=digit-'0'; printf(#val " -> %d\n",val);} while(0)
struct if_stat *get_stats(char *iface) {
	int fd;
	unsigned char buffer[4096];
	int i,j=0;
	int state=START;
	int len;
	struct if_stat *res=malloc(sizeof(struct if_stat));
	fd=open("/proc/net/dev",O_RDONLY);
	if (fd==-1) {
		perror("open");
		return NULL;
	}
	while ((len=read(fd,buffer,4096))) {
		for (i=0; i<len; i++) {
			switch (state) {
				case START:
					if (buffer[i]=='\n') state=SKIP_LINE;
					break;
				case SKIP_LINE:
					if (buffer[i]=='\n') state=START_LINE;
					break;
				case START_LINE:
					if (buffer[i]!=' ') {
						if (buffer[i]==iface[0]) {
							state=START_IFNAME;
							j=1;
						} else
							state=SKIP_LINE;
					}
					break;
				case START_IFNAME:
					if (buffer[i]==':' && iface[j]==0)
						state=IFACE_FOUND;
					else if (buffer[i]==iface[j])
						j++;
					else
						state=SKIP_LINE;
					break;
				case IFACE_FOUND:
					if (isdigit(buffer[i])) {
						state=RX_BYTES;
						FIRST_DIGIT(res->in_bytes,buffer[i]);
					}
					break;
				case RX_BYTES:
					if (isdigit(buffer[i]))
						NEXT_DIGIT(res->in_bytes,buffer[i]);
					else
						state=WAIT_RX_PACKETS;
					break;
				READ_INT(RX_PACKETS,res->in_packets,WAIT_RX_ERRORS);
				READ_INT(RX_ERRORS,res->in_errors,WAIT_RX_DROPS);
				READ_INT(RX_DROPS,res->in_drops,WAIT_RX_FIFO);
				READ_INT(RX_FIFO,res->in_fifo,WAIT_RX_FRAME);
				READ_INT(RX_FRAME,res->in_frame,WAIT_RX_COMPRESS);
				READ_INT(RX_COMPRESS,res->in_compress,WAIT_RX_MULTICAST);
				READ_INT(RX_MULTICAST,res->in_multicast,WAIT_TX_BYTES);
				READ_INT(TX_BYTES,res->out_bytes,WAIT_TX_PACKETS);
				READ_INT(TX_PACKETS,res->out_packets,WAIT_TX_ERRORS);
				READ_INT(TX_ERRORS,res->out_errors,WAIT_TX_DROPS);
				READ_INT(TX_DROPS,res->out_drops,WAIT_TX_FIFO);
				READ_INT(TX_FIFO,res->out_fifo,WAIT_TX_COLLS);
				READ_INT(TX_COLLS,res->out_colls,WAIT_TX_CARRIER);
				READ_INT(TX_CARRIER,res->out_carrier,WAIT_TX_MULTICAST);
				READ_INT(TX_MULTICAST,res->out_carrier,SKIP_LINE);
				default:
					fprintf(stderr,"Mon totomate est po bon!\n");
					break;
			}
		}
	}
	return res;
}

void usage(char *name) {
	fprintf(stderr,"Usage: %s [options] iface\n",name);
	fprintf(stderr,"    -e   Says if iface exists or not\n"
		       "    -p   Print out the whole config of iface\n"
		       "    -pe  Print out yes or no according to existence\n"
		       "    -pa  Print out the address\n"
		       "    -pn  Print netmask\n"
		       "    -pN  Print network address\n"
		       "    -pb  Print broadcast\n"
		       "    -pm  Print mtu\n"
		       "    -pf  Print flags\n"
		       "    -si  Print all statistics on input\n"
		       "    -sip Print # of in packets\n"
		       "    -sib Print # of in bytes\n"
		       "    -sie Print # of in errors\n"
		       "    -sid Print # of in drops\n"
		       "    -sif Print # of in fifo overruns\n"
		       "    -sic Print # of in compress\n"
		       "    -sim Print # of in multicast\n"
		       "    -so  Print all statistics on output\n"
		       "    -sop Print # of out packets\n"
		       "    -sob Print # of out bytes\n"
		       "    -soe Print # of out errors\n"
		       "    -sod Print # of out drops\n"
		       "    -sof Print # of out fifo overruns\n"
		       "    -sox Print # of out collisions\n"
		       "    -soc Print # of out carrier loss\n"
		       "    -som Print # of out multicast\n");
}

void add_do(int *ndo, int **todo, int act) {
	*todo=realloc(*todo,(*ndo+1)*sizeof(int));
	(*todo)[*ndo]=act;
	*ndo+=1;
}

#define PRINT_OR_ERR(adr) if (adr) print_quad(adr); else { fprintf(stderr, "Error\n"); exit(1); }

void please_do(int ndo, int *todo, char *ifname) {
	int i;
	struct sockaddr *sadr;
	struct if_stat *stats=NULL;
	if (!ndo) return;
//	printf("J'ai %d actions a réaliser\n",ndo);
	for (i=0; i<ndo; i++) {
		switch (todo[i]) {
			case DO_EXISTS:
				if (if_exists(ifname)) {
					exit(0);
				} else {
					exit(1);
				}
				break;
			case DO_PEXISTS:
				if (if_exists(ifname)) {
					printf("yes");
				} else {
					printf("no");
				}
				break;
			case DO_PADDRESS:
				sadr=if_addr(ifname);
				PRINT_OR_ERR(sadr);
				break;
			case DO_PFLAGS:
				if_flags(ifname);
				break;
			case DO_PMASK:
				sadr=if_mask(ifname);
				PRINT_OR_ERR(sadr);
				break;
			case DO_PCAST:
				sadr=if_bcast(ifname);
				PRINT_OR_ERR(sadr);
				break;
			case DO_PMTU:
				printf("%d",if_mtu(ifname));
				break;
			case DO_PNETWORK:
				sadr=if_network(ifname);
				PRINT_OR_ERR(sadr);
				break;
			case DO_PALL:
				sadr=if_addr(ifname);
				PRINT_OR_ERR(sadr);
				printf(" ");
				sadr=if_mask(ifname);
				PRINT_OR_ERR(sadr);
				printf(" ");
				sadr=if_bcast(ifname);
				PRINT_OR_ERR(sadr);
				printf(" ");
				printf("%d",if_mtu(ifname));
				break;
			case DO_SINPACKETS:
				if (!stats) stats=get_stats(ifname);
				printf("%llu",stats->in_packets);
				break;
			case DO_SINBYTES:
				if (!stats) stats=get_stats(ifname);
				printf("%llu",stats->in_bytes);
				break;
			case DO_SINERRORS:
				if (!stats) stats=get_stats(ifname);
				printf("%llu",stats->in_errors);
				break;
			case DO_SINDROPS:
				if (!stats) stats=get_stats(ifname);
				printf("%llu",stats->in_drops);
				break;
			case DO_SINFIFO:
				if (!stats) stats=get_stats(ifname);
				printf("%llu",stats->in_fifo);
				break;
			case DO_SINFRAME:
				if (!stats) stats=get_stats(ifname);
				printf("%llu",stats->in_frame);
				break;
			case DO_SINCOMPRESSES:
				if (!stats) stats=get_stats(ifname);
				printf("%llu",stats->in_compress);
				break;
			case DO_SINMULTICAST:
				if (!stats) stats=get_stats(ifname);
				printf("%llu",stats->in_multicast);
				break;
			case DO_SINALL:
				if (!stats) stats=get_stats(ifname);
				printf("%llu %llu %llu %llu %llu %llu %llu %llu",
						stats->in_packets,
						stats->in_bytes,
						stats->in_errors,
						stats->in_drops,
						stats->in_fifo,
						stats->in_frame,
						stats->in_compress,
						stats->in_multicast);
				break;
			case DO_SOUTBYTES:
				if (!stats) stats=get_stats(ifname);
				printf("%llu",stats->out_bytes);
				break;
			case DO_SOUTPACKETS:
				if (!stats) stats=get_stats(ifname);
				printf("%llu",stats->out_packets);
				break;
			case DO_SOUTERRORS:
				if (!stats) stats=get_stats(ifname);
				printf("%llu",stats->out_errors);
				break;
			case DO_SOUTDROPS:
				if (!stats) stats=get_stats(ifname);
				printf("%llu",stats->out_drops);
				break;
			case DO_SOUTFIFO:
				if (!stats) stats=get_stats(ifname);
				printf("%llu",stats->out_fifo);
				break;
			case DO_SOUTCOLLS:
				if (!stats) stats=get_stats(ifname);
				printf("%llu",stats->out_colls);
				break;
			case DO_SOUTCARRIER:
				if (!stats) stats=get_stats(ifname);
				printf("%llu",stats->out_carrier);
				break;
			case DO_SOUTMULTICAST:
				if (!stats) stats=get_stats(ifname);
				printf("%llu",stats->out_multicast);
				break;
			case DO_SOUTALL:
				if (!stats) stats=get_stats(ifname);
				printf("%llu %llu %llu %llu %llu %llu %llu %llu",
						stats->out_packets,
						stats->out_bytes,
						stats->out_errors,
						stats->out_drops,
						stats->out_fifo,
						stats->out_colls,
						stats->out_carrier,
						stats->out_multicast);
				break;
			default:
				printf("Je comprends pas ce que je dois faire: %d\n",todo[i]);
				break;
		}
		printf("\n");
	}
}

int main(int argc, char *argv[]) {
	int ndo=0;
	int *todo=NULL;
	char *me=*argv;
	char *ifname=NULL;
	int narg=0;
	/*
	print_quad(&res.ifr_addr);
	s=socket(PF_INET6,SOCK_DGRAM,IPPROTO_IP);
	ret=ioctl(s,SIOCGIFADDR,&res);
	print_quad(&res.ifr_addr);
	*/
	if (argc==1) {
		usage(me);
		return 1;
	}
	narg++;
	while (narg<argc) {
		if (!strcmp(argv[narg],"-e")) {
			add_do(&ndo,&todo,DO_EXISTS);
		} else if (!strcmp(argv[narg],"-p")) {
			add_do(&ndo,&todo,DO_PALL);
		} else if (!strcmp(argv[narg],"-pa")) {
			add_do(&ndo,&todo,DO_PADDRESS);
		} else if (!strcmp(argv[narg],"-pn")) {
			add_do(&ndo,&todo,DO_PMASK);
		} else if (!strcmp(argv[narg],"-pN")) {
			add_do(&ndo,&todo,DO_PNETWORK);
		} else if (!strcmp(argv[narg],"-pb")) {
			add_do(&ndo,&todo,DO_PCAST);
		} else if (!strcmp(argv[narg],"-pm")) {
			add_do(&ndo,&todo,DO_PMTU);
		} else if (!strcmp(argv[narg],"-pe")) {
			add_do(&ndo,&todo,DO_PEXISTS);
		} else if (!strcmp(argv[narg],"-pf")) {
			add_do(&ndo,&todo,DO_PFLAGS);
		} else if (!strcmp(argv[narg],"-si")) {
			add_do(&ndo,&todo,DO_SINALL);
		} else if (!strcmp(argv[narg],"-sip")) {
			add_do(&ndo,&todo,DO_SINPACKETS);
		} else if (!strcmp(argv[narg],"-sib")) {
			add_do(&ndo,&todo,DO_SINBYTES);
		} else if (!strcmp(argv[narg],"-sie")) {
			add_do(&ndo,&todo,DO_SINERRORS);
		} else if (!strcmp(argv[narg],"-sid")) {
			add_do(&ndo,&todo,DO_SINDROPS);
		} else if (!strcmp(argv[narg],"-sif")) {
			add_do(&ndo,&todo,DO_SINFIFO);
		} else if (!strcmp(argv[narg],"-sic")) {
			add_do(&ndo,&todo,DO_SINCOMPRESSES);
		} else if (!strcmp(argv[narg],"-sim")) {
			add_do(&ndo,&todo,DO_SINMULTICAST);
		} else if (!strcmp(argv[narg],"-so")) {
			add_do(&ndo,&todo,DO_SOUTALL);
		} else if (!strcmp(argv[narg],"-sop")) {
			add_do(&ndo,&todo,DO_SOUTPACKETS);
		} else if (!strcmp(argv[narg],"-sob")) {
			add_do(&ndo,&todo,DO_SOUTBYTES);
		} else if (!strcmp(argv[narg],"-soe")) {
			add_do(&ndo,&todo,DO_SOUTERRORS);
		} else if (!strcmp(argv[narg],"-sod")) {
			add_do(&ndo,&todo,DO_SOUTDROPS);
		} else if (!strcmp(argv[narg],"-sof")) {
			add_do(&ndo,&todo,DO_SOUTFIFO);
		} else if (!strcmp(argv[narg],"-sox")) {
			add_do(&ndo,&todo,DO_SOUTCOLLS);
		} else if (!strcmp(argv[narg],"-soc")) {
			add_do(&ndo,&todo,DO_SOUTCARRIER);
		} else if (!strcmp(argv[narg],"-som")) {
			add_do(&ndo,&todo,DO_SOUTMULTICAST);
		} else if (!strcmp(argv[narg],"-som")) {
			usage(me);
			return 1;
		} else if (argv[narg][0] == '-') {
			usage(me);
			return 1;
		} else {
			ifname=argv[narg];
			narg++;
			break;
		}
		narg++;
	}
	if (narg<argc || ifname==NULL) {
		usage(me);
		return 1;
	}
//	printf("Je travaille sur l'interface %s\n",ifname);
	please_do(ndo,todo,ifname);
	return 0;
}



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/bpf.h>
#include <net/ethernet.h>

#include <net80211/ieee80211.h>
#include <net80211/ieee80211_mesh.h>
#include <net80211/ieee80211_radiotap.h>

extern int errno;
static int fd = -1;
static int BUF_SIZE = 0;
static int packets_rcv = 0;
static int install_filter();

static int
hwmp_recv_action_meshpath(const struct ieee80211_frame *wh,
    const uint8_t *frm, const uint8_t *efrm)
{
	struct ieee80211_meshpreq_ie *preq;
	const uint8_t *iefrm = frm + 2; /* action + code */
	const uint8_t *iefrm_t = iefrm; /* temporary pointer */
	int found = 0;

	while (efrm - iefrm > 1) {
		if((efrm - iefrm) < (iefrm[1] + 2)) {
			return 0;
		}
		switch (*iefrm) {
		case IEEE80211_ELEMID_MESHPREQ:
		{
			printf("PREQ with ");
			preq = (struct ieee80211_meshpreq_ie *)iefrm;
			if (preq->preq_flags & IEEE80211_MESHPREQ_FLAGS_PR) {
				printf("GateAnnouncement!\n");
				return 0;
			} else {
				printf("NO GateAnnouncement!\n");
				return -1;
			}
			found++;
			break;
		}
		}
		iefrm += iefrm[1] + 2;
	}
	if (!found) {
		printf("MESH HWMP action without any IEs!\n");
	}
	return -1;
}

/* If assertion passes return 0 otherwise -1 */
static int
assertion(struct ieee80211_frame_min *whmin, uint8_t *ep)
{
#define	WH_TYPE(wh) (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK)
#define	WH_SUBTYPE(wh) (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK)
	const struct ieee80211_frame *wh;

	if (WH_TYPE(whmin) == IEEE80211_FC0_TYPE_MGT &&
	    WH_SUBTYPE(whmin) == IEEE80211_FC0_SUBTYPE_ACTION) {
		wh = (const struct ieee80211_frame *) whmin;
		printf("RA %s, ",
		    ether_ntoa((const struct ether_addr *)wh->i_addr1));
		printf("TA(SA) %s, ",
		    ether_ntoa((const struct ether_addr *)wh->i_addr2));
		return hwmp_recv_action_meshpath(wh,
		    (const uint8_t *)((uint8_t *)whmin +
		    sizeof(struct ieee80211_frame)), ep);

	}
	return -1;
#undef	WH_TYPE
#undef	WH_SUBTYPE
}

int main(){
	struct ifreq req;
	int error = 0;
	int assert_result = -1; /* 0 means success, -1 failure */
	fd = open("/dev/bpf0", O_RDWR);

	if(fd < 0){
		perror("can't opening /dev/bpf0");
		return -1;
	}
	strcpy(req.ifr_name, "wlan0");
	error =  ioctl(fd, BIOCSETIF, &req);
	if(error < 0){
		perror("error setting network interface");
		return -1;
	}

	if(ioctl(fd, BIOCGBLEN, &BUF_SIZE)){
		perror("error getting BIOCGBLEN\n");
	}
	uint32_t iftype = DLT_IEEE802_11_RADIO;
	if(ioctl(fd, BIOCSDLT, &iftype)){
		perror("error setting BIOCSDLT\n");
	}

	if(install_filter() != 0){
		printf("error cant install filter\n");
		return -1;
	}
	uint8_t *buf = malloc(sizeof(uint8_t)*BUF_SIZE);
	struct bpf_hdr *hdr;
	struct ether_header *eh;
	struct ieee80211_frame_min *whmin;
	struct ieee80211_radiotap_header *rdtap;
	int i, failed = 0;
	while(1){
		ssize_t size;
		if((size = read(fd, buf, BUF_SIZE, 0)) < 0){
			perror("error reading from /dev/bpf0");
		}
		uint8_t *p = buf;
		uint8_t *c = p;
		uint8_t radiotap_length = 0;
		while(p < buf + sizeof(uint8_t)*size){
			hdr = (struct bpf_hdr *)p;
			c = p + BPF_WORDALIGN(hdr->bh_hdrlen);
			rdtap = (struct ieee80211_radiotap_header *)
			    (p + BPF_WORDALIGN(hdr->bh_hdrlen));
			whmin = (struct ieee80211_frame_min *)
			    (c + rdtap->it_len);
			assert_result = assertion(whmin,
			    (uint8_t *)(p +
			    BPF_WORDALIGN(hdr->bh_hdrlen + hdr->bh_caplen)));
			if (assert_result == 0)
				return (0); /* sucess */
			failed++;
			if(failed == 10) /* XXX: magic number */
				return (-1);
			p=(uint8_t *)p + BPF_WORDALIGN(hdr->bh_hdrlen +
			    hdr->bh_caplen);
		}
	}

	if(close(fd) != 0){
		perror("cant close /dev/bpf0");
	}
	return -1;
}

int install_filter(){
	struct bpf_program prg;
	struct bpf_insn insns[] = {
             BPF_STMT(BPF_RET+BPF_K, sizeof(uint8_t)*128),
	};
	prg.bf_len = 1;

	prg.bf_insns = insns;

	if(ioctl(fd, BIOCSETF, &prg)){
		perror("error setting BIOCSETF");
	}
	return 0;
}

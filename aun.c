/* File Server Emulator   */
/* aun.c                  */
/* (c) 2021 Martin Mather */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <malloc.h>
#include <time.h>

#include "aun.h"
#include "ebuf.h"

static struct aun_t {
	uint32_t in_addr;
	struct sockaddr_in si;
	uint32_t rxhandle;
	clock_t rxtime;
	uint32_t txhandle;
	struct ebuf_t *txbuf;
	clock_t txtime;
	int txattempt;
} stations[AUN_MAX_STATIONS], *stnp;

static struct sockaddr_in si_me, si_other;
static int mysock, slen = sizeof(si_other), rxlen;
static uint8_t *rxbuf = NULL;
static uint16_t mystn, otherstn;

static void die(char *s) {
	perror(s);
	exit(1);
}

static void _opensock() {
	//create a UDP socket
	if ((mysock=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		die("socket");
	
	//zero out the structure
	memset((char *) &si_me, 0, sizeof(si_me));
	
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(AUN_PORT_BASE + mystn);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);
	
	//bind socket to port
	if (bind(mysock, (struct sockaddr*) &si_me, sizeof(si_me)) == -1)
		die("bind");

	//make socket non-blocking
	int flags = fcntl(mysock, F_GETFL, 0);
	if (flags == -1)
		die("get flags");

	if (fcntl(mysock, F_SETFL, flags | O_NONBLOCK) == -1)
		die("set flags");
}

static void _sendack(void) {
	rxbuf[0] = AUN_TYPE_ACK;//reuse rest of received header
	if (sendto(mysock, rxbuf, AUN_HDR_SIZE, 0, (struct sockaddr*) &si_other, slen) == -1)
		die("sendto()");
	//printf("ACK SENT\n");
}

static void _sendmachinetype(void) {
	//printf("send immediate reply ack\n");
	rxbuf[0] = AUN_TYPE_IMM_REPLY;//reuse rest of received header
	rxbuf[8] = 1;//bbc micro
	rxbuf[9] = 0;
	rxbuf[10] = 0x60;//nfs x.60
	rxbuf[11] = 3;//nfs 3.xx
	if (sendto(mysock, rxbuf, AUN_HDR_SIZE + 4, 0, (struct sockaddr*) &si_other, slen) == -1)
		die("sendto()");
}

static void _sendresult(struct ebuf_t *p, int result) {
	//printf("_sendresult %d, %d\n", p->index, result);
	if (p->state == EB_STATE_SENDING) {
		p->state = EB_STATE_SENT;
		p->result = result;
	} else
		ebuf_kill(p);
}

static void _sendresult2(int result) {
	if (stnp->txbuf) {
		_sendresult(stnp->txbuf, result);
		stnp->txbuf = NULL;
	}
}

static int _gotdata(void) {
	uint8_t port;
	uint32_t handle;

	//printf("ip=%08x\n", stnp->in_addr);
	//for (int i = 0; i < rxlen; i++)
	//	printf("%02x ", rxbuf[i]);
	//printf("\n");

	port = rxbuf[1];
	handle = rxbuf[7] << 24 | rxbuf[6] << 16 | rxbuf[5] << 8 | rxbuf[4];

	//printf("AUN:RX type=%02x port=%02x cb=%02x handle=%08x\n", rxbuf[0], port, rxbuf[2], handle);

	if (stnp->txbuf && rxbuf[0] != AUN_TYPE_ACK)
		_sendresult2(EB_RESULT_OTHER);//expecting an ACK but go something else!

	switch (rxbuf[0]) {// type
		case AUN_TYPE_UNICAST:
			//printf("UNICAST\n");
			if ((handle > stnp->rxhandle) || (clock() > stnp->rxtime)) {
				stnp->rxhandle = handle;
				stnp->rxtime = clock() + AUN_RXTIMEOUT * CLOCKS_PER_SEC;
				struct ebuf_t *p = ebuf_rxfind(otherstn, port);
				if (p) {
					//printf("ebuf %d found\n", p->index);
					if (rxlen <= (p->len + AUN_HDR_SIZE)) {
						_sendack();// do this first
						p->station = otherstn;//from station
						p->port = port;//to port
						p->control = rxbuf[2] | 0x80;//control byte
						ebuf_bind(p, rxbuf, rxlen);
						p->state = EB_STATE_RECEIVED;
						rxbuf = NULL;
					} else
						printf("AUN:buffer too small\n");
				} else
					printf("AUN:ebuf not found\n");
			} else if (handle == stnp->rxhandle) {
				_sendack();//duplicate of last packet, send ack
			}
			//else//duplicate of old packet, ignore
			//	printf("duplicate\n");
			break;
		case AUN_TYPE_ACK:
			//printf("ACK RECEIVED\n");
			if (handle == stnp->txhandle)
				_sendresult2(EB_RESULT_SUCCESSFUL);
			break;
		case AUN_TYPE_IMMEDIATE:
			//printf("IMM %02x : ", rxbuf[2]);//control byte
			//for (int i = 8; i < rxlen && i < 32; i++)
			//	printf("%02x ", rxbuf[i]);
			//printf("\n");
			switch (rxbuf[2]) {//control byte
				case ECONET_MACHINEPEEK:
					//printf("MACHINE PEEK\n");
					_sendmachinetype();
					break;
				default:
					//printf("Unhandled\n");
					break;
			}
			break;
		case AUN_TYPE_IMM_REPLY:
		default:
			//printf("Type?\n");
			break;
	}
}

static int _receiver(void) {
	if (!rxbuf)
		rxbuf = malloc(AUN_RXBUFLEN);

	if (rxbuf)
		rxlen = recvfrom(mysock, rxbuf, AUN_RXBUFLEN, 0, (struct sockaddr *) &si_other, &slen);
	else
		rxlen = 0;

	if (rxlen == -1) {
		if (errno != EWOULDBLOCK) 
			die("recvfrom()");
	} else if (rxlen >= AUN_HDR_SIZE) {
		//printf("Received packet from %s:%d length=%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port), rxlen);
		otherstn = ntohs(si_other.sin_port) - AUN_PORT_BASE;
		//printf("stn=%d\n", otherstn);
		if (otherstn < AUN_MAX_STATIONS) {
			if (otherstn == mystn) 
				printf("AUN:Duplicate station %d\n", otherstn);
			else {
				stnp = &stations[otherstn];
				uint32_t in_addr = ntohl(si_other.sin_addr.s_addr);
				if (stnp->in_addr == 0) {
					//printf("New station\n");
					stnp->in_addr = in_addr;
					stnp->si = si_other;
				}
				
				if (stnp->in_addr != in_addr)
					printf("AUN:Duplicate station %d\n", otherstn);
				else {
					//printf("Station OK\n");
					_gotdata();
				}
			}
		} else
			printf("AUN:Station number out of range\n");
	}
}


static int _transmitter() {
	struct ebuf_t *p;
	int restart = 1;

	while ((p = ebuf_txfind(restart))) {
		restart = 0;
		int send = 0;
		//printf("tx %d %d %04x %02x\n", p->index, p->state, p->station, p->port);
		if (p->station < AUN_MAX_STATIONS) {
			stnp = &stations[p->station];

			if (stnp->in_addr) {
				switch (p->state) {
					case EB_STATE_SEND:
					case EB_STATE_SEND2:
						//if already transmitting to station time that out!
						if (stnp->txbuf != p)
							_sendresult2(EB_RESULT_TIMEDOUT);
		
						if (p->state == EB_STATE_SEND)
							p->state = EB_STATE_SENDING;
						else
							p->state = EB_STATE_SENDING2;
	
						stnp->txbuf = p;
						stnp->txattempt = AUN_TXATTEMPTS;
	
						//pop AUN header
						uint8_t *hdr = p->buf2;
						uint32_t handle = (stnp->txhandle += 4);
						hdr[0] = AUN_TYPE_UNICAST;
						hdr[1] = p->port;
						hdr[2] = p->control & 0x7f;
						hdr[3] = 0;
						hdr[4] = handle & 0xff;
						hdr[5] = (handle >> 8) & 0xff;
						hdr[6] = (handle >> 16) & 0xff;
						hdr[7] = (handle >> 24) & 0xff;

						//printf("AUN:TX type=%02x port=%02x cb=%02x handle=%08x\n", hdr[0], p->port, hdr[2], handle);
						if (AUN_TXDELAY)//delay before first transmission?
							stnp->txtime = clock() + AUN_TXDELAY * CLOCKS_PER_SEC;
						else
							send = 1;
						break;
					case EB_STATE_SENDING:	// timed out?
					case EB_STATE_SENDING2:
						if (clock() > stnp->txtime) {
							//printf("TX: timed out\n");
							if(stnp->txattempt == 0)
								_sendresult2(EB_RESULT_TIMEDOUT);
							else {
								//printf("AUN:ReTX %d\n", stnp->txattempt);
								send = 1;//try again
							}
						}
						break;
				}
			} else	{
				//printf("TX: stn doesn't have an ip address!\n");
				_sendresult(p, EB_RESULT_TIMEDOUT);
			}
	
			if (send) {
				stnp->txtime = clock() + AUN_TXTIMEOUT * CLOCKS_PER_SEC;
				stnp->txattempt--;
	
				//printf("TX: Sending packet to %s:%d length=%d\n", inet_ntoa(stnp->si.sin_addr), ntohs(stnp->si.sin_port), p->len2);
				if (sendto(mysock, p->buf2, p->len2, 0, (struct sockaddr*) &stnp->si, slen) == -1)
					die("sendto()");
			}
		} else {//station number out of range
			p->state = EB_STATE_SENT;
			p->result = EB_RESULT_TIMEDOUT;
		}
	}
}


int aun_open(uint16_t stn) {
	//printf("aun_open stn=%d\n", stn);

	mystn = stn;	// remember my station number
	ebuf_open(AUN_MAX_BUFFERS);
	_opensock();
}

int aun_close(void) {
	//printf("aun_close\n");
	close(mysock);
	ebuf_close();
}

int aun_poll(void) {
	return _receiver() | _transmitter();
}


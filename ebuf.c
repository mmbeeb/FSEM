/* File Server Emulator   */
/* ebuf.c                 */
/* (c) 2021 Martin Mather */

#include <stdio.h>
#include <stdint.h>
#include <malloc.h>

#include "ebuf.h"
#include "aun.h"

static int ebuf_count;
static struct ebuf_t *ebufs = NULL;
 
void ebuf_open(int max_buffers) {
	//printf("ebuf_open %d\n", max_buffers);
	ebuf_count = max_buffers;
	ebufs = malloc(ebuf_count * sizeof(struct ebuf_t));

	struct ebuf_t *p = ebufs;
	for (int i = 0; i < ebuf_count; i++, p++) {
		p->index = i;
		p->state = EB_STATE_VOID;
		p->buf2 = NULL;
	}
}

void ebuf_close(void) {
	//printf("ebuf_close\n");

	struct ebuf_t *p = ebufs;
	for (int i = 0; i < ebuf_count; i++, p++) {
		if (p->buf2)
			free(p->buf2);
	}

	free(ebufs);
}

void ebuf_bind(struct ebuf_t *p, uint8_t *buf, int len) {
	if (buf) {
		p->buf2 = buf;
		p->len2 = len;
		p->buf = buf + AUN_HDR_SIZE;
		p->len = len - AUN_HDR_SIZE;
	} else {
		p->buf2 = NULL;
		p->buf = NULL;
	}	
}

uint8_t *ebuf_malloc(struct ebuf_t *p, int len) {
	int l = len + AUN_HDR_SIZE;
	uint8_t *m = malloc(l);
	ebuf_bind(p, m, l);
	return p->buf;
}

void ebuf_kill(struct ebuf_t *p) {
	if (p) {
		//printf("Kill %d\n", p->index);
		p->state = EB_STATE_VOID;
		free(p->buf2);
		p->buf2 = NULL;
	}
}

struct ebuf_t *ebuf_new() {
	struct ebuf_t *p = ebufs;
	for (int i = 0; i < ebuf_count; i++, p++) {
		if (p->state == EB_STATE_VOID)
			return p;
	}
	return NULL;
}

struct ebuf_t *ebuf_x(int x) {//assume i is valid
	return &ebufs[x];
}

struct ebuf_t *ebuf_rxfind(uint16_t stn, int port) {
	struct ebuf_t *p = ebufs;
	for (int i = 0; i < ebuf_count; i++, p++) {
		if (p->state == EB_STATE_LISTENING) {
			if ((p->port == 0 || p->port == port) && (p->station == 0 || p->station == stn))
				return p;
		}
	}
	return NULL;
}

struct ebuf_t *ebuf_txfind(int restart) {
	static int x;
	if (restart)
		x = 0;

	while (x < ebuf_count) {
		struct ebuf_t *p = &ebufs[x++];
		switch (p->state) {
			case EB_STATE_SEND:
			case EB_STATE_SENDING:
			case EB_STATE_SEND2:
			case EB_STATE_SENDING2:
				return p;
		}
	}

	return NULL;
}

void ebuf_print(struct ebuf_t *p) {
	//printf("ebuf index=%d state=%d stn=%x port=%x len=%x buf2=%x len2=%x\n", p->index, p->state, p->station, p->port, p->len, p->buf2, p->len2);
}

void ebuf_list() {
	struct ebuf_t *p = ebufs;
	for (int i = 0; i < ebuf_count; i++, p++)
		if (p->state != EB_STATE_VOID)
			ebuf_print(p);
}


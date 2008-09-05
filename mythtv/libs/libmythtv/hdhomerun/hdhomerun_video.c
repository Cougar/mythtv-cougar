/*
 * hdhomerun_video.c
 *
 * Copyright � 2006 Silicondust Engineering Ltd. <www.silicondust.com>.
 *
 * This library is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "hdhomerun.h"

struct hdhomerun_video_sock_t {
	uint8_t *buffer;
	size_t buffer_size;
	volatile size_t head;
	volatile size_t tail;
	size_t advance;
	volatile bool_t terminate;
	pthread_t thread;
	int sock;
	struct hdhomerun_debug_t *dbg;
	volatile uint32_t packet_count;
	volatile uint32_t transport_error_count;
	volatile uint32_t network_error_count;
	volatile uint32_t sequence_error_count;
	volatile uint32_t overflow_error_count;
	volatile uint8_t sequence[0x2000];
	uint32_t rtp_sequence;
};

static THREAD_FUNC_PREFIX hdhomerun_video_thread_execute(void *arg);

static bool_t hdhomerun_video_bind_sock_internal(struct hdhomerun_video_sock_t *vs, uint16_t listen_port)
{
	struct sockaddr_in sock_addr;
	memset(&sock_addr, 0, sizeof(sock_addr));
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	sock_addr.sin_port = htons(listen_port);
	if (bind(vs->sock, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) != 0) {
		return FALSE;
	}
	return TRUE;
}

static bool_t hdhomerun_video_bind_sock(struct hdhomerun_video_sock_t *vs, uint16_t listen_port)
{
	if (listen_port != 0) {
		return hdhomerun_video_bind_sock_internal(vs, listen_port);
	}

#if defined(__CYGWIN__) || defined(__WINDOWS__)
	/* Windows firewall silently blocks a listening port if the port number is not explicitly given. */
	/* Workaround - pick a random port number. The port may already be in use to try multiple port numbers. */
	srand((int)getcurrenttime());
	int retry;
	for (retry = 8; retry > 0; retry--) {
		uint16_t listen_port = (uint16_t)((rand() % 32768) + 32768);
		if (hdhomerun_video_bind_sock_internal(vs, listen_port)) {
			return TRUE;
		}
	}
	return FALSE;
#else
	return hdhomerun_video_bind_sock_internal(vs, listen_port);
#endif
}

struct hdhomerun_video_sock_t *hdhomerun_video_create(uint16_t listen_port, size_t buffer_size)
{
	/* Create object. */
	struct hdhomerun_video_sock_t *vs = (struct hdhomerun_video_sock_t *)calloc(1, sizeof(struct hdhomerun_video_sock_t));
	if (!vs) {
		return NULL;
	}

	/* Reset sequence tracking. */
	hdhomerun_video_flush(vs);

	/* Buffer size. */
	vs->buffer_size = (buffer_size / VIDEO_DATA_PACKET_SIZE) * VIDEO_DATA_PACKET_SIZE;
	if (vs->buffer_size == 0) {
		free(vs);
		return NULL;
	}
	vs->buffer_size += VIDEO_DATA_PACKET_SIZE;

	/* Create buffer. */
	vs->buffer = (uint8_t *)malloc(vs->buffer_size);
	if (!vs->buffer) {
		free(vs);
		return NULL;
	}
	
	/* Create socket. */
	vs->sock = (int)socket(AF_INET, SOCK_DGRAM, 0);
	if (vs->sock == -1) {
		free(vs->buffer);
		free(vs);
		return NULL;
	}

	/* Expand socket buffer size. */
	int rx_size = 1024 * 1024;
	setsockopt(vs->sock, SOL_SOCKET, SO_RCVBUF, (char *)&rx_size, sizeof(rx_size));

	/* Set timeouts. */
	setsocktimeout(vs->sock, SOL_SOCKET, SO_SNDTIMEO, 1000);
	setsocktimeout(vs->sock, SOL_SOCKET, SO_RCVTIMEO, 1000);

	/* Bind socket. */
	if (!hdhomerun_video_bind_sock(vs, listen_port)) {
		close(vs->sock);
		free(vs->buffer);
		free(vs);
		return NULL;
	}

	/* Start thread. */
	if (pthread_create(&vs->thread, NULL, &hdhomerun_video_thread_execute, vs) != 0) {
		close(vs->sock);
		free(vs->buffer);
		free(vs);
		return NULL;
	}

	/* Success. */
	return vs;
}

void hdhomerun_video_destroy(struct hdhomerun_video_sock_t *vs)
{
	vs->terminate = TRUE;
	pthread_join(vs->thread, NULL);

	close(vs->sock);
	free(vs->buffer);

	free(vs);
}

void hdhomerun_video_set_debug(struct hdhomerun_video_sock_t *vs, struct hdhomerun_debug_t *dbg)
{
	vs->dbg = dbg;
}

uint16_t hdhomerun_video_get_local_port(struct hdhomerun_video_sock_t *vs)
{
	struct sockaddr_in sock_addr;
	socklen_t sockaddr_size = sizeof(sock_addr);
	if (getsockname(vs->sock, (struct sockaddr*)&sock_addr, &sockaddr_size) != 0) {
		hdhomerun_debug_printf(vs->dbg, "hdhomerun_video_get_local_port: getsockname failed (%d)\n", sock_getlasterror);
		return 0;
	}
	return ntohs(sock_addr.sin_port);
}

static void hdhomerun_video_stats_ts_pkt(struct hdhomerun_video_sock_t *vs, uint8_t *ptr)
{
	uint16_t packet_identifier = ((uint16_t)(ptr[1] & 0x1F) << 8) | (uint16_t)ptr[2];
	if (packet_identifier == 0x1FFF) {
		return;
	}

	bool_t transport_error = ptr[1] >> 7;
	if (transport_error) {
		vs->transport_error_count++;
		vs->sequence[packet_identifier] = 0xFF;
		return;
	}

	uint8_t continuity_counter = ptr[3] & 0x0F;
	uint8_t previous_sequence = vs->sequence[packet_identifier];

	if (continuity_counter == ((previous_sequence + 1) & 0x0F)) {
		vs->sequence[packet_identifier] = continuity_counter;
		return;
	}
	if (previous_sequence == 0xFF) {
		vs->sequence[packet_identifier] = continuity_counter;
		return;
	}
	if (continuity_counter == previous_sequence) {
		return;
	}

	vs->sequence_error_count++;
	vs->sequence[packet_identifier] = continuity_counter;
}

static void hdhomerun_video_parse_rtp(struct hdhomerun_video_sock_t *vs, struct hdhomerun_pkt_t *pkt)
{
	pkt->pos += 2;
	uint32_t rtp_sequence = hdhomerun_pkt_read_u16(pkt);
	pkt->pos += 8;

	if (rtp_sequence != ((vs->rtp_sequence + 1) & 0xFFFF)) {
		if (vs->rtp_sequence != 0xFFFFFFFF) {
			vs->network_error_count++;

			/* restart pid sequence check */
			memset((void *)vs->sequence, 0xFF, sizeof(vs->sequence));
		}
	}

	vs->rtp_sequence = rtp_sequence;
}

static THREAD_FUNC_PREFIX hdhomerun_video_thread_execute(void *arg)
{
	struct hdhomerun_video_sock_t *vs = (struct hdhomerun_video_sock_t *)arg;
	struct hdhomerun_pkt_t pkt_inst;

	while (!vs->terminate) {
		struct hdhomerun_pkt_t *pkt = &pkt_inst;
		hdhomerun_pkt_reset(pkt);

		/* Receive. */
		int length = recv(vs->sock, (char *)pkt->end, VIDEO_RTP_DATA_PACKET_SIZE, 0);
		pkt->end += length;

		if (length == VIDEO_RTP_DATA_PACKET_SIZE) {
			hdhomerun_video_parse_rtp(vs, pkt);
			length = (int)(pkt->end - pkt->pos);
		}

		if (length != VIDEO_DATA_PACKET_SIZE) {
			if (length > 0) {
				/* Data received but not valid - ignore. */
				continue;
			}
			if (sock_getlasterror_socktimeout) {
				/* Wait for more data. */
				continue;
			}
			vs->terminate = TRUE;
			return NULL;
		}

		/* Store in ring buffer. */
		size_t head = vs->head;
		uint8_t *ptr = vs->buffer + head;
		memcpy(ptr, pkt->pos, length);

		/* Stats. */
		vs->packet_count++;
		hdhomerun_video_stats_ts_pkt(vs, ptr + TS_PACKET_SIZE * 0);
		hdhomerun_video_stats_ts_pkt(vs, ptr + TS_PACKET_SIZE * 1);
		hdhomerun_video_stats_ts_pkt(vs, ptr + TS_PACKET_SIZE * 2);
		hdhomerun_video_stats_ts_pkt(vs, ptr + TS_PACKET_SIZE * 3);
		hdhomerun_video_stats_ts_pkt(vs, ptr + TS_PACKET_SIZE * 4);
		hdhomerun_video_stats_ts_pkt(vs, ptr + TS_PACKET_SIZE * 5);
		hdhomerun_video_stats_ts_pkt(vs, ptr + TS_PACKET_SIZE * 6);

		/* Calculate new head. */
		head += length;
		if (head >= vs->buffer_size) {
			head -= vs->buffer_size;
		}

		/* Check for buffer overflow. */
		if (head == vs->tail) {
			vs->overflow_error_count++;
			continue;
		}

		/* Atomic update. */
		vs->head = head;
	}

	return NULL;
}

uint8_t *hdhomerun_video_recv(struct hdhomerun_video_sock_t *vs, size_t max_size, size_t *pactual_size)
{
	size_t head = vs->head;
	size_t tail = vs->tail;

	if (vs->advance > 0) {
		tail += vs->advance;
		if (tail >= vs->buffer_size) {
			tail -= vs->buffer_size;
		}
	
		/* Atomic update. */
		vs->tail = tail;
	}

	if (head == tail) {
		vs->advance = 0;
		*pactual_size = 0;
		return NULL;
	}

	size_t size = (max_size / VIDEO_DATA_PACKET_SIZE) * VIDEO_DATA_PACKET_SIZE;
	if (size == 0) {
		vs->advance = 0;
		*pactual_size = 0;
		return NULL;
	}

	size_t avail;
	if (head > tail) {
		avail = head - tail;
	} else {
		avail = vs->buffer_size - tail;
	}
	if (size > avail) {
		size = avail;
	}
	vs->advance = size;
	*pactual_size = size;
	return vs->buffer + tail;
}

void hdhomerun_video_flush(struct hdhomerun_video_sock_t *vs)
{
	/* Atomic update of tail. */
	vs->tail = vs->head;
	vs->advance = 0;

	memset((void *)vs->sequence, 0xFF, sizeof(vs->sequence));

	vs->rtp_sequence = 0xFFFFFFFF;

	vs->packet_count = 0;
	vs->transport_error_count = 0;
	vs->network_error_count = 0;
	vs->sequence_error_count = 0;
	vs->overflow_error_count = 0;
}

void hdhomerun_video_debug_print_stats(struct hdhomerun_video_sock_t *vs)
{
	hdhomerun_debug_printf(vs->dbg, "video sock: pkt=%ld net=%ld te=%ld miss=%ld drop=%ld\n", vs->packet_count, vs->network_error_count, vs->transport_error_count, vs->sequence_error_count, vs->overflow_error_count);
}

void hdhomerun_video_get_stats(struct hdhomerun_video_sock_t *vs, struct hdhomerun_video_stats_t *stats)
{
	memset(stats, 0, sizeof(struct hdhomerun_video_stats_t));

	stats->packet_count = vs->packet_count;
	stats->network_error_count = vs->network_error_count;
	stats->transport_error_count = vs->transport_error_count;
	stats->sequence_error_count = vs->sequence_error_count;
}

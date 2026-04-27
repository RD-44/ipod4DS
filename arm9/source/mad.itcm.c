#include <mad.h>

#include <nds.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ipc2.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define SOUND_NEED_BUFFER

#include "sound.h"
#include "file.h"

#define INPUT_BUFFER_SIZE 8192
#define FILE_BUFFER_SIZE 8192

#define MAD_DECODED_BUFFER_SIZE 4096

#define MIN(A,B) ((A > B) ? (B) : (A))

static s16 *readbufL, *readbufR;
static int lastbuf, lastpos;

static struct mad_stream Stream;
static struct mad_frame Frame;
static struct mad_synth Synth;
//static mad_timer_t Timer;
static unsigned char *InputBuffer, *GuardPtr;
static int mad_initialized;
// static unsigned FrameCount;
static u32 mad_size;
static u32 mad_samples_written;
static u32 mad_time_estimate;

static char *buffer;
static u32 buflen = 0;

u32 madplay_position(void);

static void madplay_update_time(void) {
	u32 played_samples = 0;

	if(sound_samplerate == 0) {
		sound_elapsed = 0;
		return;
	}

	if(mad_samples_written > (u32)buffer_samples)
		played_samples = mad_samples_written - buffer_samples;

	sound_elapsed = played_samples / sound_samplerate;
	sound_time = mad_time_estimate;
}

void madplay_stop(void) {
	if(mad_initialized) {
		mad_synth_finish(&Synth);
		mad_frame_finish(&Frame);
		mad_stream_finish(&Stream);
		mad_initialized = 0;
	}

	if(sound_playfile != NULL) {
		fclose(sound_playfile);
		sound_playfile = NULL;
	}

	free(readbufL);
	readbufL = NULL;
	free(readbufR);
	readbufR = NULL;
	free(buffer);
	buffer = NULL;
	free(InputBuffer);
	InputBuffer = NULL;
	GuardPtr = NULL;
}

static s16 madplay_fixedtos16(mad_fixed_t sample) {
	sample += (1L << (MAD_F_FRACBITS - 16));

	/* Clipping */
	if(sample > MAD_F_ONE - 1)
		sample = MAD_F_ONE - 1;
	if(sample < -MAD_F_ONE)
		sample = MAD_F_ONE;

	/* Conversion. */
	return sample >> (MAD_F_FRACBITS + 1 - 16);
}

static int madplay_read(void *ptr, u32 size) {
	u32 read;

	if(buflen < FILE_BUFFER_SIZE) {
		buflen += fread(buffer + buflen, 1, FILE_BUFFER_SIZE - buflen, sound_playfile);
	}

	if(buflen < size) {
		memcpy(ptr, buffer, buflen);
		read = buflen;
		buflen = 0;
	} else {
		memcpy(ptr, buffer, size);
		memmove(buffer, buffer + size, buflen - size);
		read = size;
		buflen -= size;
	}

	if(buflen < FILE_BUFFER_SIZE) {
		buflen += fread(buffer + buflen, 1, FILE_BUFFER_SIZE - buflen, sound_playfile);
	}

	return read;
}

static int madplay_eof(void) {
	if(buflen == 0)
		return feof(sound_playfile);
	else
		return 0;
}

static int madplay_decode(void) {
	int i;

	if(state == FINISHING)
		return 0;

	if(Stream.buffer == NULL || Stream.error == MAD_ERROR_BUFLEN) {
		size_t ReadSize, Remaining, bytes_read;
		unsigned char *ReadStart;

		if(Stream.next_frame != NULL) {
			Remaining = Stream.bufend - Stream.next_frame;
			memmove(InputBuffer, Stream.next_frame, Remaining);
			ReadStart = InputBuffer + Remaining;
			ReadSize = INPUT_BUFFER_SIZE - Remaining;
		} else {
			ReadSize = INPUT_BUFFER_SIZE;
			ReadStart = InputBuffer;
			Remaining = 0;
		}

		bytes_read = madplay_read(ReadStart, ReadSize);
		ReadSize = bytes_read;
		GuardPtr = NULL;

		if(ReadSize == 0 && Remaining == 0) {
			state = FINISHING;
			return 0;
		}

		if(madplay_eof()) {
			GuardPtr = ReadStart + ReadSize;
			memset(GuardPtr, 0, MAD_BUFFER_GUARD);
			ReadSize += MAD_BUFFER_GUARD;

			state = FINISHING;
		}

		mad_stream_buffer(&Stream, InputBuffer, ReadSize + Remaining);
		Stream.error = 0;
	}

	if(mad_frame_decode(&Frame, &Stream)) {
		if(MAD_RECOVERABLE(Stream.error)) {
			if(Stream.error != MAD_ERROR_LOSTSYNC || Stream.this_frame != GuardPtr) {
				if(IPC2->messageflag < IPC2_MAX_MESSAGES)
					sprintf((char *)IPC2->message[IPC2->messageflag++], "recoverable frame level error (%s)\n", mad_stream_errorstr(&Stream));
			}
			return 1;
		} else {
			if(Stream.error == MAD_ERROR_BUFLEN)
				return 1;
			else {
				if(IPC2->messageflag < IPC2_MAX_MESSAGES)
					sprintf((char *)IPC2->message[IPC2->messageflag++], "unrecoverable frame level error (%s).\n", mad_stream_errorstr(&Stream));

				return -1;
			}
		}
	}

//	FrameCount++;
	
//	mad_timer_add(&Timer, Frame.header.duration);

	mad_synth_frame(&Synth, &Frame);

	for(i=0; i < Synth.pcm.length; i++) {

		readbufL[i] = madplay_fixedtos16(Synth.pcm.samples[0][i]);

		if(MAD_NCHANNELS(&Frame.header)==2)
			readbufR[i] = madplay_fixedtos16(Synth.pcm.samples[1][i]);
	}

	lastbuf += Synth.pcm.length;

	return 0;
}

void madplay_update(void) {
	int oldval;
	int status;

	s16 *src16L, *dst16L, *src16R, *dst16R;
	int size;

	while(buffer_samples < buffer_size) {
		if(lastbuf == 0) {
			while((status = madplay_decode()) > 0);
			if(status < 0) {
				madplay_stop();
				state = ERROR;
				return;
			}

			if(state == FINISHING)
				return;

			lastpos = 0;
			sound_channels = MAD_NCHANNELS(&Frame.header);
			sound_samplerate =  Frame.header.samplerate;
			if(mad_time_estimate == 0 && Frame.header.bitrate > 0)
				mad_time_estimate = mad_size / (Frame.header.bitrate / 8);
		}

		src16L = ((s16 *) readbufL) + lastpos;
		dst16L = ((s16 *) pcmbufL) + buffer_end;
		src16R = ((s16 *) readbufR) + lastpos;
		dst16R = ((s16 *) pcmbufR) + buffer_end;

		size = MIN(buffer_size - buffer_end, buffer_size - buffer_samples);
		size = MIN(size, lastbuf);

		/* we can't be interrupted here or the sound will be corrupted
		 * this shold be very fast, we're only copying the buffers */
		oldval = REG_IME;
		REG_IME = 0;

		memcpy(dst16L, src16L, size * 2);
		if(sound_channels == 2)
			memcpy(dst16R, src16R, size * 2);

		lastbuf -= size;
		lastpos += size;
		buffer_end += size;
		buffer_end %= buffer_size;
		buffer_samples += size;
		mad_samples_written += size;
		madplay_update_time();

		REG_IME = oldval;
	}

	madplay_update_time();
}

int madplay(struct media *m) {
	struct stat s;

	readbufL = NULL;
	readbufR = NULL;
	buffer = NULL;
	InputBuffer = NULL;
	GuardPtr = NULL;
	sound_playfile = NULL;
	mad_initialized = 0;

	readbufL = (s16 *) malloc(MAD_DECODED_BUFFER_SIZE * 2);
	readbufR = (s16 *) malloc(MAD_DECODED_BUFFER_SIZE * 2);
	buffer = (char *) malloc(FILE_BUFFER_SIZE);

	InputBuffer = (char *) malloc(INPUT_BUFFER_SIZE + MAD_BUFFER_GUARD);
	if(readbufL == NULL || readbufR == NULL || buffer == NULL || InputBuffer == NULL) {
		state = ERROR;
		madplay_stop();
		return 1;
	}

	stat(m->path, &s);
	mad_size = s.st_size;

	sound_playfile = fopen(m->path, "rb");
	if(sound_playfile == NULL) {
		state = ERROR;
		madplay_stop();
		return 1;
	}

	mad_stream_init(&Stream);
	mad_frame_init(&Frame);
	mad_synth_init(&Synth);
	mad_initialized = 1;
//	mad_timer_reset(&Timer);

	buflen = 0;
	lastbuf = lastpos = 0;
	mad_samples_written = 0;
	mad_time_estimate = 0;
//	FrameCount = 0;

	madplay_update();
	sound_bps = 2;

	format = MAD;

	return 0;
}

void madplay_flush(void) {
	mad_synth_finish(&Synth);
	mad_frame_finish(&Frame);
	mad_stream_finish(&Stream);

	mad_stream_init(&Stream);
	mad_frame_init(&Frame);
	mad_synth_init(&Synth);

	buflen = 0;
	lastbuf = lastpos = 0;
	mad_samples_written = 0;
	mad_time_estimate = sound_time;
}

u32 madplay_position(void) {
	return ftell(sound_playfile) - buflen;
}

u32 madplay_size(void) {
	return mad_size;
}

void madplay_seek(u32 pos) {
	fseek(sound_playfile, pos, SEEK_SET);
	mad_samples_written = 0;
}

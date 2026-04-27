#include <nds.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>

#include <unistd.h>

#include <ipc2.h>

#include "file.h"
#include "input.h"
#define SOUNDC
#include "sound.h"

#include "madplay.h"
#include "flac.h"
#include "tremor.h"
#include "sound_tables.h"

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))

static enum state seeking_state;
static int seek_delay;
static u32 seek_pos;

static int sound_wait_for_arm7_state(u8 expected_state, const char *message) {
	int frames;

	for(frames = 0; frames < 180; frames++) {
		if(IPC2->sound_state == expected_state)
			return 1;
		swiWaitForVBlank();
	}

	if(IPC2->messageflag < IPC2_MAX_MESSAGES) {
		strncpy((char *)IPC2->message[IPC2->messageflag], message, sizeof(IPC2->message[0]) - 1);
		((char *)IPC2->message[IPC2->messageflag])[sizeof(IPC2->message[0]) - 1] = '\0';
		IPC2->messageflag++;
	}

	return 0;
}

void InterruptHandler_IPC_SYNC(void) {
	u8 sync;
	int copy_samples;
	int first_part;

	sync = IPC_GetSync();

	if(sync == IPC2_REQUEST_WRITE_SOUND) {
		copy_samples = MIN(buffer_samples, SOUND_MAX_SAMPLES_TRANSFER);
		first_part = MIN(buffer_size - buffer_pos, copy_samples);

		memset(pcmL, 0, SOUND_MAX_SAMPLES_TRANSFER * sound_bps);
		if(sound_channels == 2)
			memset(pcmR, 0, SOUND_MAX_SAMPLES_TRANSFER * sound_bps);

		if(copy_samples > 0) {
			memcpy(pcmL, pcmbufL + buffer_pos * sound_bps, first_part * sound_bps);
			if(sound_channels == 2)
				memcpy(pcmR, pcmbufR + buffer_pos * sound_bps, first_part * sound_bps);

			if(copy_samples > first_part) {
				memcpy(pcmL + first_part * sound_bps, pcmbufL, (copy_samples - first_part) * sound_bps);
				if(sound_channels == 2)
					memcpy(pcmR + first_part * sound_bps, pcmbufR, (copy_samples - first_part) * sound_bps);
			}
		}

		buffer_pos += copy_samples;
		if(buffer_size > 0)
			buffer_pos %= buffer_size;

		buffer_samples -= copy_samples;
		if(buffer_samples < buffer_lowest)
			buffer_lowest = buffer_samples;

		DC_FlushRange(pcmL, SOUND_MAX_SAMPLES_TRANSFER * sound_bps);
		if(sound_channels == 2)
			DC_FlushRange(pcmR, SOUND_MAX_SAMPLES_TRANSFER * sound_bps);

		IPC2->sound_writerequest = 0;
	}
}

void sound_init(void) {
	buffer_size = SOUND_BUFFER_SAMPLES;
	buffer_samples = buffer_end = buffer_pos = 0;
	buffer_lowest = 0xffff;

	IPC2->sound_tables = tables;

	/* these are never freed */
	pcmbufL = (char *) malloc(SOUND_BUFFER_SAMPLES * 2);
	pcmbufR = (char *) malloc(SOUND_BUFFER_SAMPLES * 2);
	pcmL = (char *) malloc(SOUND_MAX_SAMPLES_TRANSFER * 2);
	pcmR = (char *) malloc(SOUND_MAX_SAMPLES_TRANSFER * 2);
}

void sound_setup(struct media *m) {
	int target_samples;

	switch(m->format) {
		case MAD:
			madplay(m);
			break;

		case TREMOR:
			tremor_play(m);
			break;

		case FLAC:
			flac_play(m);
			break;

		default:
			break;
	}

	if(state == ERROR) {
		sound_stop();
	} else if(state == WAITING) {
		state = PLAYING;
	} else {
		IPC2->sound_channels = sound_channels;
		IPC2->sound_frequency = sound_samplerate;
		IPC2->sound_bytes_per_sample = sound_bps;
		IPC2->sound_samples = SOUND_MAX_SAMPLES_TRANSFER;
		IPC2->sound_writerequest = 0;

		IPC2->sound_lbuf = (void *) pcmL;
		IPC2->sound_rbuf = (void *) pcmR;

		target_samples = SOUND_MAX_SAMPLES_TRANSFER * 3;
		while(buffer_samples < target_samples && state != ERROR && state != FINISHING) {
			switch(format) {
				case TREMOR:
					tremor_update();
					break;

				case FLAC:
					flac_update();
					break;

				case MAD:
					madplay_update();
					break;

				default:
					buffer_samples = target_samples;
					break;
			}
		}

		irqSet(IRQ_IPC_SYNC, InterruptHandler_IPC_SYNC);
		irqEnable(IRQ_IPC_SYNC);
		REG_IPC_SYNC = IPC_SYNC_IRQ_ENABLE;
	}
}

void sound_stop(void) {
	if(state == STOPPED)
		return;

	if(IPC2->sound_state == IPC2_PLAYING)
		IPC_SendSync(IPC2_REQUEST_STOP_PLAYING);

	irqClear(IRQ_IPC_SYNC);
	irqDisable(IRQ_IPC_SYNC);

	if(!sound_wait_for_arm7_state(IPC2_STOPPED, "sound: timeout stopping arm7")) {
		state = ERROR;
		return;
	}

	switch(format) {
		case MAD:
			madplay_stop();
			break;

		case TREMOR:
			tremor_stop();
			break;

		case FLAC:
			flac_stop();
			break;

		default:
			break;
	}

	format = UNKNOWN;
	state = STOPPED;
	buffer_samples = buffer_end = buffer_pos = 0;
}

void sound_rewind(void) {
	u32 newpos;

	if(state != SEEKING) {
		seeking_state = state;
		seek_pos = sound_position();
		buffer_pos = buffer_end = buffer_samples = 0;
	}
	if(state == PLAYING)
		sound_playpause();
	state = SEEKING;
	seek_delay = 30;

	newpos = (u32) (sound_position() - 0.01 * sound_size());

	if(newpos <= 0.01 * sound_size())
		return;

	seek_pos = newpos;
}

void sound_forward(void) {
	u32 newpos;

	if(state != SEEKING) {
		seeking_state = state;
		seek_pos = sound_position();
		buffer_pos = buffer_end = buffer_samples = 0;
	}
	if(state == PLAYING)
		sound_playpause();
	state = SEEKING;
	seek_delay = 30;

	newpos = (u32) (sound_position() + 0.01 * sound_size());

	if(newpos >= 0.99 * sound_size())
		return;

	seek_pos = newpos;
}

void sound_finishing(void) {
	switch(format) {
		case MAD:
			madplay_stop();
			break;

		case TREMOR:
			tremor_stop();
			break;

		case FLAC:
			flac_stop();
			break;

		default:
			break;
	}

	format = UNKNOWN;
	state = WAITING;
}

void sound_playpause(void) {
	if(state == PLAYING)
		IPC_SendSync(IPC2_REQUEST_STOP_PLAYING);
	else if(state == STOPPED || state == PAUSED)
		IPC2->sound_control = IPC2_SOUND_START;

	if(state == PAUSED || state == STOPPED) {
		if(!sound_wait_for_arm7_state(IPC2_PLAYING, "sound: timeout starting arm7")) {
			state = ERROR;
			return;
		}
		state = PLAYING;
	} else if(state == PLAYING) {
		if(!sound_wait_for_arm7_state(IPC2_STOPPED, "sound: timeout pausing arm7")) {
			state = ERROR;
			return;
		}
		state = PAUSED;
	}
}

void sound_update(void) {
	if(state == ERROR)
		sound_stop();

	if(state == FINISHING)
		input_sound_finished();

	if(state == PLAYING &&
		(IPC2->sound_frequency != sound_samplerate
		|| IPC2->sound_bytes_per_sample != sound_bps
		|| IPC2->sound_channels != sound_channels)) {
		sound_playpause();
		IPC2->sound_channels = sound_channels;
		IPC2->sound_frequency = sound_samplerate;
		IPC2->sound_bytes_per_sample = sound_bps;
		sound_playpause();
	}

	if(state == SEEKING) {
		if(seek_delay > 0) {
			seek_delay--;
			return;
		} else {
			switch(format) {
				case MAD:
					madplay_seek(seek_pos);
					break;

				case TREMOR:
					tremor_seek(seek_pos);
					break;

				case FLAC:
					flac_seek(seek_pos);
					break;

				default:
					break;
			}

			buffer_samples = buffer_pos = buffer_end = 0;
			sound_flush();

			if(seeking_state == PLAYING) {
				switch(format) {
					case TREMOR:
						tremor_update();
						break;

					case FLAC:
						flac_update();
						break;

					case MAD:
						madplay_update();
						break;

					default:
						break;
				}
				state = PAUSED;

				sound_playpause();
			}
			state = seeking_state;
		}
	}

	switch(format) {
		case TREMOR:
			tremor_update();
			break;

		case FLAC:
			flac_update();
			break;

		case MAD:
			madplay_update();
			break;

		default:
			break;
	}
}

u32 sound_size(void) {
	switch(format) {
		case TREMOR:
			return tremor_size();
			break;

		case MAD:
			return madplay_size();
			break;

		case FLAC:
			return flac_size();
			break;

		default:
			return 1;
			break;
	}
}

u32 sound_position(void) {
	if(state == SEEKING)
		return seek_pos;

	switch(format) {
		case TREMOR:
			return tremor_position();
			break;

		case MAD:
			return madplay_position();
			break;

		case FLAC:
			return flac_position();
			break;

		default:
			return 0;
			break;
	}
}

void sound_flush(void) {
	switch(format) {
		case TREMOR:
			tremor_flush();
			break;

		case FLAC:
			flac_flush();
			break;

		case MAD:
			madplay_flush();
			break;

		default:
			break;
	}
}

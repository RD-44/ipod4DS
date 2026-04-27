/*
 * ipc2.h
 *
 * Based on ipc.h in libnds which is
	Copyright (C) 2005
		Michael Noland (joat)
		Jason Rogers (dovoto)
		Dave Murphy (WinterMute)
	*
	*/

#ifndef NDS_IPC2_INCLUDE
#define NDS_IPC2_INCLUDE

#define IPC2_SOUND_START 1
#define IPC2_SOUND_STOP 2
#define IPC2_SOUND_CLICK 3

/* from ARM7 to ARM9 */
#define IPC2_REQUEST_WRITE_SOUND 1

/* from ARM9 to ARM7 */
#define IPC2_REQUEST_START_PLAYING 1
#define IPC2_REQUEST_STOP_PLAYING 2
#define IPC2_REQUEST_SET_BACKLIGHTS_OFF 3
#define IPC2_REQUEST_SET_BACKLIGHTS_ON 4
#define IPC2_REQUEST_LEDBLINK_OFF 5
#define IPC2_REQUEST_LEDBLINK_ON 6

#define IPC2_STOPPED 1
#define IPC2_PLAYING 2

#define IPC2_SOUND_MODE_PCM_OVERSAMPLING4x 1
#define IPC2_SOUND_MODE_PCM_OVERSAMPLING2x 2
#define IPC2_SOUND_MODE_PCM_NORMAL 3

#define IPC2_MAX_MESSAGES 1
#define IPC2_MAX_MESSAGES2 1

#include <nds/ipc.h>

#define IPC_BASE_ADDR 0x02FFF000

#ifndef IPC
typedef struct {
	union {
		struct {
			u8 unused;
			struct {
				u8 year;
				u8 month;
				u8 day;
				u8 weekday;
				u8 hours;
				u8 minutes;
				u8 seconds;
			} rtc;
		};
		u8 curtime[8];
	};
} TransferTimeCompat;

typedef struct {
	vs16 touchX;
	vs16 touchY;
	vs16 touchXpx;
	vs16 touchYpx;
	vs16 touchZ1;
	vs16 touchZ2;
	vu16 buttons;
	vu16 _padding;
	u32 unixTime;
	u32 bootcode;
	vu16 battery;
	vu16 aux;
	vu16 mailBusy;
	vu16 _padding2;
	TransferTimeCompat time;
} TransferRegion;

#define IPC ((TransferRegion volatile *)IPC_BASE_ADDR)
#endif

typedef struct sTransferRegion2 {
	void *sound_lbuf, *sound_rbuf;

	u8 sound_control;
	u8 sound_mode;
	u8 sound_state;

	u8 sound_channels;
	u16 sound_frequency;
	u8 sound_bytes_per_sample;
	u16 sound_samples;

	u8 sound_writerequest;

	u16 wheel_status;

	char message[1][96];
	u16 messageflag;
	char message2[1][96];
	u16 messageflag2;

	u16 sound_volume;
	s16 **sound_tables;

	u8 sound_arm7_mode;
	u8 arm7_stage;
	u16 arm7_last_keyxy;
	u32 arm7_main_loops;
	u32 arm7_vblanks;
	u16 arm7_touch_rawx;
	u16 arm7_touch_rawy;
	u16 arm7_touch_px;
	u16 arm7_touch_py;
	u16 arm7_touch_z1;
	u16 arm7_touch_z2;
	u16 arm7_touch_pen;
	u16 arm7_touch_valid;
} TransferRegion2, * pTransferRegion2;


#define IPC2 ((TransferRegion2 volatile *)(IPC_BASE_ADDR + sizeof(TransferRegion)))

#endif

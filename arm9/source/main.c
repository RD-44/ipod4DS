#include <fat.h>
#include <nds.h>
#include <nds/system.h>
#include <nds/fifocommon.h>
#include <nds/arm9/console.h>
#include <stdio.h>

#include <ipc2.h>

#define POWER_OFF_VBLANKS 600

#include "screen.h"
#include "sound.h"
#include "playlist.h"
#include "input.h"

static void show_boot_message(const char *title, const char *body) {
	videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE);
	vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
	BG_PALETTE[255] = RGB15(0,0,0);
	consoleInit(NULL, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
	consoleClear();
	iprintf("%s\n\n%s\n", title, body);
}

int main(void) {
	unsigned power_state = 0, power_vblanks = POWER_OFF_VBLANKS;
	int lastkeys;

	powerOn(POWER_ALL_2D);

	irqInit();
	fifoInit();
	fifoSetValue32Handler(FIFO_SYSTEM, systemValueHandler, 0);
	fifoSetDatamsgHandler(FIFO_SYSTEM, systemMsgHandler, 0);
	irqEnable(IRQ_VBLANK);

	if(fatInitDefault() != 1) {
		show_boot_message(
			"ipod4DS",
			"FAT init failed.\n"
			"Your emulator probably needs\n"
			"a mounted SD card / DLDI-backed\n"
			"filesystem with the app data."
		);
		while(1)
			swiWaitForVBlank();
	}

	screen_initdisplays();

	IPC2->messageflag = IPC2->messageflag2 = 0;
	input_hold = INPUT_HOLD_OFF;

	IPC2->sound_volume = 0x3fff;
	IPC2->sound_control = 0;
	IPC2->sound_mode = IPC2_SOUND_MODE_PCM_NORMAL;
	state = STOPPED;
	format = UNKNOWN;
	sound_init();

	scanKeys();
	file_scan("/");

	file_sort();

	if(media == NULL) {
		show_boot_message(
			"ipod4DS",
			"No supported music files were found.\n\n"
			"Add .mp3, .ogg, or .flac files\n"
			"to the mounted SD folder and\n"
			"restart the app."
		);
		while(1)
			swiWaitForVBlank();
	}

	playlist_add(playlist_build_songs());

	scanKeys();
	if((keysHeld() & KEY_B) == 0) {
		file_scan_playlists("/");
	}

	srand(IPC->time.rtc.seconds + IPC->time.rtc.minutes * 60 + IPC->time.rtc.hours * 3600);

	playlist_shuffle(playlists.p[0]);
	shufflepos = playlist_add(shufflelist);

	currentmedia = media;
	screen_mainmenu();

	while(1) {
		int pen_down;

		scanKeys();
		pen_down = ((IPC->buttons & BIT(6)) == 0);

		if((keysDown() & KEY_SELECT) != 0) {
			// input_hold = (input_hold + 1) % 3 + 1;
			input_hold = 3 - input_hold;
			IPC2->wheel_status = 0;
		}

		screen_update();
		sound_update();

		if(keysHeld() != lastkeys
			|| pen_down
			|| keysHeld() & KEY_R
			|| keysHeld() & KEY_L) {
			lastkeys = keysHeld();
			power_vblanks = POWER_OFF_VBLANKS;
		} else if(power_vblanks > 0)
			power_vblanks--;

		if((power_vblanks == 0 || (keysHeld() & KEY_LID)) && power_state == 0) {
			power_state = 1;

			IPC_SendSync(IPC2_REQUEST_SET_BACKLIGHTS_OFF);
			swiWaitForVBlank(); /* HACK */
			IPC_SendSync(IPC2_REQUEST_LEDBLINK_ON);
			swiWaitForVBlank(); /* HACK */
		} else if(power_vblanks > 0 && (keysHeld() & KEY_LID) == 0 && power_state == 1) {
			power_state = 0;

			IPC_SendSync(IPC2_REQUEST_SET_BACKLIGHTS_ON);
			swiWaitForVBlank(); /* HACK */
			IPC_SendSync(IPC2_REQUEST_LEDBLINK_OFF);
			swiWaitForVBlank(); /* HACK */
		}

		swiWaitForVBlank();
	}

	return 0;
}

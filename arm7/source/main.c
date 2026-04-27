#include <nds.h>
#include <nds/arm7/input.h>
#include <nds/arm7/touch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ipc2.h>

#include "sound.h"

#define KEY_X BIT(0)
#define KEY_Y BIT(1)
#define PEN_DOWN BIT(6)
#define HINGE BIT(7)

void InterruptHandler_IPC_SYNC(void) {
	u8 sync;

	sync = IPC_GetSync();

	if(sync == IPC2_REQUEST_STOP_PLAYING) {
		IPC2->arm7_stage = 40;
		pcmstop();
		return;
	}

	{
		int oldval, newval;

		oldval = readPowerManagement(PM_CONTROL_REG);
		newval = oldval;

		if(sync == IPC2_REQUEST_SET_BACKLIGHTS_OFF)
			newval = oldval & ~PM_BACKLIGHT_TOP & ~PM_BACKLIGHT_BOTTOM;
		else if(sync == IPC2_REQUEST_SET_BACKLIGHTS_ON)
			newval = oldval | PM_BACKLIGHT_TOP | PM_BACKLIGHT_BOTTOM;
		else if(sync == IPC2_REQUEST_LEDBLINK_OFF)
			newval = oldval & ~PM_LED_BLINK;
		else if(sync == IPC2_REQUEST_LEDBLINK_ON)
			newval = oldval | PM_LED_BLINK;
		else
			return;

		IPC2->arm7_stage = 41;
		writePowerManagement(PM_CONTROL_REG, newval);
	}
}

static void arm7_update_input(void) {
	static int pen_armed;
	static touchPosition last_touch;
	u16 buttons;
	int pen_down;

	buttons = REG_KEYXY;
	pen_down = ((buttons & PEN_DOWN) == 0);
	IPC2->arm7_stage = 10;
	IPC2->arm7_last_keyxy = buttons;
	IPC2->arm7_touch_pen = pen_down;
	IPC2->arm7_touch_valid = 0;

	if(pen_down) {
		if(pen_armed) {
			touchPosition touchpos;

			touchReadXY(&touchpos);
			if(touchpos.rawx != 0 || touchpos.rawy != 0
				|| touchpos.px != 0 || touchpos.py != 0) {
				last_touch = touchpos;
				IPC2->arm7_touch_valid = 1;
			}
		} else {
			pen_armed = 1;
		}
	} else {
		pen_armed = 0;
		memset(&last_touch, 0, sizeof(last_touch));
	}

	IPC2->arm7_touch_rawx = last_touch.rawx;
	IPC2->arm7_touch_rawy = last_touch.rawy;
	IPC2->arm7_touch_px = last_touch.px;
	IPC2->arm7_touch_py = last_touch.py;
	IPC2->arm7_touch_z1 = last_touch.z1;
	IPC2->arm7_touch_z2 = last_touch.z2;

	IPC2->arm7_stage = 11;
	inputGetAndSend();
	IPC->battery = 0;
	IPC->aux = 0;
	IPC2->arm7_stage = 12;
}

void InterruptHandler_VBLANK(void) {
	IPC2->arm7_vblanks++;
	arm7_update_input();
}

int main(void) {
	u8 ct[sizeof(IPC->time)];
	int i;

	readUserSettings();
	touchInit();

	// Reset the clock if needed
	rtcReset();

	rtcGetTime((u8 *)ct);
	BCDToInteger((u8 *)&(ct[1]), 7);

	for(i=0; i<sizeof(ct); i++)
		IPC->time.curtime[i] = ct[i];

	irqInit();
	fifoInit();
	installSystemFIFO();
	irqEnable(IRQ_VBLANK);
	irqSet(IRQ_VBLANK, InterruptHandler_VBLANK);

	InitSoundDevice();
	IPC2->arm7_stage = 1;
	IPC2->arm7_last_keyxy = 0xffff;
	IPC2->arm7_main_loops = 0;
	IPC2->arm7_vblanks = 0;

	irqSet(IRQ_IPC_SYNC, InterruptHandler_IPC_SYNC);
	irqEnable(IRQ_IPC_SYNC);
	REG_IPC_SYNC = IPC_SYNC_IRQ_ENABLE;

	// Keep the ARM7 idle
	while (1) {
		IPC2->arm7_main_loops++;
		IPC2->arm7_stage = 2;
		IPC2->arm7_stage = 3;

		if(IPC2->sound_control == IPC2_SOUND_START) {
			IPC2->arm7_stage = 20;
			pcmplay();
		} else if(IPC2->sound_control == IPC2_SOUND_CLICK) {
			IPC2->arm7_stage = 30;
			playclick();
		}

		swiDelay(10000);
	}
}

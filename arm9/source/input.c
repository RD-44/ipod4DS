#include <nds.h>
#include <nds/arm9/console.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <ipc2.h>

#define INPUT_C

#include "input.h"
#include "screen.h"
#include "text.h"
#include "wheel.h"
#include "file.h"
#include "sound.h"
#include "playlist.h"
#include "tags.h"

static enum screen_menu nowplaying_last_menu;
static u32 nowplaying_last_selected;
static u32 music_last_selected;
static enum screen_menu playlist_last_menu;
static u32 playlist_last_selected;
static u32 playlists_last_selected;
static u32 playlists_playlist;
static u32 settings_last_selected;
static u32 skins_last_selected;

static u8 input_abort;

#define TOUCH_CENTER_X 128
#define TOUCH_CENTER_Y 96
#define TOUCH_PEN_DOWN BIT(6)
#define TOUCH_INNER_RADIUS 24
#define TOUCH_OUTER_RADIUS 76
#define TOUCH_CENTER_RADIUS 40
#define TOUCH_BUTTON_RADIUS 40
#define TOUCH_MENU_X 0
#define TOUCH_MENU_Y -54
#define TOUCH_FORWARD_X 56
#define TOUCH_FORWARD_Y 0
#define TOUCH_REWIND_X -56
#define TOUCH_REWIND_Y 0
#define TOUCH_PLAY_X 0
#define TOUCH_PLAY_Y 56

static u16 input_touch_button_status(int px, int py) {
	int distance = px * px + py * py;

	if(distance <= TOUCH_CENTER_RADIUS * TOUCH_CENTER_RADIUS)
		return WHEEL_CENTER;
	if(distance < TOUCH_INNER_RADIUS * TOUCH_INNER_RADIUS
		|| distance > TOUCH_OUTER_RADIUS * TOUCH_OUTER_RADIUS)
		return 0;

	if(abs(py) > abs(px)) {
		if(py < 0)
			return WHEEL_MENU;
		return WHEEL_PLAY;
	}

	if(px > 0)
		return WHEEL_FORWARD;
	if(px < 0)
		return WHEEL_REWIND;

	return 0;
}

static u16 input_touch_wheel_status(u32 held) {
	static int posx, posy;
	static int pressx, pressy;
	static int lastpx, lastpy;
	static int inwheel;
	static int moved;
	static int down;
	int px, py;
	int dx, dy;
	int distance;
	int arm7_pen_down;
	int arm7_touch_valid;
	int libnds_pen_down;
	int raw_pen_down;
	u16 wheel = 0;
	touchPosition touch;

	arm7_pen_down = (IPC2->arm7_touch_pen != 0);
	arm7_touch_valid = (IPC2->arm7_touch_valid != 0);
	libnds_pen_down = ((held & KEY_TOUCH) != 0);
	raw_pen_down = ((IPC->buttons & TOUCH_PEN_DOWN) == 0);

	if((arm7_pen_down && arm7_touch_valid) || libnds_pen_down || raw_pen_down) {
		if(arm7_pen_down && arm7_touch_valid) {
			px = IPC2->arm7_touch_px - TOUCH_CENTER_X;
			py = IPC2->arm7_touch_py - TOUCH_CENTER_Y;
		} else if(libnds_pen_down) {
			touchRead(&touch);
			px = touch.px - TOUCH_CENTER_X;
			py = touch.py - TOUCH_CENTER_Y;
		} else {
			px = IPC->touchXpx - TOUCH_CENTER_X;
			py = IPC->touchYpx - TOUCH_CENTER_Y;
		}

		if(down == 0) {
			pressx = lastpx = posx = px;
			pressy = lastpy = posy = py;

			down = 1;
			moved = 0;
		}

		distance = px * px + py * py;

		if(distance > TOUCH_OUTER_RADIUS * TOUCH_OUTER_RADIUS
			|| distance < TOUCH_INNER_RADIUS * TOUCH_INNER_RADIUS) {
			inwheel = 0;
		} else if(inwheel == 0) {
			inwheel = 1;
			posx = px;
			posy = py;
		} else {
			float angle;

			dx = px - posx;
			dy = py - posy;
			if(dx * dx + dy * dy < 64) {
				lastpx = px;
				lastpy = py;
				return 0;
			}

			angle = atan2((float) py, (float) px) - atan2((float) posy, (float) posx);
			if(angle > 8 * M_PI / 9)
				angle -= 2 * M_PI;
			if(angle < -8 * M_PI / 9)
				angle += 2 * M_PI;

			if(angle > M_PI / 9) {
				posx = px;
				posy = py;
				moved = 1;
				wheel |= WHEEL_RIGHT;
			} else if(angle < -M_PI / 9) {
				posx = px;
				posy = py;
				moved = 1;
				wheel |= WHEEL_LEFT;
			}
		}

		lastpx = px;
		lastpy = py;
		return wheel;
	}

	if(down != 0) {
		if(!moved) {
			if(abs(pressx - lastpx) < TOUCH_BUTTON_RADIUS && abs(pressy - lastpy) < TOUCH_BUTTON_RADIUS)
				wheel |= input_touch_button_status((pressx + lastpx) / 2, (pressy + lastpy) / 2);
		}

		inwheel = 0;
		down = 0;
	}

	return wheel;
}

void input_sound_finished(void) {
	sound_finishing();

	if(PLAYLIST->current >= PLAYLIST->size - 1)
		PLAYLIST->current = 0;
	else
		PLAYLIST->current++;

	currentmedia = PLAYLIST->list[PLAYLIST->current];

	if(screen_type == NOW_PLAYING)
		screen_nowplaying_reset();

	sound_setup(currentmedia);
}

void input_nowplaying_setup(int pos, const char *text) {
	input_abort = 1;

	nowplaying_last_selected = pos;
	nowplaying_last_menu = screen_menu;

	screen_type = NOW_PLAYING;
}

void input_musicmenu_setup(int pos, const char *text) {
	input_abort = 1;

	music_last_selected = pos;

	screen_musicmenu();
}

void input_settings_setup(int pos, const char *text) {
	input_abort = 1;

	settings_last_selected = pos;

	screen_settingsmenu();
}

void input_skins_setup(int pos, const char *text) {
	input_abort = 1;

	skins_last_selected = pos;

	screen_skinsmenu();
}

void input_songs_setup(int pos, const char *text) {
	input_abort = 1;

	playlists_last_selected = 0;
	playlists_playlist = 0;

	input_playlist_setup(0, "Songs");
}

void input_playlist_setup(int pos, const char *text) {
	input_abort = 1;

	playlist_last_selected = pos;
	playlist_last_menu = screen_menu;

	screen_playlistmenu(playlists.p[pos]);
}

void input_playlists_setup(int pos, const char *text) {
	input_abort = 1;

	playlists_last_selected = pos;

	screen_playlistsmenu();
}

void input_tags_scan(int pos, const char *text) {
	int i;
	int old;

	input_abort = 1;

	old = playlists.current;
	playlists.current = 0;

	for(i = 0; i < PLAYLIST->size; i++) {
		tags_scan(PLAYLIST->list[i]);
	}

	playlists.current = old;
}

void input_shuffle_setup(int pos, const char *text) {
	input_abort = 1;

	if(playlists.current == shufflepos)
		playlists.current = 0;
	playlist_shuffle(playlists.p[playlists.current]);
	playlist_change(shufflepos, shufflelist);
	playlists.current = shufflepos;
	currentmedia = PLAYLIST->list[PLAYLIST->current];

	menu_stop();
	menu_play();

	input_nowplaying_setup(pos, text);
}

void input_playlist(int pos, const char *text) {
	if(playlists.current != playlists_playlist || PLAYLIST->current != pos) {
		playlists.current = playlists_playlist;

		PLAYLIST->current = pos;

		currentmedia = PLAYLIST->list[pos];

		menu_play();
	} else if(state == PAUSED)
		sound_playpause();
	else if(state != PLAYING)
		menu_play();

	input_nowplaying_setup(pos, text);
}

void input_playlists(int pos, const char *text) {
	playlists_playlist = pos;

	input_playlist_setup(pos, text);
}

static void menu_down(void) {
	if(screen_menu_selected < screen_menu_items - 1)
		screen_menu_selected++;
}

static void menu_up(void) {
	if(screen_menu_selected > 0)
		screen_menu_selected--;
}

static void menu_center(void) {
	if(currentmenu[screen_menu_selected].callback != NULL)
		currentmenu[screen_menu_selected].callback(screen_menu_selected, currentmenu[screen_menu_selected].label);
}

static void menu_stop(void) {
	sound_stop();
}

static void menu_play(void) {
	sound_stop();

	sound_setup(currentmedia);
	if(state != ERROR)
		sound_playpause();
}

static void menu_next(void) {
	if(state == STOPPED) {
		if(PLAYLIST->current == PLAYLIST->size - 1)
			PLAYLIST->current = 0;
		else
			PLAYLIST->current++;

		currentmedia = PLAYLIST->list[PLAYLIST->current];
	} else if(state == PLAYING) {
		menu_stop();

		if(PLAYLIST->current == PLAYLIST->size - 1)
			PLAYLIST->current = 0;
		else
			PLAYLIST->current++;

		currentmedia = PLAYLIST->list[PLAYLIST->current];

		menu_play();
	}
}

static void menu_prev(void) {
	if(state == STOPPED) {
		if(PLAYLIST->current == 0)
			PLAYLIST->current = PLAYLIST->size - 1;
		else
			PLAYLIST->current--;

		currentmedia = PLAYLIST->list[PLAYLIST->current];
	} else if(state == PLAYING) {
		menu_stop();

		if(PLAYLIST->current == 0)
			PLAYLIST->current = PLAYLIST->size - 1;
		else
			PLAYLIST->current--;

		currentmedia = PLAYLIST->list[PLAYLIST->current];

		menu_play();
	}
}

static void nowplaying_back(void) {
	input_abort = 1;

	switch(nowplaying_last_menu) {
		case MUSIC:
			screen_musicmenu();
			break;

		case LIST:
			screen_playlistmenu(PLAYLIST);
			break;

		default:
		case MAIN:
			screen_mainmenu();
			break;
	}


	screen_menu_selected = nowplaying_last_selected;
}

static void nowplaying_next(void) {
	if(state != PLAYING)
		menu_stop();

	menu_next();

	screen_nowplaying_reset();
}

static void nowplaying_prev(void) {
	if(state != PLAYING)
		menu_stop();

	menu_prev();

	screen_nowplaying_reset();
}

static void nowplaying_playpause(void) {
	if(state == PLAYING || state == PAUSED)
		sound_playpause();
	else if(state == STOPPED)
		menu_play();
}

static void nowplaying_R(void) {
	if(keysHeld() & KEY_LID) { /* lid closed */
		if(keysHeld() & KEY_L) {
			nowplaying_next();
		}
	}
}

static void nowplaying_L(void) {
	if(keysHeld() & KEY_LID) { /* lid closed */
		if(keysHeld() & KEY_R) {
			nowplaying_playpause();
		}
	}
}

static void nowplaying_right(void) {
	if(keysHeld() & KEY_LID)
		return;

	if(nowplaying_mode == NOWPLAYING_SEEK) {
		nowplaying_delay = 60;
		sound_forward();
	} else if(nowplaying_mode == NOWPLAYING_VOLUME
		|| nowplaying_mode == NOWPLAYING_VOLUME_CHANGING) {
		nowplaying_mode = NOWPLAYING_VOLUME_CHANGING;
		nowplaying_delay = 60;

		if(IPC2->sound_volume > 0xffff - 0.01f * 0xffff)
			IPC2->sound_volume = 0xffff;
		else
			IPC2->sound_volume += 0.01f * 0xffff;
	}
}

static void nowplaying_left(void) {
	if(keysHeld() & KEY_LID)
		return;

	if(nowplaying_mode == NOWPLAYING_SEEK) {
		nowplaying_delay = 60;
		sound_rewind();
	} else if(nowplaying_mode == NOWPLAYING_VOLUME
		|| nowplaying_mode == NOWPLAYING_VOLUME_CHANGING) {
		nowplaying_mode = NOWPLAYING_VOLUME_CHANGING;
		nowplaying_delay = 60;

		if(IPC2->sound_volume < 0.01f * 0xffff)
			IPC2->sound_volume = 0;
		else
			IPC2->sound_volume -= 0.01f * 0xffff;
	}
}

static void nowplaying_changemode(void) {
	if(nowplaying_mode == NOWPLAYING_SEEK)
		nowplaying_mode = NOWPLAYING_VOLUME;
	else {
		nowplaying_mode = NOWPLAYING_SEEK;
		nowplaying_delay = 60;
	}
}

static void music_back(void) {
	input_abort = 1;

	screen_mainmenu();

	screen_menu_selected = music_last_selected;
}

static void music_down(void) {
	if(screen_menu_selected < screen_menu_items - 1)
		screen_menu_selected++;
}

static void music_up(void) {
	if(screen_menu_selected > 0)
		screen_menu_selected--;
}

static void music_center(void) {
	if(currentmenu[screen_menu_selected].callback != NULL)
		currentmenu[screen_menu_selected].callback(screen_menu_selected, currentmenu[screen_menu_selected].label);
}

static void playlist_back(void) {
	input_abort = 1;

	switch(playlist_last_menu) {
		case MUSIC:
			screen_musicmenu();
			break;

		case LISTS:
			screen_playlistsmenu();
			break;

		default:
		case MAIN:
			screen_mainmenu();
			break;
	}


	screen_menu_selected = playlist_last_selected;
}

static void playlist_up(void) {
	if(screen_menu_selected > 0)
		screen_menu_selected--;
}

static void playlist_down(void) {
	if(screen_menu_selected < screen_menu_items - 1)
		screen_menu_selected++;
}

static void playlist_center(void) {
	if(currentmenu[screen_menu_selected].callback != NULL)
		currentmenu[screen_menu_selected].callback(screen_menu_selected, currentmenu[screen_menu_selected].label);
}

static void playlists_back(void) {
	input_abort = 1;

	screen_musicmenu();

	screen_menu_selected = playlists_last_selected;
}

static void playlists_up(void) {
	if(screen_menu_selected > 0)
		screen_menu_selected--;
}

static void playlists_down(void) {
	if(screen_menu_selected < screen_menu_items - 1)
		screen_menu_selected++;
}

static void playlists_center(void) {
	if(currentmenu[screen_menu_selected].callback != NULL)
		currentmenu[screen_menu_selected].callback(screen_menu_selected, currentmenu[screen_menu_selected].label);
}

static void settings_back(void) {
	input_abort = 1;

	screen_mainmenu();

	screen_menu_selected = settings_last_selected;
}

static void settings_down(void) {
	if(screen_menu_selected < screen_menu_items - 1)
		screen_menu_selected++;
}

static void settings_up(void) {
	if(screen_menu_selected > 0)
		screen_menu_selected--;
}

static void settings_center(void) {
	if(currentmenu[screen_menu_selected].callback != NULL)
		currentmenu[screen_menu_selected].callback(screen_menu_selected, currentmenu[screen_menu_selected].label);
}

static void skins_back(void) {
	input_abort = 1;

	screen_settingsmenu();

	screen_menu_selected = skins_last_selected;
}

static void skins_down(void) {
	if(screen_menu_selected < screen_menu_items - 1)
		screen_menu_selected++;
}

static void skins_up(void) {
	if(screen_menu_selected > 0)
		screen_menu_selected--;
}

static void skins_center(void) {
	if(currentmenu[screen_menu_selected].callback != NULL)
		currentmenu[screen_menu_selected].callback(screen_menu_selected, currentmenu[screen_menu_selected].label);
}

void input_handleinput(void) {
	u16 wheel;
	u32 keys, held;
	int i;

	if(input_hold == INPUT_HOLD_ALL || input_hold == INPUT_HOLD_NORMAL)
		return;

	keys = keysDown();
	held = keysHeld();
	wheel = IPC2->wheel_status | input_touch_wheel_status(held);
	IPC2->wheel_status = 0;

	input_abort = 0;
	for(i = 0; input_abort == 0 && currenthandle[i].type != END; i++) {
		switch(currenthandle[i].type) {
			case WHEEL:
				if(wheel & currenthandle[i].key)
					currenthandle[i].callback();
				break;

			case KEY_PRESS:
				if(keys & currenthandle[i].key)
					currenthandle[i].callback();
				break;

			case KEY_HELD:
				if(held & currenthandle[i].key)
					currenthandle[i].callback();
				break;

			default:
				break;
		}
	}
}

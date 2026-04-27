#include <fat.h>

#include <nds.h>

#include <sys/dir.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define FILE_C
#include "file.h"

#include "heap.h"
#include "playlist.h"

#define BLOCK 1
#define SCAN_QUEUE_BLOCK 32

static struct media *freelist;
struct media *media;
struct media *currentmedia;

static void wait(void) {
	int i;

	for(i=0; i<60; i++)
		swiWaitForVBlank();
}

static void scan_malloc_failed(const char *where) {
	printf("%s: malloc failed!\n", where);
	while(1)
		swiWaitForVBlank();
}

static int scan_should_skip(const char *filename) {
	if(strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0)
		return 1;

	if(filename[0] == '.')
		return 1;

	if(strcasecmp(filename, "moonshl") == 0)
		return 1;

	return 0;
}

static void scan_queue_push(char ***queue, unsigned *qcapacity, unsigned *qend, const char *path, const char *where) {
	char **newqueue;

	if(*qend >= *qcapacity) {
		*qcapacity += SCAN_QUEUE_BLOCK;
		newqueue = (char **) realloc(*queue, *qcapacity * sizeof(char *));
		if(newqueue == NULL)
			scan_malloc_failed(where);
		*queue = newqueue;
	}

	(*queue)[*qend] = strdup(path);
	if((*queue)[*qend] == NULL)
		scan_malloc_failed(where);

	(*qend)++;
}

struct media *media_alloc(void) {
	static int block = BLOCK;
	struct media *r;

	if(freelist == NULL) {
		int i;

		freelist = (struct media *) malloc(block * sizeof(struct media));
		if(freelist == NULL)
			return NULL;

		for(i=0; i<block; i++)
			freelist[i].next  = &freelist[i+1];
		freelist[block-1].next = NULL;

		block <<= 1;
	}

	r = freelist;
	freelist = r->next;

	return r;
}

static enum format verify_type(char *path, char *filename) {
	int s = strlen(filename);

	if(s < 3)
		return UNKNOWN;

	if(tolower(filename[s-1]) == '3' && tolower(filename[s-2]) == 'p' && tolower(filename[s-3]) == 'm')
		return MAD;

	if(tolower(filename[s-1]) == 'g' && tolower(filename[s-2]) == 'g' && tolower(filename[s-3]) == 'o')
		return TREMOR;

	if(tolower(filename[s-1]) == 'c' && tolower(filename[s-2]) == 'a' && tolower(filename[s-3]) == 'l')
		return FLAC;

	return UNKNOWN;
}

static void add_media(char *path, char *filename, enum format type) {
	struct media *m;
	int plen = strlen(path);

	m = media_alloc();

	if(m == NULL) {
		printf("add_media: malloc failed!\n");
		while(1)
			swiWaitForVBlank();
	}

	m->path = (char *) malloc(plen + strlen(filename) + 2);

	if(m->path == NULL) {
		printf("add_media: malloc failed!\n");
		while(1)
			swiWaitForVBlank();
	}

	strcpy(m->path, path);
	if(path[plen - 1] != '/')
		strcat(m->path, "/");
	strcat(m->path, filename);
	m->format = type;

	m->artist = NULL;
	m->album = NULL;

	m->title = strdup(filename);

	if(m->title == NULL) {
		printf("add_media: malloc failed!\n");
		while(1)
			swiWaitForVBlank();
	}

	if(media == NULL) {
		media = m;
		m->next = m;
		m->prev = m;
	} else {
		m->next = media;
		m->prev = media->prev;
		media->prev->next = m;
		media->prev = m;
		media = m;
	}
}

static int heap_cmp(const void *e1, const void *e2) {
	const struct media *m1 = (const struct media *) e1,
				*m2 = (const struct media *) e2;

	if(e1 == e2)
		return 0;

	if(e1 == NULL)
		return -1;
	if(e2 == NULL)
		return 1;

	return strcasecmp(m1->path, m2->path);
}

int file_scan(char *path) {
	DIR *d;
	struct dirent *entry;
	struct stat s;
	char filename[256];
	char fullpath[1024];
	char **queue;
	unsigned qcapacity, qinit, qend;

	qcapacity = SCAN_QUEUE_BLOCK;
	queue = (char **) malloc(qcapacity * sizeof(char *));
	if(queue == NULL)
		scan_malloc_failed("file_scan");

	qinit = qend = 0;
	scan_queue_push(&queue, &qcapacity, &qend, path, "file_scan");

	while(qinit < qend) {
		d = opendir(queue[qinit]);
		if(d == NULL) {
			free(queue);
			return 1;
		}

		while((entry = readdir(d)) != NULL) {
			strcpy(filename, entry->d_name);

			if(scan_should_skip(filename))
				continue;

			strcpy(fullpath, queue[qinit]);
			if(fullpath[strlen(fullpath) - 1] != '/')
				strcat(fullpath, "/");
			strcat(fullpath, filename);

			if(stat(fullpath, &s) == -1)
				continue;

			if(S_ISDIR(s.st_mode)) {
				scan_queue_push(&queue, &qcapacity, &qend, fullpath, "file_scan");
			} else if(S_ISREG(s.st_mode)) {
				enum format type;

				if((type = verify_type(queue[qinit], filename)) != UNKNOWN) {
					add_media(queue[qinit], filename, type);
				}
			}
		}

		if(closedir(d) == -1) {
			free(queue[qinit]);
			free(queue);
			return 1;
		}

		free(queue[qinit]);
		qinit++;
	}

	free(queue);
	return 0;
}

static int verify_playlist(char *path, char *filename) {
	int s = strlen(filename);

	if(s < 3)
		return 0;

	if(tolower(filename[s-1]) == 'u' && tolower(filename[s-2]) == '3' && tolower(filename[s-3]) == 'm')
		return 1;

	return 0;
}

int file_scan_playlists(char *path) {
	DIR *d;
	struct dirent *entry;
	struct stat s;
	char filename[256];
	char fullpath[1024];
	char **queue;
	unsigned qcapacity, qinit, qend;

	qcapacity = SCAN_QUEUE_BLOCK;
	queue = (char **) malloc(qcapacity * sizeof(char *));
	if(queue == NULL)
		scan_malloc_failed("file_scan_playlists");

	qinit = qend = 0;
	scan_queue_push(&queue, &qcapacity, &qend, path, "file_scan_playlists");

	while(qinit < qend) {
		d = opendir(queue[qinit]);

		if(d == NULL) {
			free(queue);
			return 1;
		}

		while((entry = readdir(d)) != NULL) {
			strcpy(filename, entry->d_name);

			if(scan_should_skip(filename))
				continue;

			strcpy(fullpath, queue[qinit]);
			if(fullpath[strlen(fullpath) - 1] != '/')
				strcat(fullpath, "/");
			strcat(fullpath, filename);

			if(stat(fullpath, &s) == -1)
				continue;

			if(S_ISDIR(s.st_mode)) {
				scan_queue_push(&queue, &qcapacity, &qend, fullpath, "file_scan_playlists");
			} else if(S_ISREG(s.st_mode)) {
				if((verify_playlist(queue[qinit], filename)) == 1) {
					playlist_add(playlist_process(queue[qinit], filename));
				}
			}
		}

		if(closedir(d) == -1) {
			free(queue[qinit]);
			free(queue);
			return 1;
		}

		free(queue[qinit]);
		qinit++;
	}

	free(queue);
	return 0;
}

void file_sort(void) {
	tHEAP h;
	struct media *m;

	if(media == NULL)
		return;

	h = heap_init(BLOCK, heap_cmp);

	while(media->next != media) {
		max_heap_insert(h, media);

		media->prev->next = media->next;
		media->next->prev = media->prev;
		media = media->next;
	}
	max_heap_insert(h, media);

	m = heap_extract_max(h);
	m->prev = m;
	m->next = m;
	media = m;

	while((m = heap_extract_max(h)) != NULL) {
		m->next = media->next;
		m->prev = media;
		media->next->prev = m;
		media->next = m;
	}
	media = media->next;

	heap_destroy(h);
}

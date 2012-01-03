#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <SDL/SDL.h>
#include <string.h>

#define WIDTH 800
#define HEIGHT 600
#define FADE 10
#define MOUSEDIST 4
#define MARGIN 10

SDL_Surface *screen;
int lastmousex, lastmousey;
int lastmousebtn;

unsigned int seed = 0x91236537;
unsigned int myrand() {
	return (seed = seed * 0x16138913 + 0xdeadbeef);
}

void SDL_ClearScreen(void)
{
	SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
}

void lockScreen(void) {
	if ( SDL_MUSTLOCK(screen) ) {
		while(SDL_LockSurface(screen) < 0) {
			usleep(1);
		}
	}
}

void unlockScreen(void) {
	if ( SDL_MUSTLOCK(screen) ) {
		SDL_UnlockSurface(screen);
	}
}

void drawpixel(int x, int y, Uint32 color) {
	*((Uint32 *)screen->pixels + y*screen->pitch/4 + x) = color;
}

void incpixel(int x, int y, Uint32 dcolor) {
	Uint32 *color = ((Uint32 *)screen->pixels + y*screen->pitch/4 + x);
	int blue = ((*color) & 0xff0000) >> 16;
	int green = ((*color) & 0xff00) >> 8;
	int red = ((*color) & 0xff);
	int dblue = (dcolor & 0xff0000) >> 16;
	int dgreen = (dcolor & 0xff00) >> 8;
	int dred = dcolor & 0xff;
	blue = blue < (0xff - dblue)? blue + dblue: 0xff;
	green = green < (0xff - dgreen)? green + dgreen: 0xff;
	red = red < (0xff - dred)? red + dred: 0xff;
	*color = blue * 0x010000 + green * 0x0100 + red;
}

int calcX(int a, int b, int c, int d) {
	Uint32 x = a + b * b * 13 + c * c * c * 51 + d * d * d * d * 73;
	return (x % (WIDTH - 2 * MARGIN)) + MARGIN;
}

int calcY(int a, int b, int c, int d) {
	Uint32 y = b + c * c * 13 + d * d * d * 51 + a * a * a * a * 73;
	return (y % (HEIGHT - 2 * MARGIN)) + MARGIN;
}

void drawline(int sx, int sy, int tx, int ty, Uint32 color, int maxrecur) {
	if(maxrecur <= 0) {
		incpixel(sx + (myrand() % 5) - 2, sy + (myrand() % 5 - 2), color);
	} else {
		drawline(sx, sy, (sx + tx) / 2, (sy + ty) / 2, color, maxrecur - 1);
		drawline((sx + tx) / 2, (sy + ty) / 2, tx, ty, color, maxrecur - 1);
	}
}

void drawline(int sa, int sb, int sc, int sd, int ta, int tb, int tc, int td,
		Uint32 color, int maxrecur) {
	int sx = calcX(sa, sb, sc, sd);
	int sy = calcY(sa, sb, sc, sd);
	int tx = calcX(ta, tb, tc, td);
	int ty = calcY(ta, tb, tc, td);

	drawline(sx, sy, tx, ty, color, maxrecur);
}

inline int myabs(int i) { return i < 0? -i: i; }

int drawhost(int a, int b, int c, int d, Uint32 color) {
	int x = calcX(a, b, c, d);
	int y = calcY(a, b, c, d);

	for(int xx = x - 1; xx <= x + 1; xx++) {
		for(int yy = y - 1; yy <= y + 1; yy++) {
			drawpixel(xx, yy, color);
		}
	}

	if((lastmousebtn & SDL_BUTTON(1)) &&
			myabs(lastmousex - x) < MOUSEDIST &&
			myabs(lastmousey - y) < MOUSEDIST) {
		return 1;
	}

	return 0;
	
}

void parseArp(char *line) {
	int sa, sb, sc, sd;
	int ta, tb, tc, td;

	char *start = strstr(line, "who-has");
	if(!start) return;

	sscanf(start, "who-has %d.%d.%d.%d", &ta, &tb, &tc, &td);
	if(drawhost(ta, tb, tc, td, 0xff8080)) {
		printf("Host (arp-tg): %d.%d.%d.%d\n", ta, tb, tc, td);
	}

	char *source = strstr(start, "tell");
	if(!source) return;

	sscanf(source, "tell %d.%d.%d.%d", &sa, &sb, &sc, &sd);
	if(drawhost(sa, sb, sc, sd, 0xff8080)) {
		printf("Host (arp-sr): %d.%d.%d.%d\n", sa, sb, sc, sd);
	}

	drawline(sa, sb, sc, sd, ta, tb, tc, td, 0x004000, 10);
}

void parse(char *line) {
	int sa, sb, sc, sd, sp;
	int ta, tb, tc, td, tp;

	char *start = strstr(line, "IP");
	if(!start) {
		parseArp(line);
		return;
	}

	sscanf(start, "IP %d.%d.%d.%d.%d", &sa, &sb, &sc, &sd, &sp);
	if(drawhost(sa, sb, sc, sd, 0x80ff80)) {
		printf("Host (source): %d.%d.%d.%d:%d\n", sa, sb, sc, sd, sp);
	}
	
	char *target = strstr(start, ">");
	if(!target) return;

	sscanf(target, "> %d.%d.%d.%d.%d", &ta, &tb, &tc, &td, &tp);
	if(drawhost(ta, tb, tc, td, 0xff8080)) {
		printf("Host (target): %d.%d.%d.%d:%d\n", ta, tb, tc, td, tp);
	}

	Uint32 color = 0x000020;
	int maxrecur = 10;
	if(tp == 80 || sp == 80) {
		color = 0x200000;
		maxrecur = 10;
	} else if(tp == 137 || sp == 137) {
		color = 0x800080;
		maxrecur = 7;
	} else if(tp == 22 || sp == 22) {
		color = 0x202000;
		maxrecur = 10;
	}
	drawline(sa, sb, sc, sd, ta, tb, tc, td, color, maxrecur);
}

void fade() {
	for(int x = 0; x < WIDTH; x++) {
		for(int y = 0; y < HEIGHT; y++) {
			Uint32 *color = ((Uint32 *)screen->pixels + y*screen->pitch/4 + x);
			int blue = ((*color) & 0xff0000) >> 16;
			int green = ((*color) & 0xff00) >> 8;
			int red = ((*color) & 0xff);
			blue = blue > FADE? blue - FADE: 0;
			green = green > FADE? green - FADE: 0;
			red = red > FADE? red - FADE: 0;
			*color = blue * 0x010000 + green * 0x0100 + red;
		}
	}
}

int main(void) {
	char line[4096];
	if(SDL_Init(SDL_INIT_VIDEO) < 0) {
		return 1;
	}
	atexit(SDL_Quit);

	if(!(screen = SDL_SetVideoMode(WIDTH, HEIGHT, 32, SDL_HWSURFACE))) {
		return 1;
	}

	SDL_ClearScreen();

	int infd = fileno(stdin);
	SDL_Event event;

	while(1) {
		fd_set input;
		FD_SET(infd, &input);
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 10000;
		lockScreen();
		while(select(infd + 1, &input, NULL, NULL, &tv) > 0) {
			if(!fgets(line, 4096, stdin)) return 0;
			if(!line) break;
			parse(line);
		}

		fade();
		unlockScreen();

		SDL_UpdateRect(screen, 0, 0, WIDTH, HEIGHT);
		if(SDL_PollEvent(&event)) {
			switch (event.type)
			{
				case SDL_KEYUP:
					if (event.key.keysym.sym == SDLK_ESCAPE) {
						return 0;
					}
					break;
				case SDL_QUIT:
					return 0;
			}
		}

		lastmousebtn = SDL_GetMouseState(&lastmousex, &lastmousey);
	}
}

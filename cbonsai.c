#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <curses.h>
#include <locale.h>
#include <panel.h>
#include <getopt.h>
#include <time.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <ctype.h>
#include <unistd.h>

#define COLOR_WOOD_BRIGHT COLOR_PAIR(4)
#define COLOR_WOOD_DARK COLOR_PAIR(2)
#define COLOR_LEAF_BRIGHT COLOR_PAIR(3)
#define COLOR_LEAF_DARK COLOR_PAIR(1)
#define COLOR_TEXT COLOR_PAIR(5)

enum branchType {trunk, shootLeft, shootRight, dying, dead};
enum growthResult {growthComplete, growthResize};
enum inputResult {inputContinue, inputQuit, inputResize};

volatile sig_atomic_t resizePending = 0;

void markResize(int signalNumber) {
	(void) signalNumber;
	resizePending = 1;
}

struct config {
	int live;
	int infinite;
	int screensaver;
	int printTree;
	int verbosity;
	int lifeStart;
	int multiplier;
	int baseType;
	int baseWidth;
	int baseHeight;
	int baseOriginX;
	int baseOriginY;
	int automaticBasePosition;
	int growthHeight;
	int textOriginX;
	int textOriginY;
	int seed;
	int leavesSize;
	int save;
	int load;
	int targetBranchCount;

	double timeWait;
	float timeStep;

	char* message;
	char* text;
	char* leaves[64];
	int colors[4];
	char* saveFile;
	char* loadFile;
};

struct ncursesObjects {
	WINDOW* baseWin;
	WINDOW* treeWin;
	WINDOW* messageBorderWin;
	WINDOW* messageWin;

	PANEL* basePanel;
	PANEL* treePanel;
	PANEL* messageBorderPanel;
	PANEL* messagePanel;
};

struct counters {
	int branches;
	int shoots;
	int shootCounter;
	int growthSteps;
	int replayUntil;
};

void delObjects(struct ncursesObjects *objects) {
	if (!objects) return;

	// Delete panels before their windows. ncurses accepts NULL in some
	// implementations, but that is not portable across curses libraries.
	if (objects->basePanel) {
		del_panel(objects->basePanel);
		objects->basePanel = NULL;
	}
	if (objects->treePanel) {
		del_panel(objects->treePanel);
		objects->treePanel = NULL;
	}
	if (objects->messageBorderPanel) {
		del_panel(objects->messageBorderPanel);
		objects->messageBorderPanel = NULL;
	}
	if (objects->messagePanel) {
		del_panel(objects->messagePanel);
		objects->messagePanel = NULL;
	}

	if (objects->baseWin) {
		delwin(objects->baseWin);
		objects->baseWin = NULL;
	}
	if (objects->treeWin) {
		delwin(objects->treeWin);
		objects->treeWin = NULL;
	}
	if (objects->messageBorderWin) {
		delwin(objects->messageBorderWin);
		objects->messageBorderWin = NULL;
	}
	if (objects->messageWin) {
		delwin(objects->messageWin);
		objects->messageWin = NULL;
	}
}

void quit(struct config *conf, struct ncursesObjects *objects, int returnCode) {
	delObjects(objects);
	free(conf->saveFile);
	free(conf->loadFile);
	free(conf->text);
	exit(returnCode);
}

int saveToFile(char* fname, int seed, int branchCount) {
	FILE *fp = fopen(fname, "w");

	if (!fp) {
		printf("error: file was not opened properly for writing: %s\n", fname);
		return 1;
	}

	fprintf(fp, "%d %d", seed, branchCount);
	fclose(fp);

	return 0;
}

// load seed and counter from file
int loadFromFile(struct config *conf) {
	FILE* fp = fopen(conf->loadFile, "r");

	if (!fp) {
		printf("error: file was not opened properly for reading: %s\n", conf->loadFile);
		return 1;
	}

	int seed, targetBranchCount;
	if (fscanf(fp, "%i %i", &seed, &targetBranchCount) != 2) {
		printf("error: save file could not be read\n");
		return 1;
	}

	conf->seed = seed;
	conf->targetBranchCount = targetBranchCount;

	fclose(fp);

	return 0;
}

void finish(const struct config *conf, struct counters *myCounters) {
	refresh();
	endwin();	// delete ncurses screen
	if (conf->save)
		saveToFile(conf->saveFile, conf->seed, myCounters->branches);
}

void printHelp(void) {
	printf("%s",
		"Usage: cbonsai [OPTION]...\n"
	        "\n"
	        "cbonsai is a beautifully random bonsai tree generator.\n"
	        "\n"
	        "Options:\n"
	        "  -l, --live             live mode: show each step of growth\n"
	        "  -t, --time=TIME        in live mode, wait TIME secs between\n"
	        "                           steps of growth (must be larger than 0) [default: 0.03]\n"
	        "  -i, --infinite         infinite mode: keep growing trees\n"
	        "  -w, --wait=TIME        in infinite mode, wait TIME between each tree\n"
	        "                           generation [default: 4.00]\n"
	        "  -S, --screensaver      screensaver mode; equivalent to -li and\n"
	        "                           quit on any keypress\n"
	        "  -m, --message=STR      attach message next to the tree\n"
        "  -T, --text-origin=ROW,COL\n"
        "                           display text from stdin at row ROW, column COL\n"
        "  -g, --geometry=ROW,COL\n"
        "                           place the plant base at row ROW, column COL\n"
	        "  -b, --base=INT         ascii-art plant base to use, 0 is none\n"
	        "  -c, --leaf=LIST        list of comma-delimited strings randomly chosen\n"
	        "                           for leaves [default: &]\n"
	        "  -k, --color=LIST       list of 4 comma-delimited color indices (0-255) for\n"
	        "                           each of dark leaves, dark wood, light leaves, and\n"
	        "                           light wood, in that order [default: 2,3,10,11]\n"
	        "  -M, --multiplier=INT   branch multiplier; higher -> more\n"
	        "                           branching (0-20) [default: 5]\n"
	        "  -L, --life=INT         life; higher -> more growth (0-200) [default: 32]\n"
	        "  -p, --print            print tree to terminal when finished\n"
	        "  -s, --seed=INT         seed random number generator\n"
	        "  -W, --save=FILE        save progress to file [default: $XDG_CACHE_HOME/cbonsai or $HOME/.cache/cbonsai]\n"
	        "  -C, --load=FILE        load progress from file [default: $XDG_CACHE_HOME/cbonsai]\n"
	        "  -v, --verbose          increase output verbosity\n"
	        "  -h, --help             show help\n"
    );
}

void drawBase(WINDOW* baseWin, int baseType) {
	// draw base art
	switch(baseType) {
	case 1:
		wattron(baseWin, A_BOLD | COLOR_TEXT);
		wprintw(baseWin, "%s", ":");
		wattron(baseWin, COLOR_LEAF_BRIGHT);
		wprintw(baseWin, "%s", "___________");
		wattron(baseWin, COLOR_WOOD_BRIGHT);
		wprintw(baseWin, "%s", "./~~~\\.");
		wattron(baseWin, COLOR_LEAF_BRIGHT);
		wprintw(baseWin, "%s", "___________");
		wattron(baseWin, COLOR_TEXT);
		wprintw(baseWin, "%s", ":");

		mvwprintw(baseWin, 1, 0, "%s", " \\                           / ");
		mvwprintw(baseWin, 2, 0, "%s", "  \\_________________________/ ");
		mvwprintw(baseWin, 3, 0, "%s", "  (_)                     (_)");

		wattroff(baseWin, A_BOLD);
		break;
	case 2:
		wattron(baseWin, COLOR_TEXT);
		wprintw(baseWin, "%s", "(");
		wattron(baseWin, COLOR_LEAF_BRIGHT);
		wprintw(baseWin, "%s", "---");
		wattron(baseWin, COLOR_WOOD_BRIGHT);
		wprintw(baseWin, "%s", "./~~~\\.");
		wattron(baseWin, COLOR_LEAF_BRIGHT);
		wprintw(baseWin, "%s", "---");
		wattron(baseWin, COLOR_TEXT);
		wprintw(baseWin, "%s", ")");

		mvwprintw(baseWin, 1, 0, "%s", " (           ) ");
		mvwprintw(baseWin, 2, 0, "%s", "  (_________)  ");
		break;
	}
}

void drawWins(const struct config *conf, struct ncursesObjects *objects) {
	// clean up old objects
	delObjects(objects);
	// create windows
	objects->baseWin = newwin(conf->baseHeight, conf->baseWidth, conf->baseOriginY, conf->baseOriginX);
	objects->treeWin = newwin(conf->baseOriginY, getmaxx(stdscr), 0, 0);

	// create tree and base panels
	objects->basePanel = new_panel(objects->baseWin);
	objects->treePanel = new_panel(objects->treeWin);

	drawBase(objects->baseWin, conf->baseType);
}

// roll (randomize) a given die
void roll(int *dice, int mod) { *dice = rand() % mod; }

int takeResizeEvent(int key) {
	if (resizePending
#ifdef KEY_RESIZE
			|| key == KEY_RESIZE
#endif
	) {
		resizePending = 0;
		return 1;
	}
	return 0;
}

// Check for key presses and terminal resizes without handling either here.
int checkKeyPress(const struct config *conf) {
	int key = wgetch(stdscr);
	if (takeResizeEvent(key)) return inputResize;
	if ((conf->screensaver && key != ERR) || key == 'q') return inputQuit;
	return inputContinue;
}

// display changes
void updateScreen(float timeStep) {
	update_panels();
	doupdate();

	// convert given time into seconds and nanoseconds and sleep
	struct timespec ts;
	ts.tv_sec = timeStep / 1;
	ts.tv_nsec = (timeStep - ts.tv_sec) * 1000000000;
	nanosleep(&ts, NULL);	// sleep for given time
}

// based on type of tree, determine what color a branch should be
void chooseColor(enum branchType type, WINDOW* treeWin) {
	switch(type) {
	case trunk:
	case shootLeft:
	case shootRight:
		if (rand() % 2 == 0) wattron(treeWin, A_BOLD | COLOR_WOOD_BRIGHT);
		else wattron(treeWin, COLOR_WOOD_DARK);
		break;

	case dying:
		if (rand() % 10 == 0) wattron(treeWin, A_BOLD | COLOR_LEAF_BRIGHT);
		else wattron(treeWin, COLOR_LEAF_BRIGHT);
		break;

	case dead:
		if (rand() % 3 == 0) wattron(treeWin, A_BOLD | COLOR_LEAF_DARK);
		else wattron(treeWin, COLOR_LEAF_DARK);
		break;
	}
}

// determine change in X and Y coordinates of a given branch
void setDeltas(enum branchType type, int life, int age, int multiplier, int *returnDx, int *returnDy) {
	int dx = 0;
	int dy = 0;
	int dice;
	switch (type) {
	case trunk: // trunk

		// new or dead trunk
		if (age <= 2 || life < 4) {
			dy = 0;
			dx = (rand() % 3) - 1;
		}
		// young trunk should grow wide
		else if (age < (multiplier * 3)) {

			// every (multiplier * 0.8) steps, raise tree to next level
			if (age % (int) (multiplier * 0.5) == 0) dy = -1;
			else dy = 0;

			roll(&dice, 10);
			if (dice >= 0 && dice <=0) dx = -2;
			else if (dice >= 1 && dice <= 3) dx = -1;
			else if (dice >= 4 && dice <= 5) dx = 0;
			else if (dice >= 6 && dice <= 8) dx = 1;
			else if (dice >= 9 && dice <= 9) dx = 2;
		}
		// middle-aged trunk
		else {
			roll(&dice, 10);
			if (dice > 2) dy = -1;
			else dy = 0;
			dx = (rand() % 3) - 1;
		}
		break;

	case 1: // left shoot: trend left and little vertical movement
		roll(&dice, 10);
		if (dice >= 0 && dice <= 1) dy = -1;
		else if (dice >= 2 && dice <= 7) dy = 0;
		else if (dice >= 8 && dice <= 9) dy = 1;

		roll(&dice, 10);
		if (dice >= 0 && dice <=1) dx = -2;
		else if (dice >= 2 && dice <= 5) dx = -1;
		else if (dice >= 6 && dice <= 8) dx = 0;
		else if (dice >= 9 && dice <= 9) dx = 1;
		break;

	case 2: // right shoot: trend right and little vertical movement
		roll(&dice, 10);
		if (dice >= 0 && dice <= 1) dy = -1;
		else if (dice >= 2 && dice <= 7) dy = 0;
		else if (dice >= 8 && dice <= 9) dy = 1;

		roll(&dice, 10);
		if (dice >= 0 && dice <=1) dx = 2;
		else if (dice >= 2 && dice <= 5) dx = 1;
		else if (dice >= 6 && dice <= 8) dx = 0;
		else if (dice >= 9 && dice <= 9) dx = -1;
		break;

	case 3: // dying: discourage vertical growth(?); trend left/right (-3,3)
		roll(&dice, 10);
		if (dice >= 0 && dice <=1) dy = -1;
		else if (dice >= 2 && dice <=8) dy = 0;
		else if (dice >= 9 && dice <=9) dy = 1;

		roll(&dice, 15);
		if (dice >= 0 && dice <=0) dx = -3;
		else if (dice >= 1 && dice <= 2) dx = -2;
		else if (dice >= 3 && dice <= 5) dx = -1;
		else if (dice >= 6 && dice <= 8) dx = 0;
		else if (dice >= 9 && dice <= 11) dx = 1;
		else if (dice >= 12 && dice <= 13) dx = 2;
		else if (dice >= 14 && dice <= 14) dx = 3;
		break;

	case 4: // dead: fill in surrounding area
		roll(&dice, 10);
		if (dice >= 0 && dice <= 2) dy = -1;
		else if (dice >= 3 && dice <= 6) dy = 0;
		else if (dice >= 7 && dice <= 9) dy = 1;
		dx = (rand() % 3) - 1;
		break;
	}

	*returnDx = dx;
	*returnDy = dy;
}

char* chooseString(const struct config *conf, enum branchType type, int life, int dx, int dy) {
	char* branchStr;

	const unsigned int maxStrLen = 32;

	branchStr = malloc(maxStrLen);
	strcpy(branchStr, "?");	// fallback character

	if (life < 4) type = dying;

	switch(type) {
	case trunk:
		if (dy == 0) strcpy(branchStr, "/~");
		else if (dx < 0) strcpy(branchStr, "\\|");
		else if (dx == 0) strcpy(branchStr, "/|\\");
		else if (dx > 0) strcpy(branchStr, "|/");
		break;
	case shootLeft:
		if (dy > 0) strcpy(branchStr, "\\");
		else if (dy == 0) strcpy(branchStr, "\\_");
		else if (dx < 0) strcpy(branchStr, "\\|");
		else if (dx == 0) strcpy(branchStr, "/|");
		else if (dx > 0) strcpy(branchStr, "/");
		break;
	case shootRight:
		if (dy > 0) strcpy(branchStr, "/");
		else if (dy == 0) strcpy(branchStr, "_/");
		else if (dx < 0) strcpy(branchStr, "\\|");
		else if (dx == 0) strcpy(branchStr, "/|");
		else if (dx > 0) strcpy(branchStr, "/");
		break;
	case dying:
	case dead:
		strncpy(branchStr, conf->leaves[rand() % conf->leavesSize], maxStrLen - 1);
		branchStr[maxStrLen - 1] = '\0';
	}

	return branchStr;
}

int branch(struct config *conf, struct ncursesObjects *objects, struct counters *myCounters,
		int y, int x, enum branchType type, int life) {
	myCounters->branches++;
	int dx = 0;
	int dy = 0;
	int age = 0;
	int shootCooldown = conf->multiplier;

	while (life > 0) {
		int input = checkKeyPress(conf);
		if (input == inputQuit) {
			finish(conf, myCounters);
			quit(conf, objects, 0);
		}
		if (input == inputResize) return growthResize;

		life--;		// decrement remaining life counter
		age = conf->lifeStart - life;

		setDeltas(type, life, age, conf->multiplier, &dx, &dy);

		// Use the original tree height during replay. A resize should relocate
		// the tree, not change the random growth decisions already made.
		if (dy > 0 && y > (conf->growthHeight - 2)) dy--;

		// near-dead branch should branch into a lot of leaves
		if (life < 3) {
			if (branch(conf, objects, myCounters, y, x, dead, life) == growthResize)
				return growthResize;
		}

		// dying trunk should branch into a lot of leaves
		else if (type == trunk && life < (conf->multiplier + 2)) {
			if (branch(conf, objects, myCounters, y, x, dying, life) == growthResize)
				return growthResize;
		}

		// dying shoot should branch into a lot of leaves
		else if ((type == shootLeft || type == shootRight) && life < (conf->multiplier + 2)) {
			if (branch(conf, objects, myCounters, y, x, dying, life) == growthResize)
				return growthResize;
		}

		// trunks should re-branch if either randomly, or upon every multiplier steps
		else if (type == trunk && (((rand() % 3) == 0) || (life % conf->multiplier == 0))) {

			// if trunk is branching and not about to die, create another trunk with random life
			if ((rand() % 8 == 0) && life > 7) {
				shootCooldown = conf->multiplier * 2;
				if (branch(conf, objects, myCounters, y, x, trunk, life + (rand() % 5 - 2)) == growthResize)
					return growthResize;
			}

			// otherwise create a shoot
			else if (shootCooldown <= 0) {
				shootCooldown = conf->multiplier * 2;

				int shootLife = life + conf->multiplier;
				myCounters->shoots++;
				myCounters->shootCounter++;
				if (conf->verbosity) mvwprintw(objects->treeWin, 4, 5, "shoots: %02d", myCounters->shoots);

				if (branch(conf, objects, myCounters, y, x,
						(myCounters->shootCounter % 2) + 1, shootLife) == growthResize)
					return growthResize;
			}
		}
		shootCooldown--;

		if (conf->verbosity > 0) {
			mvwprintw(objects->treeWin, 5, 5, "dx: %02d", dx);
			mvwprintw(objects->treeWin, 6, 5, "dy: %02d", dy);
			mvwprintw(objects->treeWin, 7, 5, "type: %d", type);
			mvwprintw(objects->treeWin, 8, 5, "shootCooldown: % 3d", shootCooldown);
		}

		x += dx;
		y += dy;

		chooseColor(type, objects->treeWin);
		char *branchStr = chooseString(conf, type, life, dx, dy);

		wchar_t wc = 0;
		mbrtowc(&wc, branchStr, 32, NULL);
		int width = wcwidth(wc);
		if (width > 0 && x % width == 0)
			mvwprintw(objects->treeWin, y, x, "%s", branchStr);

		wattroff(objects->treeWin, A_BOLD);
		free(branchStr);

		myCounters->growthSteps++;
		if (conf->live && myCounters->growthSteps >= myCounters->replayUntil &&
				!(conf->load && myCounters->branches < conf->targetBranchCount))
			updateScreen(conf->timeStep);
	}
	return growthComplete;
}

struct messageLine {
	wchar_t *characters;
	size_t length;
	size_t capacity;
	int width;
};

struct messageLayout {
	wchar_t **lines;
	size_t count;
	size_t capacity;
	int width;
};

void freeMessageLayout(struct messageLayout *layout) {
	for (size_t i = 0; i < layout->count; i++)
		free(layout->lines[i]);
	free(layout->lines);
}

int appendToMessageLine(struct messageLine *line, wchar_t character, int width) {
	if (line->length + 1 >= line->capacity) {
		size_t capacity = line->capacity ? line->capacity * 2 : 32;
		if (capacity > SIZE_MAX / sizeof(*line->characters)) return 1;

		wchar_t *characters = realloc(line->characters, capacity * sizeof(*characters));
		if (!characters) return 1;

		line->characters = characters;
		line->capacity = capacity;
	}

	line->characters[line->length++] = character;
	line->characters[line->length] = L'\0';
	line->width += width;
	return 0;
}

int addMessageLine(struct messageLayout *layout, struct messageLine *line) {
	if (layout->count == layout->capacity) {
		size_t capacity = layout->capacity ? layout->capacity * 2 : 8;
		if (capacity > SIZE_MAX / sizeof(*layout->lines)) return 1;

		wchar_t **lines = realloc(layout->lines, capacity * sizeof(*lines));
		if (!lines) return 1;

		layout->lines = lines;
		layout->capacity = capacity;
	}

	wchar_t *characters = malloc((line->length + 1) * sizeof(*characters));
	if (!characters) return 1;
	if (line->length) memcpy(characters, line->characters, line->length * sizeof(*characters));
	characters[line->length] = L'\0';

	layout->lines[layout->count++] = characters;
	if (line->width > layout->width) layout->width = line->width;
	line->length = 0;
	line->width = 0;
	return 0;
}

void normalizeMessageCharacter(wchar_t *character, int *width) {
	if (iswcntrl(*character) || (*width = wcwidth(*character)) < 0) {
		*character = L'?';
		*width = 1;
	}
}

int appendWrappedCharacter(struct messageLayout *layout, struct messageLine *line,
		wchar_t character, int maxWidth) {
	int width;
	normalizeMessageCharacter(&character, &width);
	if (width > maxWidth) {
		character = L'?';
		width = 1;
	}

	if (width > 0 && line->width > 0 && line->width + width > maxWidth) {
		if (addMessageLine(layout, line) != 0) return 1;
	}

	return appendToMessageLine(line, character, width);
}

int appendMessageWord(struct messageLayout *layout, struct messageLine *line,
		const struct messageLine *word, int addSpace, int maxWidth) {
	int wordWidth = 0;
	for (size_t i = 0; i < word->length; i++) {
		wchar_t character = word->characters[i];
		int width;
		normalizeMessageCharacter(&character, &width);
		if (width > maxWidth) width = 1;
		wordWidth += width;
	}

	if (addSpace && line->length) {
		if (line->width + 1 + wordWidth > maxWidth) {
			if (addMessageLine(layout, line) != 0) return 1;
		} else if (appendToMessageLine(line, L' ', 1) != 0) {
			return 1;
		}
	}

	for (size_t i = 0; i < word->length; i++) {
		if (appendWrappedCharacter(layout, line, word->characters[i], maxWidth) != 0)
			return 1;
	}
	return 0;
}

int flushMessageWord(struct messageLayout *layout, struct messageLine *line,
		struct messageLine *word, int *addSpace, int maxWidth) {
	if (!word->length) return 0;
	if (appendMessageWord(layout, line, word, *addSpace, maxWidth) != 0) return 1;

	word->length = 0;
	word->width = 0;
	*addSpace = 0;
	return 0;
}

// Wrap UTF-8 message text before creating windows so layout and rendering use
// the same display widths. Horizontal whitespace becomes one separator; newlines
// and blank lines are preserved.
int layoutMessage(const char *message, int maxWidth, struct messageLayout *layout) {
	struct messageLine line = {0};
	struct messageLine word = {0};
	mbstate_t state = {0};
	const char *cursor = message;
	int addSpace = 0;
	int previousWasCarriageReturn = 0;

	while (*cursor) {
		wchar_t character;
		size_t bytes = mbrtowc(&character, cursor, MB_CUR_MAX, &state);
		if (bytes == (size_t) -1 || bytes == (size_t) -2) {
			character = L'?';
			bytes = 1;
			memset(&state, 0, sizeof(state));
		} else if (bytes == 0) {
			break;
		}
		cursor += bytes;

		if (character == L'\r' || character == L'\n') {
			if (character == L'\n' && previousWasCarriageReturn) {
				previousWasCarriageReturn = 0;
				continue;
			}
			if (flushMessageWord(layout, &line, &word, &addSpace, maxWidth) != 0 ||
					addMessageLine(layout, &line) != 0)
				goto error;
			previousWasCarriageReturn = character == L'\r';
			addSpace = 0;
			continue;
		}
		previousWasCarriageReturn = 0;

		if (iswspace(character)) {
			if (flushMessageWord(layout, &line, &word, &addSpace, maxWidth) != 0)
				goto error;
			addSpace = line.length > 0;
			continue;
		}

		int width = wcwidth(character);
		if (appendToMessageLine(&word, character, width > 0 ? width : 0) != 0)
			goto error;
	}

	if (flushMessageWord(layout, &line, &word, &addSpace, maxWidth) != 0)
		goto error;
	if (line.length || !layout->count) {
		if (addMessageLine(layout, &line) != 0) goto error;
	}

	free(line.characters);
	free(word.characters);
	return 0;

error:
	free(line.characters);
	free(word.characters);
	freeMessageLayout(layout);
	memset(layout, 0, sizeof(*layout));
	return 1;
}

// Create ncurses windows to contain the already-laid-out message and its box.
int createMessageWindows(struct ncursesObjects *objects, int contentWidth, int contentHeight) {
	int maxY, maxX;
	getmaxyx(stdscr, maxY, maxX);

	// A border needs two columns on each side and two rows for its top and
	// bottom. Refuse to create it if the terminal cannot contain one.
	if (maxX < 5 || maxY < 3) {
		fprintf(stderr, "error: terminal is too small to display the message\n");
		return 1;
	}

	int borderWidth = contentWidth + 4;
	int borderHeight = contentHeight + 2;
	int borderY = (maxY * 7) / 10 - 1;
	int borderX = (maxX * 7) / 10 - 2;

	if (borderY < 0) borderY = 0;
	if (borderX < 0) borderX = 0;
	if (borderY + borderHeight > maxY) borderY = maxY - borderHeight;
	if (borderX + borderWidth > maxX) borderX = maxX - borderWidth;

	WINDOW* messageBorderWin = newwin(borderHeight, borderWidth, borderY, borderX);
	if (!messageBorderWin) {
		fprintf(stderr, "error: could not create the message border window\n");
		return 1;
	}

	// Keep one blank column between each border and the content window.
	WINDOW* messageWin = newwin(contentHeight, contentWidth, borderY + 1, borderX + 2);
	if (!messageWin) {
		delwin(messageBorderWin);
		fprintf(stderr, "error: could not create the message window\n");
		return 1;
	}

	// Draw the box before exposing the windows through the object structure.
	wattron(messageBorderWin, COLOR_TEXT | A_BOLD);
	wborder(messageBorderWin, '|', '|', '-', '-', '+', '+', '+', '+');

	PANEL* messageBorderPanel = new_panel(messageBorderWin);
	if (!messageBorderPanel) {
		delwin(messageWin);
		delwin(messageBorderWin);
		fprintf(stderr, "error: could not create the message border panel\n");
		return 1;
	}

	PANEL* messagePanel = new_panel(messageWin);
	if (!messagePanel) {
		del_panel(messageBorderPanel);
		delwin(messageWin);
		delwin(messageBorderWin);
		fprintf(stderr, "error: could not create the message panel\n");
		return 1;
	}

	objects->messageBorderWin = messageBorderWin;
	objects->messageWin = messageWin;
	objects->messageBorderPanel = messageBorderPanel;
	objects->messagePanel = messagePanel;
	return 0;
}

int drawMessage(const struct config *conf, struct ncursesObjects *objects, const char* message) {
	if (!message) return 1;

	int maxY, maxX;
	getmaxyx(stdscr, maxY, maxX);
	if (maxX < 5 || maxY < 3) {
		fprintf(stderr, "error: terminal is too small to display the message\n");
		return 1;
	}

	int maxContentWidth = (maxX - 4) / 4;
	if (maxContentWidth < 1) maxContentWidth = 1;

	struct messageLayout layout = {0};
	if (layoutMessage(message, maxContentWidth, &layout) != 0) {
		fprintf(stderr, "error: could not lay out the message\n");
		return 1;
	}

	int contentWidth = layout.width ? layout.width : 1;
	int contentHeight = layout.count > (size_t) (maxY - 2)
		? maxY - 2
		: (int) layout.count;
	if (createMessageWindows(objects, contentWidth, contentHeight) != 0) {
		freeMessageLayout(&layout);
		return 1;
	}

	for (int y = 0; y < contentHeight; y++)
		mvwaddnwstr(objects->messageWin, y, 0, layout.lines[y], -1);

	// Make clipping visible instead of silently dropping lines on short terminals.
	if (layout.count > (size_t) contentHeight) {
		const char *marker = contentWidth >= 3 ? "..." : ".";
		int markerX = contentWidth - (int) strlen(marker);
		mvwaddnstr(objects->messageWin, contentHeight - 1, markerX, marker, -1);
	}

	if (conf->verbosity >= 2)
		mvwprintw(objects->treeWin, 11, 5, "message lines: %zu", layout.count);

	freeMessageLayout(&layout);
	return 0;
}

void setBaseGeometry(struct config *conf) {
	int rows, cols;
	switch(conf->baseType) {
	case 1:
		conf->baseWidth = 31;
		conf->baseHeight = 4;
		break;
	case 2:
		conf->baseWidth = 15;
		conf->baseHeight = 3;
		break;
	default:
		conf->baseWidth = 0;
		conf->baseHeight = 0;
		break;
	}

	getmaxyx(stdscr, rows, cols);
	if (conf->automaticBasePosition) {
		conf->baseOriginY = rows - conf->baseHeight;
		conf->baseOriginX = (cols / 2) - (conf->baseWidth / 2);
	}
}

void init(struct config *conf, struct ncursesObjects *objects, int resizing) {
	if (resizing) {
		resizeterm(0, 0);
		erase();
	} else {
		savetty();	// save terminal settings
		initscr();	// init ncurses screen
		noecho();	// don't echo input to screen
		curs_set(0);	// make cursor invisible
		cbreak();	// don't wait for new line to grab user input
		nodelay(stdscr, TRUE);	// force getch to be a non-blocking call
		keypad(stdscr, TRUE);

		// if terminal has color capabilities, use them
		if (has_colors()) {
			start_color();

			// use native background color when possible
			int bg = COLOR_BLACK;
			if (use_default_colors() != ERR) bg = -1;

			// initialize color pairs
			int warnedAlready = 0;
			for(int id = 1; id <= 4; id++) {
				int color_id = conf->colors[id-1];

				// restrict color pallete in non-256color terminals
				if (COLORS < 256) {
					init_pair(id, color_id % 8, bg);
				} else {
					init_pair(id, color_id, bg);
				}

				if (!warnedAlready && COLORS < 256 && color_id >= 8) {
					endwin();
					printf("Warning: defaulting to 8-color support.\n");
					warnedAlready = 1;
					doupdate();
				}
			}

			// initialize COLOR_TEXT pair
			if (COLORS < 256) {
				init_pair(5, 7, bg);
			}
			else {
				init_pair(5, 8, bg);
			}
		} else {
			printf("%s", "Warning: terminal does not have color support.\n");
		}
	}

	nodelay(stdscr, TRUE);
	keypad(stdscr, TRUE);
	setBaseGeometry(conf);
	if (!resizing) conf->growthHeight = conf->baseOriginY;

	// define and draw windows, then create panels
	drawWins(conf, objects);
	drawMessage(conf, objects, conf->message);

	// If -T ROW,COL is specified then display the text in the windows
	if (conf->textOriginY >= 0 && conf->textOriginX >= 0) {
		mvprintw(conf->textOriginY, conf->textOriginX, "%s", conf->text);
		mvwprintw(objects->treeWin, conf->textOriginY, conf->textOriginX, "%s", conf->text);
	}
}

int growTree(struct config *conf, struct ncursesObjects *objects, struct counters *myCounters,
		int replayUntil) {
	int maxY, maxX;
	getmaxyx(objects->treeWin, maxY, maxX);

	// Reset deterministic state before an initial draw or replay.
	myCounters->shoots = 0;
	myCounters->branches = 0;
	myCounters->shootCounter = rand();
	myCounters->growthSteps = 0;
	myCounters->replayUntil = replayUntil;

	if (conf->verbosity > 0) {
		mvwprintw(objects->treeWin, 2, 5, "maxX: %03d, maxY: %03d", maxX, maxY);
	}

	int result = branch(conf, objects, myCounters, conf->baseOriginY - 1,
			conf->baseOriginX + conf->baseWidth / 2, trunk, conf->lifeStart);
	if (result == growthResize) return result;

	update_panels();
	doupdate();
	return growthComplete;
}

// Replay the current tree after a resize. Re-seeding and replaying its completed
// growth steps reconstructs the same shape at the new window position.
void redrawCurrentTree(struct config *conf, struct ncursesObjects *objects,
		struct counters *myCounters) {
	int result = growthResize;
	int replayUntil = myCounters->growthSteps;
	while (result == growthResize) {
		srand(conf->seed);
		init(conf, objects, 1);
		result = growTree(conf, objects, myCounters, replayUntil);
		replayUntil = myCounters->growthSteps;
	}
}

// print stdscr to terminal window
void printstdscr(void) {
	int maxY, maxX;
	getmaxyx(stdscr, maxY, maxX);

	// loop through each character on stdscr
	for (int y = 0; y < maxY; y++) {
		for (int x = 0; x < maxX; x++) {
			// grab cchar_t from stdscr
			cchar_t c;
			mvwin_wch(stdscr, y, x, &c);

			// grab wchar_t from cchar_t
			wchar_t wch[128] = {0};
			attr_t attrs;
			short color_pair;
			getcchar(&c, wch, &attrs, &color_pair, 0);

			short fg;
			short bg;
			pair_content(color_pair, &fg, &bg);

			// enable bold if needed
			if(attrs & A_BOLD) printf("\033[1m");
			else printf("\033[0m");

			// enable correct color
			if (fg <= 0) printf("\033[0m");
			else if (fg >= 16) printf("\033[38;5;%him", fg);
			else if (fg <= 7) printf("\033[3%him", fg);
			else if (fg >= 8) printf("\033[9%him", fg - 8);

			// print wchar
			printf("%ls", wch);

			// if we're on the last char in the row, print a newline
			if (x == maxX - 1)
				printf("\n");

			short clen = wcslen(wch);
			short cwidth = 0;
			for (int i = 0; i < clen; ++i)
				cwidth += wcwidth(wch[i]);

			if (cwidth > 1)
				x += cwidth - 1;
		}
	}

	printf("\033[0m\n");
}

char* createDefaultCachePath(void) {
	char* result;
	size_t envlen;
	char* toAppend;

	// follow XDG Base Directory Specification for default cache file path
	const char* env_XDG_CACHE_HOME = getenv("XDG_CACHE_HOME");
	if (env_XDG_CACHE_HOME && (envlen = strlen(env_XDG_CACHE_HOME))) {
		toAppend = "/cbonsai";

		// create result buffer
		result = malloc(envlen + strlen(toAppend) + 1);
		strncpy(result, env_XDG_CACHE_HOME, envlen);
		strcpy(result + envlen, toAppend);
		return result;
	}

	// if we don't have $XDG_CACHE_HOME, try $HOME
	const char* env_HOME = getenv("HOME");
	if (env_HOME && (envlen = strlen(env_HOME))) {
		toAppend = "/.cache/cbonsai";

		// create result buffer
		result = malloc(envlen + strlen(toAppend) + 1);
		strncpy(result, env_HOME, envlen);
		strcpy(result + envlen, toAppend);
		return result;
	}

	// if we also don't have $HOME, just use ./cbonsai
	toAppend = "cbonsai";
	result = malloc(strlen(toAppend) + 1);
	strcpy(result, toAppend);
	return result;
}

/* Parse a string like "10,20" and set the values in the corresponding row and col args.
 * Returns 0 on errors and 1 on success.
 */
int parse_rowcol(const char *rowcol, int *row, int *col) {
	char *end;
	errno = 0;
	long parsedRow = strtol(rowcol, &end, 10);
	if (end == rowcol || *end != ',' || errno == ERANGE || parsedRow < 0 || parsedRow > INT_MAX)
		return 0;

	const char *column = end + 1;
	errno = 0;
	long parsedCol = strtol(column, &end, 10);
	if (end == column || *end != '\0' || errno == ERANGE || parsedCol < 0 || parsedCol > INT_MAX)
		return 0;

	*row = (int) parsedRow;
	*col = (int) parsedCol;
	return 1;
}

char *slurp_stdin(void) {
	size_t capacity = 2048;
	size_t length = 0;
	char *buf = malloc(capacity);
	if (!buf) return NULL;

	while (true) {
		if (length + 1 == capacity) {
			if (capacity > SIZE_MAX / 2) {
				free(buf);
				return NULL;
			}
			capacity *= 2;
			char *newBuf = realloc(buf, capacity);
			if (!newBuf) {
				free(buf);
				return NULL;
			}
			buf = newBuf;
		}

		size_t count = fread(buf + length, 1, capacity - length - 1, stdin);
		length += count;
		if (ferror(stdin)) {
			free(buf);
			return NULL;
		}
		if (feof(stdin)) {
			buf[length] = '\0';
			return buf;
		}
	}
}

int main(int argc, char* argv[]) {
	setlocale(LC_ALL, "");
	signal(SIGWINCH, markResize);

	struct config conf = {
		.live = 0,
		.infinite = 0,
		.screensaver = 0,
		.printTree = 0,
		.verbosity = 0,
		.lifeStart = 32,
		.multiplier = 5,
		.baseType = 1,
		.baseWidth = 0,
		.baseOriginY = -1,
		.baseOriginX = -1,
		.automaticBasePosition = 1,
		.growthHeight = 0,
		.textOriginY = -1,  // will be >= 0 if the -T option is used
		.textOriginX = -1,  // will be >= 0 if the -T option is used
		.seed = 0,
		.leavesSize = 0,
		.save = 0,
		.load = 0,
		.targetBranchCount = 0,

		.timeWait = 4,
		.timeStep = 0.03,

		.message = NULL,
		.text = NULL,
		.leaves = {0},
		.colors = {0, 0, 0, 0},
		.saveFile = createDefaultCachePath(),
		.loadFile = createDefaultCachePath(),
	};

	struct option long_options[] = {
		{"live", no_argument, NULL, 'l'},
		{"time", required_argument, NULL, 't'},
		{"infinite", no_argument, NULL, 'i'},
		{"wait", required_argument, NULL, 'w'},
		{"screensaver", no_argument, NULL, 'S'},
		{"message", required_argument, NULL, 'm'},
		{"text-origin", required_argument, NULL, 'T'},
		{"base", required_argument, NULL, 'b'},
		{"geometry", required_argument, NULL, 'g'},
		{"leaf", required_argument, NULL, 'c'},
		{"colors", required_argument, NULL, 'k'},
		{"multiplier", required_argument, NULL, 'M'},
		{"life", required_argument, NULL, 'L'},
		{"print", required_argument, NULL, 'p'},
		{"seed", required_argument, NULL, 's'},
		{"save", required_argument, NULL, 'W'},
		{"load", required_argument, NULL, 'C'},
		{"verbose", no_argument, NULL, 'v'},
		{"help", no_argument, NULL, 'h'},
		{0, 0, 0, 0}
	};

	struct ncursesObjects objects = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

	char leavesInput[128] = "&";
	char colorsInput[128] = "2,3,10,11";

	// parse arguments
	int option_index = 0;
	int c;
	while ((c = getopt_long(argc, argv, ":lt:iw:Sm:T:b:g:c:k:M:L:ps:C:W:vh", long_options, &option_index)) != -1) {
		switch (c) {
		case 'l':
			conf.live = 1;
			break;
		case 't':
			if (strtof(optarg, NULL) != 0) conf.timeStep = strtof(optarg, NULL);
			else {
				printf("error: invalid step time: '%s'\n", optarg);
				quit(&conf, &objects, 1);
			}
			if (conf.timeStep < 0) {
				printf("error: invalid step time: '%s'\n", optarg);
				quit(&conf, &objects, 1);
			}
			break;
		case 'i':
			conf.infinite = 1;
			break;
		case 'w':
			if (strtold(optarg, NULL) != 0) conf.timeWait = strtod(optarg, NULL);
			else {
				printf("error: invalid wait time: '%s'\n", optarg);
				quit(&conf, &objects, 1);
			}
			if (conf.timeWait < 0) {
				printf("error: invalid wait time: '%s'\n", optarg);
				quit(&conf, &objects, 1);
			}
			break;
		case 'S':
			conf.live = 1;
			conf.infinite = 1;

			conf.save = 1;
			conf.load = 1;

			conf.screensaver = 1;
			break;
		case 'm':
			conf.message = optarg;
			break;
		case 'T':
			if (!parse_rowcol(optarg, &conf.textOriginY, &conf.textOriginX)) {
				printf("error: failed parsing row,col: '%s'\n", optarg);
				quit(&conf, &objects, 1);
			}
			conf.text = slurp_stdin();
			if (conf.text == NULL) {
				printf("error: Failed reading text from stdin!\n");
				quit(&conf, &objects, 1);
			}
			if (!freopen(ctermid(NULL), "r", stdin)) {
				perror("error freopen process tty: ");
				quit(&conf, &objects, 1);
			}
			break;
		case 'b':
                        /* 0 can legitimately be returned, so we cannot check wether
                           strtold(optarg, NULL) != 0.  We need to set errno to zero
                           before the conversion attempt, and check it it changed
                           afterwards. */
                        errno = 0;
                        strtold(optarg, NULL);
                        if (!errno) conf.baseType = strtod(optarg, NULL);
			else {
				printf("error: invalid base index: '%s'\n", optarg);
				quit(&conf, &objects, 1);
			}
			break;
		case 'g':
			if (!parse_rowcol(optarg, &conf.baseOriginY, &conf.baseOriginX)) {
				printf("error: failed parsing row,col: '%s'\n", optarg);
				quit(&conf, &objects, 1);
			}
			conf.automaticBasePosition = 0;
			break;
		case 'c':
			strncpy(leavesInput, optarg, sizeof(leavesInput) - 1);
			leavesInput[sizeof(leavesInput) - 1] = '\0';
			break;
		case 'k':
			strncpy(colorsInput, optarg, sizeof(colorsInput) - 1);
			colorsInput[sizeof(colorsInput) - 1] = '\0';
			break;
		case 'M':
			if (strtold(optarg, NULL) != 0) conf.multiplier = strtod(optarg, NULL);
			else {
				printf("error: invalid multiplier: '%s'\n", optarg);
				quit(&conf, &objects, 1);
			}
			if (conf.multiplier < 0) {
				printf("error: invalid multiplier: '%s'\n", optarg);
				quit(&conf, &objects, 1);
			}
			break;
		case 'L':
			if (strtold(optarg, NULL) != 0) conf.lifeStart = strtod(optarg, NULL);
			else {
				printf("error: invalid initial life: '%s'\n", optarg);
				quit(&conf, &objects, 1);
			}
			if (conf.lifeStart < 0) {
				printf("error: invalid initial life: '%s'\n", optarg);
				quit(&conf, &objects, 1);
			}
			break;
		case 'p':
			conf.printTree = 1;
			break;
		case 's':
			if (strtold(optarg, NULL) != 0) conf.seed = strtod(optarg, NULL);
			else {
				printf("error: invalid seed: '%s'\n", optarg);
				quit(&conf, &objects, 1);
			}
			if (conf.seed < 0) {
				printf("error: invalid seed: '%s'\n", optarg);
				quit(&conf, &objects, 1);
			}
			break;
		case 'W':
			// skip argument if it's actually an option
			if (optarg[0] == '-') optind -= 1;
			else {
				free(conf.saveFile);
				size_t bufsize = strlen(optarg) + 1;
				conf.saveFile = malloc(bufsize);
				strncpy(conf.saveFile, optarg, bufsize - 1);
				conf.saveFile[bufsize - 1] = '\0';
			}

			conf.save = 1;
			break;
		case 'C':
			// skip argument if it's actually an option
			if (optarg[0] == '-') optind -= 1;
			else {
				free(conf.loadFile);
				size_t bufsize = strlen(optarg) + 1;
				conf.loadFile = malloc(bufsize);
				strncpy(conf.loadFile, optarg, bufsize - 1);
				conf.loadFile[bufsize - 1] = '\0';
			}

			conf.load = 1;
			break;
		case 'v':
			conf.verbosity++;
			break;

		// option has required argument, but it was not given
		case ':':
			switch (optopt) {
			case 'W':
				conf.save = 1;
				break;
			case 'C':
				conf.load = 1;
				break;
			default:
				printf("error: option requires an argument -- '%c'\n", optopt);
				printHelp();
				return 0;
				break;
			}
			break;

		// invalid option was given
		case '?':
			printf("error: invalid option -- '%c'\n", optopt);
			printHelp();
			return 0;
			break;

		case 'h':
			printHelp();
			return 0;
			break;
		}
	}

	// delimit leaves on "," and add each token to the leaves[] list
	char *token = strtok(leavesInput, ",");
	while (token != NULL) {
		if (conf.leavesSize < 100) conf.leaves[conf.leavesSize] = token;
		token = strtok(NULL, ",");
		conf.leavesSize++;
	}

	// delimit colors on "," and add each color to the colors[] list
	token = strtok(colorsInput, ",");
	for(int i = 0; i<4; i++) {
		if (token == NULL) {
			printf("error: too few color indices provided\n");
			quit(&conf, &objects, 1);
		}
		char* temp;
		errno = 0;

		// parse each number
		long parsed = strtol(token, &temp, 0);
		if (temp == token || *temp != '\0' || errno == ERANGE || parsed < 0 || parsed >= 256) {
			printf("error: invalid color index: '%s'\n", token);
			quit(&conf, &objects, 1);
		} else {
			// add to list and continue
			conf.colors[i] = parsed;
			token = strtok(NULL, ",");
		}
	}
	if (token != NULL) {
		printf("error: too many color indices provided\n");
		quit(&conf, &objects, 1);
	}

	if (conf.load)
		loadFromFile(&conf);

	// seed random number generator
	if (conf.seed == 0) conf.seed = time(NULL);
	srand(conf.seed);

	struct counters myCounters;

	do {
		init(&conf, &objects, 0);
		int result = growTree(&conf, &objects, &myCounters, 0);
		if (result == growthResize) redrawCurrentTree(&conf, &objects, &myCounters);
		if (conf.load) conf.targetBranchCount = 0;

		if (conf.infinite) {
			// Keep the completed tree visible until the timer expires. A resize
			// during this pause redraws the same completed tree and restarts the wait.
			while (true) {
				timeout(conf.timeWait * 1000);
				int input = checkKeyPress(&conf);
				if (input == inputQuit) {
					finish(&conf, &myCounters);
					quit(&conf, &objects, 0);
				}
				if (input == inputResize) {
					redrawCurrentTree(&conf, &objects, &myCounters);
					continue;
				}
				break;
			}

			// Seed the next tree independently after the current one is complete.
			srand(time(NULL));
		}
	} while (conf.infinite);

	if (conf.printTree) {
		finish(&conf, &myCounters);

		// overlay all windows onto stdscr
		overlay(objects.baseWin, stdscr);
		overlay(objects.treeWin, stdscr);
		if (objects.messageBorderWin)
			overwrite(objects.messageBorderWin, stdscr);
		if (objects.messageWin)
			overwrite(objects.messageWin, stdscr);

		printstdscr();
	} else {
		// Static mode waits for any key, but preserves the tree across resize.
		nodelay(objects.treeWin, FALSE);
		keypad(objects.treeWin, TRUE);
		while (true) {
			int key = wgetch(objects.treeWin);
			if (takeResizeEvent(key)) {
				redrawCurrentTree(&conf, &objects, &myCounters);
				nodelay(objects.treeWin, FALSE);
				keypad(objects.treeWin, TRUE);
				continue;
			}
			finish(&conf, &myCounters);
			break;
		}
	}

	quit(&conf, &objects, 0);
}

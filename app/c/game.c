#include <stdio.h>

#include "include/conio.h"

/* strings */
const int CLEAR[] = { '\033', '[', 'H', '\033', '[', 'J', '\0' };
const int CENTER[] = { '\033', '[', 'H', '\033', '[', 'J', '\0' };
const int NEWLINE[] = { '\r', '\n', '\0' };
const int TRY_AGAIN[] = {
	'T', 'r', 'y', ' ', 'a', 'g', 'a', 'i', 'n', '!', '\0'
};
const int YOU_WIN[] = { 'Y', 'o', 'u', ' ', 'w', 'i', 'n', '!', '\0'};

/* constants */
const int SCREEN_W = 80;
const int SCREEN_H = 24;
const int PAD_W = 16;
const int PAD_Y = 20;
const int PAD_STEP = 5;
const int PAD_STEP_LARGE = 11;
const int BRICK_W = 8;
const int BRICK_H = 3;
const int BRICK_COLS = 8;
const int BRICK_ROWS = 2;

/* enum status_t */
const int STATUS_PLAY = 0;
const int STATUS_QUIT = 1;
const int STATUS_WIN = 2;
const int STATUS_LOSE = 3;

/* object states */
int ball_v_y, ball_v_x;
int ball_x, ball_y;
int pad_center = 40;
int hit_brick_i, hit_brick_j;
int hit_pad_offs;
// keep in mind that this array should be changed according to `BRICK_COLS` and
// `BRICK_ROWS`, since we don't have a preprocessor nor an enum in SysY and thus
// cannot use real "constant"s like a #define or an enum item.
int brick_broken[8][2] = {};

/* gameplay states */
int score = 0;
int status = STATUS_PLAY;
int life = 2;
int key = '\0';

/* utilities */
void print(const int string[])
{
	int i = 0;
	while (string[i]) {
		putchar(string[i]);
		i = i + 1;
	}
}

void println(const int string[])
{
	print(string);
	print(NEWLINE);
}

void pad_move_left(int step)
{
	pad_center = pad_center - step;
	if (pad_center < (PAD_W / 2))
		pad_center = PAD_W / 2;
}

void pad_move_right(int step)
{
	pad_center = pad_center + step;
	if (pad_center > SCREEN_W - 1 - (PAD_W / 2))
		pad_center = SCREEN_W - 1 - (PAD_W / 2);
}

int clamp(int val, int min, int max)
{
	if (val < min)
		return min;
	if (val > max)
		return max;
	return val;
}

int in_pad(int x, int y)
{
	hit_pad_offs = x - pad_center;

	return (x >= pad_center - (PAD_W / 2) &&
		x <= pad_center + (PAD_W / 2) &&
		y == PAD_Y);
}

int in_brick(int x, int y)
{
	hit_brick_i = clamp(x / BRICK_W - 1, 0, BRICK_COLS - 1);
	hit_brick_j = clamp(y / BRICK_H - 1, 0, BRICK_ROWS - 1);

	return (!brick_broken[hit_brick_i][hit_brick_j] &&
		x >= BRICK_W * (hit_brick_i + 1) &&
		x < BRICK_W * (hit_brick_i + 1 + 1) - 1 &&
		y == BRICK_H * (hit_brick_j + 1));
}

/* procedures */
void reset_ball()
{
	ball_v_y = -1;
	ball_v_x = 2;
	ball_x = 40;
	ball_y = 19;
}

void ball_hit()
{
	int hit = 0;
	/* left wall */
	if (ball_x < 0) {
		ball_v_x = -ball_v_x;
		ball_x = -ball_x;
		hit = 1;
	}
	/* right wall */
	if (ball_x > SCREEN_W - 1) {
		ball_v_x = -ball_v_x;
		ball_x = 2 * (SCREEN_W - 1) - ball_x;
		hit = 1;
	}
	/* ceil */
	if (ball_y < 0) {
		ball_v_y = -ball_v_y;
		ball_y = -ball_y;
		hit = 1;
	}
	/* ball_hit the pad */
	if (in_pad(ball_x, ball_y)) {
		ball_v_y = -ball_v_y;
		ball_v_x = hit_pad_offs >> 1;
		hit = 1;
	}
	/* ball_hit the brick */
	if (in_brick(ball_x, ball_y)) {
		ball_v_y = -ball_v_y;
		brick_broken[hit_brick_i][hit_brick_j] = 1;

		score = score + 1;
		if (score == BRICK_COLS * BRICK_ROWS)
			status = STATUS_WIN;
		hit = 1;
	}
	if (hit)
		putchar('\a');
}

void tick()
{
	ball_x = ball_x + ball_v_x;
	ball_y = ball_y + ball_v_y;

	ball_hit();

	if (ball_y > SCREEN_H) {
		life = life - 1;

		if (!life)
			status = STATUS_LOSE;
		else
			reset_ball();
	}
}

void draw()
{
	print(CLEAR);

	int y = 0;
	while (y < SCREEN_H) {
		int x = 0;
		while (x < SCREEN_W) {
			if (x == SCREEN_W / 2 && y == SCREEN_H - 1)
				putchar('0' + life);
			else if (x == 0 && y == 0 ||
				 x == SCREEN_W - 1 && y == SCREEN_H - 1)
				putchar('/');
			else if (x == SCREEN_W - 1 && y == 0 ||
				 x == 0 && y == SCREEN_H - 1)
				putchar('\\');
			else if (x == ball_x && y == ball_y)
				putchar('*');
			else if (in_pad(x, y))
				putchar('=');
			else if (in_brick(x, y))
				putchar('/');
			else if (y == 0 && x != SCREEN_W - 1)
				putchar('-');
			else if (x == 0 || x == SCREEN_W - 1)
				putchar('|');
			else
				putchar(' ');
			x = x + 1;
		}
		/* raw mode, CRLF for newline */
		print(NEWLINE);
		y = y + 1;
	}
}

void control()
{
	if (kbhit())
		key = getch();

	if (key == 'h')
		pad_move_left(PAD_STEP);
	else if (key == 'l')
		pad_move_right(PAD_STEP);
	else if (key == 'H')
		pad_move_left(PAD_STEP_LARGE);
	else if (key == 'L')
		pad_move_right(PAD_STEP_LARGE);
	else if (key == 'q')
		status = STATUS_QUIT;

	key = 0;
}

int main()
{
	set_conio_terminal_mode();

	reset_ball();

	while (status == STATUS_PLAY) {
		control();
		tick();
		draw();

		usleep(50000);
	}

	print(CLEAR);
	if (status == STATUS_WIN)
		println(YOU_WIN);
	else if (status == STATUS_LOSE)
       		println(TRY_AGAIN);

	return 0;
}

/* 0xb -- number game for the rad1o

   (c) Neels Hofmeyr <neels@hofmeyr.de>
   Published under the GNU General Public License v2.

   This game is based on the idea of 2048, but uses hexadecimal digits.
   https://github.com/gabrielecirulli/2048
 */
#include <stdlib.h>

#include <r0ketlib/display.h>
#include <r0ketlib/print.h>
#include <r0ketlib/fonts.h>
#include <r0ketlib/keyin.h>

#include "usetable.h"

typedef unsigned int uint;


// This font list is kind of stupid. Patches welcome.
#define N_FONTS 7
const char*const font_list[N_FONTS] = {
    "marker18.f0n",
    "orbit14.f0n",
    "pt18.f0n",
    "ptone18.f0n",
    "soviet18.f0n",
    "ubuntu18.f0n",
    "-"
};

typedef struct {
  uint8_t fg;
  uint8_t bg;
} color_t;

const color_t zero_col = { 0xff, 0 };

#define N_COLORS 6
const color_t colors[N_COLORS] = {
  { 0b11100000, 0 },
  { 0b00011100, 0 },
  { 0b00000011, 0 },
  { 0b11111100, 0 },
  { 0b00011111, 0 },
  { 0b11100011, 0 }
};


typedef struct {
  uint8_t val;
  //color_t col;
} cell_t;

void cell_init(cell_t* c)
{
  c->val = 0;
}

char cell_chr(cell_t* c)
{
  if (c->val < 1)
    return '.';
  uint val = c->val - 1;
  return (val <= 9) ? ('0' + val) : ('a' + (val - 10));
}

void cell_set_new_value(cell_t* c, uint val)
{
  c->val = val;
}

const color_t* cell_col(cell_t* c)
{
  if (c->val < 1)
    return &zero_col;

  uint col = c->val % N_COLORS;
  return colors + col;
}


// working around malloc
#define BOARD_ABSOLUTE_MAX_CELLS (8*8)

typedef struct {
  uint w;
  uint h;
  uint n;
  const char* font;
  cell_t cells[BOARD_ABSOLUTE_MAX_CELLS];
  uint cell_size_px;
  uint n_empty_cells;
  uint n_moves;

  bool menu_active;
  uint menu_item;
} board_t;

void board_set_font(board_t* b, const char* font);
void board_reinit(board_t* b);

void board_init(board_t* b, uint w, uint h, const char* font)
{
  b->w = w;
  b->h = h;

  board_set_font(b, font);

  b->menu_active = true;
  b->menu_item = 0;
  board_reinit(b);
}

void board_reinit(board_t* b)
{
  b->n = b->w * b->h;

  if (b->n > BOARD_ABSOLUTE_MAX_CELLS) {
    b->w = 2;
    b->h = 2;
    b->n = 4;
  }

  for (uint i = 0; i < b->n; i ++) {
    cell_init(b->cells + i);
  }
  b->n_empty_cells = b->n;

  b->n_moves = 0;
}

void board_set_font(board_t* b, const char* font)
{
  b->font = font;
}

cell_t* board_cell(board_t* b, uint x, uint y)
{
  return b->cells + (y*b->w + x);
}

#define menu_N 5
static const char * const menu_str[menu_N] =
  { "play!", "font", "width", "height", "quit" };

void board_menu_draw(board_t* b)
{
  setTextColor(0, 0xff);
  setIntFont(&Font_7x8);

  const int lineh = getFontHeight();

  lcdSetCrsr(0, 0);
  lcdPrintln("0xb number game");
  lcdPrintln("Push left/right/");
  lcdPrintln("up/down. Same");
  lcdPrintln("numbers combine.");
  lcdPrintln("Try to reach 'b'!");
  lcdNl();

  for (uint i = 0; i < menu_N; i ++) {
    if (i == b->menu_item)
      lcdPrint(">> ");
    else
      lcdPrint("   ");
    lcdPrint(menu_str[i]);

    switch (i) {
      case 2:
        // width
        lcdPrint(" = ");
        lcdPrint(IntToStr(b->w, 2, 0));
        break;
      case 3:
        // width
        lcdPrint(" = ");
        lcdPrint(IntToStr(b->h, 2, 0));
      default:
        break;
    }
    lcdNl();
  }

  if (b->menu_item == 1) {
    lcdPrintln(b->font);
    setExtFont(b->font);
    lcdPrint("789ab");
  }
}

void board_draw(board_t* b)
{
  if (b->menu_active)
    return board_menu_draw(b);

  setTextColor(0, 0xff);
  setIntFont(&Font_7x8);
  if (b->n_moves < 3)
    DoString(0, 0, "try to get to 'b'!");
  else
    DoString(0, 0, IntToStr(b->n_moves, 6, 0));

  setExtFont(b->font);
  b->cell_size_px = getFontHeight();

  uint centeringx = (RESX - (b->cell_size_px * b->w)) / 2;
  if (centeringx >= RESX)
    centeringx = 0;

  uint centeringy = (RESY - (b->cell_size_px * b->h)) / 2;
  if (centeringy >= RESY)
    centeringy = 0;
  if (centeringy < 8)
    centeringy = 8;

  uint x, y;

  for (x = 0; x < b->w; x ++) {
    for (y = 0; y < b->h; y ++) {
      cell_t* c = board_cell(b, x, y);
      const color_t* col = cell_col(c);
      setTextColor(col->bg, col->fg);
      uint xx = centeringx + x * b->cell_size_px;
      uint yy = centeringy + y * b->cell_size_px;
      if (xx < RESX && yy < RESY)
        DoChar(xx, yy, cell_chr(c));
    }
  }

}

/* Push cells in one direction, combining similar ones.
   Each pitch is the (signed) number to add to a cell index to reach an
   adjacent cell. pitch0 leads to the next cell in this line, pitch1 jumps from
   one line of cells to the next. Depending on the sign and value of the two
   pitches, this shoves vertically or horizontally, forward or backward.
   */
void board_shove(board_t *b, cell_t* start, int pitch0, int pitch1, uint n0, uint n1)
{
  uint n_empty_cells = 0;
  for (uint p1 = 0; p1 < n1; p1 ++) {
    cell_t* to = start + (p1 * pitch1);
    cell_t* from = to;
    cell_t* prev = NULL;

    uint remaining0 = n0;
    for (uint p0 = 0; p0 < n0; p0 ++, from += pitch0) {
      if (from->val) {
        if (prev && (prev->val == from->val)) {
          prev->val ++;
          prev = NULL;
        }
        else {
          *to = *from;
          prev = to;
          to += pitch0;
          remaining0 --;
        }
      }
    }

    // All nonzero cells have been handled. The rest of this line must be empty
    // cells.
    n_empty_cells += remaining0;
    for (; remaining0; remaining0 --, to += pitch0) {
      cell_init(to);
    }
  }

  b->n_empty_cells = n_empty_cells;
}

uint board_random(board_t* b, uint of_n)
{
  if (of_n < 2)
    return 0;
  return rand() % of_n;
}

void board_drop_new_value(board_t* b)
{
  uint drop_at = board_random(b, b->n_empty_cells);

  // find the Nth empty cell (Nth = drop_at)
  cell_t* c = b->cells;
  for (uint i = 0; i < b->n; i ++, c ++) {
    if (! c->val) {
      if (drop_at) {
        // not there yet
        drop_at --;
      }
      else {
        cell_set_new_value(c, 1 + board_random(b, 2)); // 1 or 2
        break;
      }
    }
  }
}

/* Return true to continue running, false to exit program. */
bool board_handle_input(board_t* b)
{
  static uint8_t current_font = 0;

  getInputWaitRelease();
  uint8_t key = getInputWait();

  bool reinit_board = false;

  if (b->menu_active) {
    switch (key) {
      case BTN_UP:
        if (b->menu_item == 0)
          b->menu_item = menu_N - 1;
        else
          b->menu_item --;
        break;

      case BTN_DOWN:
        b->menu_item ++;
        if (b->menu_item >= menu_N)
          b->menu_item = 0;
        break;

      case BTN_ENTER:
        b->menu_active = false;
        return b->menu_item != 4; // != quit

      default:

        switch (b->menu_item) {
          case 0:
            b->menu_active = false;
            break;

          case 1:
            // font
            if (key == BTN_LEFT) {
              if (current_font == 0)
                current_font = N_FONTS - 1;
              else
                current_font --;
            }
            else
            if (key == BTN_RIGHT) {
              current_font ++;
              if (current_font >= N_FONTS)
                current_font = 0;
            }
            board_set_font(b, font_list[current_font]);
            break;

          case 2:
            // width
            reinit_board = true;
            if (key == BTN_LEFT) {
              if (b->w > 2)
                b->w --;
            }
            else
            if (key == BTN_RIGHT) {
              if (b->w < 8)
                b->w ++;
            }
            else
              reinit_board = false;
            break;

          case 3:
            // height
            reinit_board = true;
            if (key == BTN_LEFT) {
              if (b->h > 2)
                b->h --;
            }
            else
            if (key == BTN_RIGHT) {
              if (b->h < 8)
                b->h ++;
            }
            else
              reinit_board = false;
            break;

          case 4:
            return false;
        }
    }

    if (reinit_board) {
      board_reinit(b);
    }
  }
  else {

    int pitch0;
    int pitch1;
    uint n0;
    uint n1;
    cell_t* start;

    switch (key) {
      case BTN_LEFT:
        n0 = b->w;
        n1 = b->h;
        pitch0 = 1;
        pitch1 = b->w;
        start = b->cells;
        break;

      case BTN_RIGHT:
        n0 = b->w;
        n1 = b->h;
        pitch0 = -1;
        pitch1 = b->w;
        start = b->cells + (b->w - 1);
        break;

      case BTN_UP:
        n0 = b->h;
        n1 = b->w;
        pitch0 = b->w;
        pitch1 = 1;
        start = b->cells;
        break;

      case BTN_DOWN:
        n0 = b->h;
        n1 = b->w;
        pitch0 = - b->w;
        pitch1 = 1;
        start = b->cells + ((b->h - 1) * b->w);
        break;

      case BTN_ENTER:
        b->menu_active = true;
        return true;

      default:
        // unknown input. do nothing and go on.
        return true;
    }

    board_shove(b, start, pitch0, pitch1, n0, n1);
    board_drop_new_value(b);
    b->n_moves ++;
  }

  return true;
}


void ram(void) {
  board_t b;

  board_init(&b, 4, 4, font_list[0]);

  board_drop_new_value(&b);
  board_drop_new_value(&b);

  do {
    lcdClear();
    lcdFill(0x00);

    board_draw(&b);

    lcdDisplay();

  } while(board_handle_input(&b));

  setTextColor(0xFF,0x00);
  return;
}

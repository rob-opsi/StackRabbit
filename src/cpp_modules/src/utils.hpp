#ifndef UTILS
#define UTILS

#include <stdarg.h>
#include <stdio.h>
#include <random>
#include "./config.hpp"

// No-op used to mark output parameters
#define OUT

// Shifts a variable X by Y places, either left or right depending on the sign
#define SHIFTBY(x, y) ((y) > 0 ? x >> (y) : (x) << (-1 * (y)))

// Piece spawn locations
#define SPAWN_X 3
#define SPAWN_Y(pieceIndex) ((pieceIndex) == 0 ? -2 : -1)

// Useful bit-rows
#define FULL_ROW 1023 // = 1111111111
#define HOLE_WEIGHT_BIT (1 << 30) // = 01000...0000000000, marks when a row needs to be cleared
#define TUCK_SETUP_BIT(x) (1 << (29 - x)) // See the comment in types.h for an explanation of this encoding
#define HOLE_BIT(x) (1 << (19 - x)) // See the comment in types.h for an explanation of this encoding
#define ALL_TUCK_SETUP_BITS (1023 << 20)
#define ALL_HOLE_BITS (1023 << 10)
#define ALL_AUXILIARY_BITS (~1023) // The union of hole bits and tuck bits

// Other encodings
#define TUCK_COL_ENCODED(r, x) ((r) * 10 + (x) + 2) // Encoding of a rotation/column pair, as a number 0-39
#define UNREACHED 99

// Converts a SimState to a LockPlacement (assuming no tuck)
#define TO_LOCK_PLACEMENT(s) ({(s).x, (s).y, (s).rotationIndex, -1, '.'})

/* ---------- LOGGING ----------- */

void maybePrint(const char *format, ...) {
  if (!LOGGING_ENABLED) {
    return;
  }
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
}

void printBoard(int board[20]) {
  printf("----- Board start -----\n");
  for (int i = 0; i < 20; i++) {
    char line[] = "..........";
    int thisRow = board[i];
    for (int j = 0; j < 10; j++) {
      line[9 - j] = (thisRow & 0x1) ? 'X' : '.';
      thisRow = thisRow >> 1;
    }
    printf("%s\n", line);
  }
}

void printBoardWithPiece(int board[20], Piece piece, int x, int y, int rot){
  printf("----- Board & piece start -----\n");
  for (int i = 0; i < 20; i++) {
    char line[] = "..........";
    int thisRow = board[i];
    if (i >= y && i < y+4) {
      thisRow |= SHIFTBY(piece.rowsByRotation[rot][i - y], x);
    }
    for (int j = 0; j < 10; j++) {
      line[9 - j] = (thisRow & 0x1) ? 'X' : '.';
      thisRow = thisRow >> 1;
    }
    printf("%s\n", line);
  }
}

void printSurface(int surfaceArray[10]) {
  for (int i = 0; i < 9; i++) {
    printf("%d ", surfaceArray[i]);
  }
  printf("%d\n", surfaceArray[9]);
}

void printArray(int *array, int range, char const *description){
  printf("%s:  ", description);
  for (int i = 0; i < range; i++) {
    printf("%02d ", array[i]);
  }
  printf("\n");
}

void printBoardBits(int board[20]){
  maybePrint("Tuck setups:\n");
  for (int i = 0; i < 19; i++) {
    maybePrint("%d ", (board[i] & ALL_TUCK_SETUP_BITS) >> 20);
  }
  maybePrint("%d\n", (board[19] & ALL_TUCK_SETUP_BITS) >> 20);
  maybePrint("Holes:\n");
  for (int i = 0; i < 19; i++) {
    maybePrint("%d ", (board[i] & ALL_HOLE_BITS) >> 10);
  }
  maybePrint("%d\n", (board[19] & ALL_HOLE_BITS) >> 10);
  maybePrint("Hole weights:\n");
  for (int i = 0; i < 19; i++) {
    maybePrint("%d ", (board[i] & HOLE_WEIGHT_BIT) > 0);
  }
  maybePrint("%d\n", (board[19] & HOLE_WEIGHT_BIT) > 0);

  maybePrint("END OF INITIAL BOARD STATE\n");
}


/* --------- BOARD ENCODINGS -------- */

void encodeBoard(char const *boardStr, int outBoard[20]) {
  for (int i = 0; i < 20; i++) {
    int acc = 0;
    for (int j = 0; j < 10; j++) {
      char c = boardStr[i * 10 + j];
      acc *= 2;
      if (c == '1') {
        acc += 1;
      }
    }
    outBoard[i] = acc;
  }
}

void getSurfaceArray(int board[20], int outSurface[10]) {
  for (int col = 0; col < 10; col++) {
    int colMask = 1 << (9 - col);
    int row = 0;
    while (!(board[row] & colMask) && row < 20) {
      row++;
    }
    outSurface[col] = 20 - row;
  }
}

/* ----------- MISC GAMEPLAY HELPERS ----------- */

int getLevelAfterLineClears(int level, int lines, int numLinesCleared) {
  // If it hasn't reached transition, it can't go up in level
  if (level == 18 && lines < 126) {
    return 18;
  }
  if (level == 19 && lines < 136) {
    return 19;
  }
  if (level == 29 && lines < 196) {
    return 29;
  }

  // Otherwise it goes up every time you cross a multiple of 10
  if ((lines % 10) + numLinesCleared >= 10) {
    return level + 1;
  }
  return level;
}

int getGravity(int level){
  if (level <= 18) {
    return 3;
  } else if (level < 29) {
    return 2;
  }
  return 1;
}

/**
 * Given a string such as X.... that represents a loop of which frames are allowed for inputs,
 * determines if a given frame index is an input frame
 */
int shouldPerformInputsThisFrame(int frameIndex, char const *inputFrameTimeline) {
  int len = (int) strlen(inputFrameTimeline);
  int index = frameIndex % len;
  return inputFrameTimeline[index] == 'X';
}

template<typename T>
T qualityRandom(T range_from, T range_to) {
  std::random_device rand_dev;
  std::mt19937 generator(rand_dev());
  std::uniform_int_distribution<T>    distr(range_from, range_to - 1);
  return distr(generator);
}

#endif

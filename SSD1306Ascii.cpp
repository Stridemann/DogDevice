/**
 * Copyright (c) 2011-2023 Bill Greiman
 * This file is part of the Arduino SSD1306Ascii Library
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include "SSD1306Ascii.h"
//------------------------------------------------------------------------------
uint8_t SSD1306Ascii::charWidth(uint8_t c) const {
  if (!m_font) {
    return 0;
  }
  uint8_t first = readFontByte(m_font + FONT_FIRST_CHAR);
  uint8_t count = readFontByte(m_font + FONT_CHAR_COUNT);
  if (c < first || c >= (first + count)) {
    return 0;
  }
  if (fontSize() > 1) {
    // Proportional font.
    return m_magFactor * readFontByte(m_font + FONT_WIDTH_TABLE + c - first);
  }
  // Fixed width font.
  return m_magFactor * readFontByte(m_font + FONT_WIDTH);
}
//------------------------------------------------------------------------------
void SSD1306Ascii::clear() {
#if INCLUDE_SCROLLING
  m_pageOffset = 0;
  setStartLine(0);
#endif  // INCLUDE_SCROLLING
  clear(0, displayWidth() - 1, 0, displayRows() - 1);
}
//------------------------------------------------------------------------------
void SSD1306Ascii::clear(uint8_t c0, uint8_t c1, uint8_t r0, uint8_t r1) {
  // Cancel skip character pixels.
  m_skip = 0;

  // Insure only rows on display will be cleared.
  if (r1 >= displayRows()) r1 = displayRows() - 1;

  for (uint8_t r = r0; r <= r1; r++) {
    setCursor(c0, r);
    for (uint8_t c = c0; c <= c1; c++) {
      // Insure clear() writes zero. result is (m_invertMask^m_invertMask).
      ssd1306WriteRamBuf(m_invertMask);
    }
  }
  setCursor(c0, r0);
}
//------------------------------------------------------------------------------
void SSD1306Ascii::clearToEOL() {
  clear(m_col, displayWidth() - 1, m_row, m_row + fontRows() - 1);
}
//------------------------------------------------------------------------------
void SSD1306Ascii::clearField(uint8_t col, uint8_t row, uint8_t n) {
  clear(col, col + fieldWidth(n) - 1, row, row + fontRows() - 1);
}
//------------------------------------------------------------------------------
void SSD1306Ascii::displayRemap(bool mode) {
  ssd1306WriteCmd(mode ? SSD1306_SEGREMAP : SSD1306_SEGREMAP | 1);
  ssd1306WriteCmd(mode ? SSD1306_COMSCANINC : SSD1306_COMSCANDEC);
}
//------------------------------------------------------------------------------
size_t SSD1306Ascii::fieldWidth(uint8_t n) {
  return n * (fontWidth() + letterSpacing());
}
//------------------------------------------------------------------------------
uint8_t SSD1306Ascii::fontCharCount() const {
  return m_font ? readFontByte(m_font + FONT_CHAR_COUNT) : 0;
}
//------------------------------------------------------------------------------
char SSD1306Ascii::fontFirstChar() const {
  return m_font ? readFontByte(m_font + FONT_FIRST_CHAR) : 0;
}
//------------------------------------------------------------------------------
uint8_t SSD1306Ascii::fontHeight() const {
  return m_font ? m_magFactor * readFontByte(m_font + FONT_HEIGHT) : 0;
}
//------------------------------------------------------------------------------
uint8_t SSD1306Ascii::fontRows() const {
  return m_font ? m_magFactor * ((readFontByte(m_font + FONT_HEIGHT) + 7) / 8)
                : 0;
}
//------------------------------------------------------------------------------
uint16_t SSD1306Ascii::fontSize() const {
  return (readFontByte(m_font) << 8) | readFontByte(m_font + 1);
}
//------------------------------------------------------------------------------
uint8_t SSD1306Ascii::fontWidth() const {
  return m_font ? m_magFactor * readFontByte(m_font + FONT_WIDTH) : 0;
}
//------------------------------------------------------------------------------
void SSD1306Ascii::init(const DevType* dev) {
  m_col = 0;
  m_row = 0;
#ifdef __AVR__
  const uint8_t* table =
      reinterpret_cast<const uint8_t*>(pgm_read_word(&dev->initcmds));
#else   // __AVR__
  const uint8_t* table = dev->initcmds;
#endif  // __AVR
  uint8_t size = readFontByte(&dev->initSize);
  m_displayWidth = readFontByte(&dev->lcdWidth);
  m_displayHeight = readFontByte(&dev->lcdHeight);
  m_colOffset = readFontByte(&dev->colOffset);
  for (uint8_t i = 0; i < size; i++) {
    ssd1306WriteCmd(readFontByte(table + i));
  }
  clear();
}
//------------------------------------------------------------------------------
void SSD1306Ascii::invertDisplay(bool invert) {
  ssd1306WriteCmd(invert ? SSD1306_INVERTDISPLAY : SSD1306_NORMALDISPLAY);
}
//------------------------------------------------------------------------------
void SSD1306Ascii::setCol(uint8_t col) {
  if (col < m_displayWidth) {
    m_col = col;
    col += m_colOffset;
    ssd1306WriteCmd(SSD1306_SETLOWCOLUMN | (col & 0XF));
    ssd1306WriteCmd(SSD1306_SETHIGHCOLUMN | (col >> 4));
  }
}
//------------------------------------------------------------------------------
void SSD1306Ascii::setContrast(uint8_t value) {
  ssd1306WriteCmd(SSD1306_SETCONTRAST);
  ssd1306WriteCmd(value);
}
//------------------------------------------------------------------------------
void SSD1306Ascii::setCursor(uint8_t col, uint8_t row) {
  setCol(col);
  setRow(row);
}
//------------------------------------------------------------------------------
void SSD1306Ascii::setFont(const uint8_t* font) {
  m_font = font;
  if (font && fontSize() == 1) {
    m_letterSpacing = 0;
  } else {
    m_letterSpacing = 1;
  }
}
//------------------------------------------------------------------------------
void SSD1306Ascii::setRow(uint8_t row) {
  if (row < displayRows()) {
    m_row = row;
//#if INCLUDE_SCROLLING
    //ssd1306WriteCmd(SSD1306_SETSTARTPAGE | ((m_row + m_pageOffset) & 7));
//#else   // INCLUDE_SCROLLING
    ssd1306WriteCmd(SSD1306_SETSTARTPAGE | m_row);
//#endif  // INCLUDE_SCROLLING
  }
}
#if INCLUDE_SCROLLING
//------------------------------------------------------------------------------
void SSD1306Ascii::setPageOffset(uint8_t page) {
  m_pageOffset = page & 7;
  setRow(m_row);
}
//------------------------------------------------------------------------------
void SSD1306Ascii::setStartLine(uint8_t line) {
  m_startLine = line & 0X3F;
  ssd1306WriteCmd(SSD1306_SETSTARTLINE | m_startLine);
}
#endif  // INCLUDE_SCROLLING
//------------------------------------------------------------------------------
void SSD1306Ascii::ssd1306WriteRam(uint8_t c) {
  if (m_row < m_displayHeight) {
    writeDisplay(c ^ m_invertMask, SSD1306_MODE_RAM);
    //m_row++;
  }
}
//------------------------------------------------------------------------------
void SSD1306Ascii::ssd1306WriteRamBuf(uint8_t c) {
  if (m_skip) {
    m_skip--;
  } else if (m_row < m_displayHeight) {
    writeDisplay(c ^ m_invertMask, SSD1306_MODE_RAM_BUF);
    //m_row++;
  }
}
//------------------------------------------------------------------------------
GLCDFONTDECL(scaledNibble) = {0X00, 0X03, 0X0C, 0X0F, 0X30, 0X33, 0X3C, 0X3F,
                              0XC0, 0XC3, 0XCC, 0XCF, 0XF0, 0XF3, 0XFC, 0XFF};
//------------------------------------------------------------------------------
size_t SSD1306Ascii::strWidth(const char* str) const {
  size_t sw = 0;
  while (*str) {
    uint8_t cw = charWidth(*str++);
    if (cw == 0) {
      return 0;
    }
    sw += cw + letterSpacing();
  }
  return sw;
}
//------------------------------------------------------------------------------
void SSD1306Ascii::tickerInit(TickerState* state, const uint8_t* font,
                              uint8_t row, bool mag2X, uint8_t bgnCol,
                              uint8_t endCol) {
  state->font = font;
  state->row = row;
  state->mag2X = mag2X;
  state->bgnCol = bgnCol;
  state->endCol = endCol < m_displayWidth ? endCol : m_displayWidth - 1;
  state->nQueue = 0;
}
//------------------------------------------------------------------------------
bool SSD1306Ascii::tickerText(TickerState* state, const char* text) {
  if (!text) {
    state->nQueue = 0;
    return true;
  }
  if (state->nQueue >= TICKER_QUEUE_DIM) {
    return false;
  }
  if (state->nQueue == 0) {
    state->init = true;
  }
  state->queue[state->nQueue++] = text;
  return true;
}
//------------------------------------------------------------------------------
int8_t SSD1306Ascii::tickerTick(TickerState* state) {
  if (!state->font) {
    return -1;
  }
  if (!state->nQueue) {
    return 0;
  }
  setFont(state->font);
  m_magFactor = state->mag2X ? 2 : 1;
  if (state->init) {
    clear(state->bgnCol, state->endCol, state->row,
          state->row + fontRows() - 1);
    state->col = state->endCol;
    state->skip = 0;
    state->init = false;
  }
  // Adjust display width to truncate pixels after endCol.  Find better way?
  uint8_t save = m_displayWidth;
  m_displayWidth = state->endCol + 1;

  // Skip pixels before bgnCol.
  skipColumns(state->skip);
  setCursor(state->col, state->row);
  for (uint8_t i = 0; i < state->nQueue; i++) {
    const char* str = state->queue[i];
    while (*str && m_col <= state->endCol) {
      write(*str++);
    }
    if (m_col > state->endCol) {
      break;
    }
  }
  if (m_col <= state->endCol) {
    clear(m_col, m_col, state->row, state->row + fontRows() - 1);
  }
  // Restore display width.
  m_displayWidth = save;

  if (state->nQueue == 1 && *state->queue[0] == 0) {
    state->nQueue = 0;
    return 0;
  }
  if (state->col > state->bgnCol) {
    state->col--;
  } else {
    state->skip++;
    if (state->skip >= charSpacing(*state->queue[0])) {
      state->skip = 0;
      state->queue[0]++;
      if (*state->queue[0] == 0 && state->nQueue > 1) {
        state->nQueue--;
        for (uint8_t i = 0; i < state->nQueue; i++) {
          state->queue[i] = state->queue[i + 1];
        }
      }
    }
  }
  return state->nQueue;
}
//------------------------------------------------------------------------------
size_t SSD1306Ascii::print(const String &s)
{
  return write(s.c_str(), s.length());
}
size_t SSD1306Ascii::write(const uint8_t *buffer, size_t size)
{
  size_t n = 0;
  while (size--) {
    if (write(*buffer++)) n++;
    else break;
  }
  return n;
}

size_t SSD1306Ascii::print(const char str[])
{
  return write(str);
}

size_t SSD1306Ascii::print(char c)
{
  return write(c);
}

size_t SSD1306Ascii::print(unsigned char b, int base)
{
  return print((unsigned long) b, base);
}

size_t SSD1306Ascii::print(int n, int base)
{
  return print((long) n, base);
}

size_t SSD1306Ascii::print(unsigned int n, int base)
{
  return print((unsigned long) n, base);
}

size_t SSD1306Ascii::print(long n, int base)
{
  if (base == 0) {
    return write('0');
  } else if (base == 10) {
    if (n < 0) {
      int t = print('-');
      n = -n;
      return printNumber(n, 10) + t;
    }
    return printNumber(n, 10);
  } else {
    return printNumber(n, base);
  }
}

size_t SSD1306Ascii::print(unsigned long n, int base)
{
  if (base == 0) return write(n);
  else return printNumber(n, base);
}

size_t SSD1306Ascii::println(const __FlashStringHelper *ifsh)
{
  size_t n = print(ifsh);
  n += println();
  return n;
}

size_t SSD1306Ascii::println(void)
{
  return write("\r\n");
}

size_t SSD1306Ascii::println(const String &s)
{
  size_t n = print(s);
  n += println();
  return n;
}

size_t SSD1306Ascii::println(const char c[])
{
  size_t n = print(c);
  n += println();
  return n;
}

size_t SSD1306Ascii::println(char c)
{
  size_t n = print(c);
  n += println();
  return n;
}

size_t SSD1306Ascii::println(unsigned char b, int base)
{
  size_t n = print(b, base);
  n += println();
  return n;
}

size_t SSD1306Ascii::println(int num, int base)
{
  size_t n = print(num, base);
  n += println();
  return n;
}

size_t SSD1306Ascii::println(unsigned int num, int base)
{
  size_t n = print(num, base);
  n += println();
  return n;
}

size_t SSD1306Ascii::println(long num, int base)
{
  size_t n = print(num, base);
  n += println();
  return n;
}

size_t SSD1306Ascii::println(unsigned long num, int base)
{
  size_t n = print(num, base);
  n += println();
  return n;
}

size_t SSD1306Ascii::printNumber(unsigned long n, uint8_t base)
{
  char buf[8 * sizeof(long) + 1]; // Assumes 8-bit chars plus zero byte.
  char *str = &buf[sizeof(buf) - 1];

  *str = '\0';

  // prevent crash if called with base == 1
  if (base < 2) base = 10;
  int cntLen = 0;
  do {
    char c = n % base;
    n /= base;

    *--str = c < 10 ? c + '0' : c + 'A' - 10;
    cntLen++;
  } while(n);

  int printLen = cntLen;
  while (cntLen--) {
    write(*str++);
  }

  return printLen;
}

size_t SSD1306Ascii::write(uint8_t ch) {
  if (!m_font) {
    return 0;
  }
  uint8_t w = readFontByte(m_font + FONT_WIDTH);
  uint8_t h = readFontByte(m_font + FONT_HEIGHT);
  uint8_t nr = (h + 7) / 8;
  uint8_t first = readFontByte(m_font + FONT_FIRST_CHAR);
  uint8_t count = readFontByte(m_font + FONT_CHAR_COUNT);
  const uint8_t* base = m_font + FONT_WIDTH_TABLE;

  if (ch == '\r') {
    setRow(0);
    return 1;
  }
  if (ch == '\n') {
    setRow(0);
    uint8_t fr = m_magFactor * nr;
    setCol(m_col + fr);
    return 1;
  }
  bool nfSpace = false;
  if (first <= ch && ch < (first + count)) {
    ch -= first;
  } else if (ENABLE_NONFONT_SPACE && ch == ' ') {
    nfSpace = true;
  } else {
    // Error if not in font.
    return 0;
  }
  uint8_t s = letterSpacing();
  uint8_t thieleShift = 0;
  if (nfSpace) {
    // non-font space.
  } else if (fontSize() < 2) {
    // Fixed width font.
    base += nr * w * ch;
  } else {
    if (h & 7) {
      thieleShift = 8 - (h & 7);
    }
    uint16_t index = 0;
    for (uint8_t i = 0; i < ch; i++) {
      index += readFontByte(base + i);
    }
    w = readFontByte(base + ch);
    base += nr * index + count;
  }
  uint8_t scol = m_col;
  uint8_t srow = m_row;
  uint8_t skip = m_skip;
  for (uint8_t r = 0; r < nr; r++) {
    for (uint8_t m = 0; m < m_magFactor; m++) {
      skipColumns(skip);
      if (r || m) {
        setCursor(scol + w, m_row);
      }
      for (uint8_t c = 0; c < w; c++) {
        uint8_t b = nfSpace ? 0 : readFontByte(base + c + r * w);
        if (thieleShift && (r + 1) == nr) {
          b >>= thieleShift;
        }
        ssd1306WriteRamBuf(b);
      }
      for (uint8_t i = 0; i < s; i++) {
        ssd1306WriteRamBuf(0);
      }
    }
  }
  setRow(srow + 1);
  setCol(scol);
  return 1;
}

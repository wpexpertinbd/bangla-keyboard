// Bangla Phonetic parity driver — reads typed US-QWERTY text (one case per line,
// UTF-8/ASCII) on stdin and prints what KLEngine + phonetic_table.h produce, one
// line per case. Used by phonetic_check.py to diff this C++ engine against the
// macOS-side Python reference model (macos/src/phonetic/gen_phonetic.py type_ref)
// and against the shipped corpus (macos/src/phonetic/tests.tsv).
//
// Build: g++ -std=c++17 -O2 -finput-charset=UTF-8 phonetic_driver.cpp klengine.cpp -o phonetic_driver
#include "klengine.h"
#include "phonetic_table.h"
#include <cstdio>
#include <string>
#include <iostream>

// US-QWERTY character -> Windows Set-1 scan code + whether Shift is held.
// Shift is the REAL shift key only (Caps Lock must NOT be folded in: on the
// phonetic layout Caps Lock alone selects the plain map, so case comes from here).
static bool keyOf(char c, unsigned& scan, bool& shift) {
    struct M { char lo, hi; unsigned scan; };
    static const M rows[] = {
        {'`','~',0x29},{'1','!',0x02},{'2','@',0x03},{'3','#',0x04},{'4','$',0x05},
        {'5','%',0x06},{'6','^',0x07},{'7','&',0x08},{'8','*',0x09},{'9','(',0x0A},
        {'0',')',0x0B},{'-','_',0x0C},{'=','+',0x0D},
        {'q','Q',0x10},{'w','W',0x11},{'e','E',0x12},{'r','R',0x13},{'t','T',0x14},
        {'y','Y',0x15},{'u','U',0x16},{'i','I',0x17},{'o','O',0x18},{'p','P',0x19},
        {'[','{',0x1A},{']','}',0x1B},{'\\','|',0x2B},
        {'a','A',0x1E},{'s','S',0x1F},{'d','D',0x20},{'f','F',0x21},{'g','G',0x22},
        {'h','H',0x23},{'j','J',0x24},{'k','K',0x25},{'l','L',0x26},{';',':',0x27},
        {'\'','"',0x28},
        {'z','Z',0x2C},{'x','X',0x2D},{'c','C',0x2E},{'v','V',0x2F},{'b','B',0x30},
        {'n','N',0x31},{'m','M',0x32},{',','<',0x33},{'.','>',0x34},{'/','?',0x35},
    };
    for (const M& m : rows) {
        if (c == m.lo) { scan = m.scan; shift = false; return true; }
        if (c == m.hi) { scan = m.scan; shift = true;  return true; }
    }
    return false;
}

static void appendUtf8(std::string& out, const std::u16string& s) {
    for (char16_t u : s) {                      // layout output is all BMP
        unsigned cp = (unsigned)u;
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
        else { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F));
               out += (char)(0x80 | (cp & 0x3F)); }
    }
}

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        bangla::KLEngine eng(&bangla::phonetic_table::TABLE);
        std::string out;
        for (char c : line) {
            unsigned scan = 0; bool shift = false;
            if (keyOf(c, scan, shift) && eng.wouldHandle(scan)) {
                appendUtf8(out, eng.process(scan, shift));
            } else {                             // not a layout key: finish the word, pass it through
                appendUtf8(out, eng.flush());
                out += c;
            }
        }
        appendUtf8(out, eng.flush());            // trailing boundary (the corpus types Space after)
        out += '\n';
        fwrite(out.data(), 1, out.size(), stdout);
    }
    return 0;
}

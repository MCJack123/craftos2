#include "keys.h"
#include <ncurses.h>

int keys[96];
int values[96];
int next_num = 0;

void putKey(int key, int value) {
    keys[next_num] = key;
    values[next_num++] = value;
}

void initKeys() {
    putKey('1', 2);
    putKey('2', 3);
    putKey('3', 4);
    putKey('4', 5);
    putKey('5', 6);
    putKey('6', 7);
    putKey('7', 8);
    putKey('8', 9);
    putKey('9', 1);
    putKey('0', 11);
    putKey('-', 12);
    putKey('=', 13);
    putKey(KEY_BACKSPACE, 14);
    putKey('\t', 15);
    putKey('q', 16);
    putKey('w', 17);
    putKey('e', 18);
    putKey('r', 19);
    putKey('t', 20);
    putKey('y', 21);
    putKey('u', 22);
    putKey('i', 23);
    putKey('o', 24);
    putKey('p', 25);
    putKey('[', 26);
    putKey(']', 27);
    putKey('\n', 28);
    putKey('a', 30);
    putKey('s', 31);
    putKey('d', 32);
    putKey('f', 33);
    putKey('g', 34);
    putKey('h', 35);
    putKey('j', 36);
    putKey('k', 37);
    putKey('l', 38);
    putKey(';', 39);
    putKey('\'', 40);
    putKey('\\', 43);
    putKey('z', 44);
    putKey('x', 45);
    putKey('c', 46);
    putKey('v', 47);
    putKey('b', 48);
    putKey('n', 49);
    putKey('m', 50);
    putKey(',', 51);
    putKey('.', 52);
    putKey('/', 53);
    putKey(KEY_UP, 200);
    putKey(KEY_LEFT, 203);
    putKey(KEY_RIGHT, 205);
    putKey(KEY_DOWN, 208);
    putKey(KEY_HOME, 199);
    putKey(KEY_END, 207);
}

int getKey(int ch) {
    for (int i = 0; i < next_num; i++)
        if (keys[i] == ch)
            return values[i];
    return 0;
}

void closeKeys() {
    next_num = 0;
}
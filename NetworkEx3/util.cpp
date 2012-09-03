#include <Windows.h>
#include "util.h"

void Highlight(int level)
{
    int BackC, ForgC;
    switch(level) {
    case 0:
        BackC = 0;
        ForgC = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
        break;
    case 1:
        BackC = BACKGROUND_RED | BACKGROUND_GREEN;
        ForgC = FOREGROUND_RED;
        break;
    case 2:
        BackC = BACKGROUND_RED | BACKGROUND_INTENSITY;
        ForgC = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
        break;
    }
    WORD wColor = BackC + ForgC;
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), wColor);
}
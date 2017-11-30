#include "ite/itu.h"

extern bool MainOnTimer(ITUWidget* widget, char* param);
extern bool testkeyOnEnter(ITUWidget* widget, char* param);

ITUActionFunction actionFunctions[] =
{
    "MainOnTimer", MainOnTimer,
    "testkeyOnEnter", testkeyOnEnter,
    NULL, NULL
};

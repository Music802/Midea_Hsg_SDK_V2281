#include "ite/itu.h"

extern bool MainOnTimer(ITUWidget* widget, char* param);

ITUActionFunction actionFunctions[] =
{
    "MainOnTimer", MainOnTimer,
    NULL, NULL
};

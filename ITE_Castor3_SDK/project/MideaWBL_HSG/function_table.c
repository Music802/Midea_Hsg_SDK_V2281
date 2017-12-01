#include "ite/itu.h"

extern bool PasswordOnEnter(ITUWidget* widget, char* param);
extern bool KeyboardPYCharButtonOnPress(ITUWidget* widget, char* param);
extern bool KeyboardPYBackButtonOnPress(ITUWidget* widget, char* param);
extern bool DefaultMenuOnEnter(ITUWidget* widget, char* param);
extern bool DefaultMenuSettingConfirmButtonOnMouseUp(ITUWidget* widget, char* param);
extern bool DefaultMenuOnGotoSprite1(ITUWidget* widget, char* param);
extern bool DefaultMenuSetProcessBackButtonOnMouseUp(ITUWidget* widget, char* param);
extern bool DefaultMenuSelectConfirmButtonOnMouseUp(ITUWidget* widget, char* param);
extern bool DefaultMenuModButtonOnMouseUp(ITUWidget* widget, char* param);
extern bool DefaultMenuSetCookingProcessConFirmButtonOnMouseUp(ITUWidget* widget, char* param);
extern bool DefaultMenuSetCookProcessButtonOnMouseUp(ITUWidget* widget, char* param);
extern bool DefaultMenuDisplayButtonOnPress(ITUWidget* widget, char* param);
extern bool ImageOnEnter(ITUWidget* widget, char* param);

ITUActionFunction actionFunctions[] =
{
    "PasswordOnEnter", PasswordOnEnter,
    "KeyboardPYCharButtonOnPress", KeyboardPYCharButtonOnPress,
    "KeyboardPYBackButtonOnPress", KeyboardPYBackButtonOnPress,
    "DefaultMenuOnEnter", DefaultMenuOnEnter,
    "DefaultMenuSettingConfirmButtonOnMouseUp", DefaultMenuSettingConfirmButtonOnMouseUp,
    "DefaultMenuOnGotoSprite1", DefaultMenuOnGotoSprite1,
    "DefaultMenuSetProcessBackButtonOnMouseUp", DefaultMenuSetProcessBackButtonOnMouseUp,
    "DefaultMenuSelectConfirmButtonOnMouseUp", DefaultMenuSelectConfirmButtonOnMouseUp,
    "DefaultMenuModButtonOnMouseUp", DefaultMenuModButtonOnMouseUp,
    "DefaultMenuSetCookingProcessConFirmButtonOnMouseUp", DefaultMenuSetCookingProcessConFirmButtonOnMouseUp,
    "DefaultMenuSetCookProcessButtonOnMouseUp", DefaultMenuSetCookProcessButtonOnMouseUp,
    "DefaultMenuDisplayButtonOnPress", DefaultMenuDisplayButtonOnPress,
    "ImageOnEnter", ImageOnEnter,
    NULL, NULL
};

#include <assert.h>
#include "scene.h"

extern ITUIcon *imageIconNor[16];
extern ITUIcon *imageIconSel[16];

static ITUIcon *defaultMenuDisplayIcon[8] = { 0 };
static ITUIcon *defaultMenuListIcon[16] = { 0 };
static ITURadioBox *defaultMenuListRadioBox[16] = { 0 };
static ITUIcon *defaultMenuSelectIcon = 0;
static ITUSurface *surfacetmp = 0;
static ITUSurface *surfacetmp2 = 0;
static ITUWheel *defaultMenuWheel[3][3] = { 0, 0 };
static ITUText *defaultMenuText[3][2] = { 0, 0 };
static ITUText *defaultMenuProgramNameText = 0;
static ITUTrackBar *defaultMenuTrackBar[3];
static int i, target;

bool DefaultMenuOnEnter(ITUWidget* widget, char* param)
{
	if (!defaultMenuDisplayIcon[0])
	{
		defaultMenuDisplayIcon[0] = ituSceneFindWidget(&theScene, "defaultMenuDisplayIcon0");
		assert(defaultMenuDisplayIcon[0]);

		defaultMenuDisplayIcon[1] = ituSceneFindWidget(&theScene, "defaultMenuDisplayIcon1");
		assert(defaultMenuDisplayIcon[1]);

		defaultMenuDisplayIcon[2] = ituSceneFindWidget(&theScene, "defaultMenuDisplayIcon2");
		assert(defaultMenuDisplayIcon[2]);

		defaultMenuDisplayIcon[3] = ituSceneFindWidget(&theScene, "defaultMenuDisplayIcon3");
		assert(defaultMenuDisplayIcon[3]);

		defaultMenuDisplayIcon[4] = ituSceneFindWidget(&theScene, "defaultMenuDisplayIcon4");
		assert(defaultMenuDisplayIcon[4]);

		defaultMenuDisplayIcon[5] = ituSceneFindWidget(&theScene, "defaultMenuDisplayIcon5");
		assert(defaultMenuDisplayIcon[5]);

		defaultMenuDisplayIcon[6] = ituSceneFindWidget(&theScene, "defaultMenuDisplayIcon6");
		assert(defaultMenuDisplayIcon[6]);

		defaultMenuDisplayIcon[7] = ituSceneFindWidget(&theScene, "defaultMenuDisplayIcon7");
		assert(defaultMenuDisplayIcon[7]);

		defaultMenuListIcon[0] = ituSceneFindWidget(&theScene, "defaultMenuListIcon0");
		assert(defaultMenuListIcon[0]);

		defaultMenuListIcon[1] = ituSceneFindWidget(&theScene, "defaultMenuListIcon1");
		assert(defaultMenuListIcon[1]);

		defaultMenuListIcon[2] = ituSceneFindWidget(&theScene, "defaultMenuListIcon2");
		assert(defaultMenuListIcon[2]);

		defaultMenuListIcon[3] = ituSceneFindWidget(&theScene, "defaultMenuListIcon3");
		assert(defaultMenuListIcon[3]);

		defaultMenuListIcon[4] = ituSceneFindWidget(&theScene, "defaultMenuListIcon4");
		assert(defaultMenuListIcon[4]);

		defaultMenuListIcon[5] = ituSceneFindWidget(&theScene, "defaultMenuListIcon5");
		assert(defaultMenuListIcon[5]);

		defaultMenuListIcon[6] = ituSceneFindWidget(&theScene, "defaultMenuListIcon6");
		assert(defaultMenuListIcon[6]);

		defaultMenuListIcon[7] = ituSceneFindWidget(&theScene, "defaultMenuListIcon7");
		assert(defaultMenuListIcon[7]);

		defaultMenuListIcon[8] = ituSceneFindWidget(&theScene, "defaultMenuListIcon8");
		assert(defaultMenuListIcon[8]);

		defaultMenuListIcon[9] = ituSceneFindWidget(&theScene, "defaultMenuListIcon9");
		assert(defaultMenuListIcon[9]);

		defaultMenuListIcon[10] = ituSceneFindWidget(&theScene, "defaultMenuListIcon10");
		assert(defaultMenuListIcon[10]);

		defaultMenuListIcon[11] = ituSceneFindWidget(&theScene, "defaultMenuListIcon11");
		assert(defaultMenuListIcon[11]);

		defaultMenuListIcon[12] = ituSceneFindWidget(&theScene, "defaultMenuListIcon12");
		assert(defaultMenuListIcon[12]);

		defaultMenuListIcon[13] = ituSceneFindWidget(&theScene, "defaultMenuListIcon13");
		assert(defaultMenuListIcon[13]);

		defaultMenuListIcon[14] = ituSceneFindWidget(&theScene, "defaultMenuListIcon14");
		assert(defaultMenuListIcon[14]);

		defaultMenuListIcon[15] = ituSceneFindWidget(&theScene, "defaultMenuListIcon15");
		assert(defaultMenuListIcon[15]);

		defaultMenuListRadioBox[0] = ituSceneFindWidget(&theScene, "defaultMenuListRadioBox0");
		assert(defaultMenuListRadioBox[0]);

		defaultMenuListRadioBox[1] = ituSceneFindWidget(&theScene, "defaultMenuListRadioBox1");
		assert(defaultMenuListRadioBox[1]);

		defaultMenuListRadioBox[2] = ituSceneFindWidget(&theScene, "defaultMenuListRadioBox2");
		assert(defaultMenuListRadioBox[2]);

		defaultMenuListRadioBox[3] = ituSceneFindWidget(&theScene, "defaultMenuListRadioBox3");
		assert(defaultMenuListRadioBox[3]);

		defaultMenuListRadioBox[4] = ituSceneFindWidget(&theScene, "defaultMenuListRadioBox4");
		assert(defaultMenuListRadioBox[4]);

		defaultMenuListRadioBox[5] = ituSceneFindWidget(&theScene, "defaultMenuListRadioBox5");
		assert(defaultMenuListRadioBox[5]);

		defaultMenuListRadioBox[6] = ituSceneFindWidget(&theScene, "defaultMenuListRadioBox6");
		assert(defaultMenuListRadioBox[6]);

		defaultMenuListRadioBox[7] = ituSceneFindWidget(&theScene, "defaultMenuListRadioBox7");
		assert(defaultMenuListRadioBox[7]);

		defaultMenuListRadioBox[8] = ituSceneFindWidget(&theScene, "defaultMenuListRadioBox8");
		assert(defaultMenuListRadioBox[8]);

		defaultMenuListRadioBox[9] = ituSceneFindWidget(&theScene, "defaultMenuListRadioBox9");
		assert(defaultMenuListRadioBox[9]);

		defaultMenuListRadioBox[10] = ituSceneFindWidget(&theScene, "defaultMenuListRadioBox10");
		assert(defaultMenuListRadioBox[10]);

		defaultMenuListRadioBox[11] = ituSceneFindWidget(&theScene, "defaultMenuListRadioBox11");
		assert(defaultMenuListRadioBox[11]);

		defaultMenuListRadioBox[12] = ituSceneFindWidget(&theScene, "defaultMenuListRadioBox12");
		assert(defaultMenuListRadioBox[12]);

		defaultMenuListRadioBox[13] = ituSceneFindWidget(&theScene, "defaultMenuListRadioBox13");
		assert(defaultMenuListRadioBox[13]);

		defaultMenuListRadioBox[14] = ituSceneFindWidget(&theScene, "defaultMenuListRadioBox14");
		assert(defaultMenuListRadioBox[14]);

		defaultMenuListRadioBox[15] = ituSceneFindWidget(&theScene, "defaultMenuListRadioBox15");
		assert(defaultMenuListRadioBox[15]);

		defaultMenuSelectIcon = ituSceneFindWidget(&theScene, "defaultMenuSelectIcon");
		assert(defaultMenuSelectIcon);

		defaultMenuWheel[0][0] = ituSceneFindWidget(&theScene, "defaultMenuCookTime1MWheel");
		assert(defaultMenuWheel[0][0]);

		defaultMenuWheel[0][1] = ituSceneFindWidget(&theScene, "defaultMenuCookTime1SWheel");
		assert(defaultMenuWheel[0][1]);

		defaultMenuWheel[0][2] = ituSceneFindWidget(&theScene, "defaultMenuPower1Wheel");
		assert(defaultMenuWheel[0][2]);

		defaultMenuWheel[1][0] = ituSceneFindWidget(&theScene, "defaultMenuCookTime2MWheel");
		assert(defaultMenuWheel[1][0]);

		defaultMenuWheel[1][1] = ituSceneFindWidget(&theScene, "defaultMenuCookTime2SWheel");
		assert(defaultMenuWheel[1][1]);

		defaultMenuWheel[1][2] = ituSceneFindWidget(&theScene, "defaultMenuPower2Wheel");
		assert(defaultMenuWheel[1][2]);

		defaultMenuWheel[2][0] = ituSceneFindWidget(&theScene, "defaultMenuCookTime3MWheel");
		assert(defaultMenuWheel[2][0]);

		defaultMenuWheel[2][1] = ituSceneFindWidget(&theScene, "defaultMenuCookTime3SWheel");
		assert(defaultMenuWheel[2][1]);

		defaultMenuWheel[2][2] = ituSceneFindWidget(&theScene, "defaultMenuPower3Wheel");
		assert(defaultMenuWheel[2][2]);

		defaultMenuText[0][0] = ituSceneFindWidget(&theScene, "defaultMenu1Text0");
		assert(defaultMenuText[0][0]);

		defaultMenuText[0][1] = ituSceneFindWidget(&theScene, "defaultMenu1Text1");
		assert(defaultMenuText[0][1]);

		defaultMenuText[1][0] = ituSceneFindWidget(&theScene, "defaultMenu2Text0");
		assert(defaultMenuText[1][0]);

		defaultMenuText[1][1] = ituSceneFindWidget(&theScene, "defaultMenu2Text1");
		assert(defaultMenuText[1][1]);

		defaultMenuText[2][0] = ituSceneFindWidget(&theScene, "defaultMenu3Text0");
		assert(defaultMenuText[2][0]);

		defaultMenuText[2][1] = ituSceneFindWidget(&theScene, "defaultMenu3Text1");
		assert(defaultMenuText[2][1]);

		defaultMenuProgramNameText = ituSceneFindWidget(&theScene, "defaultMenuProgramNameText");
		assert(defaultMenuProgramNameText);

		defaultMenuTrackBar[0] = ituSceneFindWidget(&theScene, "defaultMenuTotalTimeTrackBar");
		assert(defaultMenuTrackBar[0]);

		defaultMenuTrackBar[1] = ituSceneFindWidget(&theScene, "defaultMenuUpperTrackBar");
		assert(defaultMenuTrackBar[1]);

		defaultMenuTrackBar[2] = ituSceneFindWidget(&theScene, "defaultMenuBottomTrackBar");
		assert(defaultMenuTrackBar[2]);
	}
	for (i = 0; i < 8; i++)
	{
		surfacetmp = ituCreateSurface(imageIconNor[fConfig[i].sequence]->surf->width,
			imageIconNor[fConfig[i].sequence]->surf->height,
			imageIconNor[fConfig[i].sequence]->surf->pitch,
			imageIconNor[fConfig[i].sequence]->surf->format,
			NULL,
			0);
		ituBitBlt(surfacetmp, 0, 0, surfacetmp->width, surfacetmp->height, imageIconNor[fConfig[i].sequence]->surf, 0, 0);
		defaultMenuDisplayIcon[i]->surf = surfacetmp;
	}
	ituTextSetString(defaultMenuText[0][0], "First");
	ituTextSetString(defaultMenuText[1][0], "Second");
	ituTextSetString(defaultMenuText[2][0], "Third");
	for (i = 0; i < 3; i++)
		ituTextSetString(defaultMenuText[i][1], "MWO");
    return true;
}

bool DefaultMenuDisplayButtonOnPress(ITUWidget* widget, char* param)
{
	target = atoi(param);
	surfacetmp = ituCreateSurface(imageIconNor[target]->surf->width,
		imageIconNor[target]->surf->height,
		imageIconNor[target]->surf->pitch,
		imageIconNor[target]->surf->format,
		NULL,
		0);
	ituBitBlt(surfacetmp, 0, 0, surfacetmp->width, surfacetmp->height, imageIconNor[target]->surf, 0, 0);

	defaultMenuSelectIcon->surf = surfacetmp;

	ituTextSetString(defaultMenuProgramNameText, fConfig[target].name);

	return true;
}

bool DefaultMenuModButtonOnMouseUp(ITUWidget* widget, char* param)
{
	for (i = 0; i < 8; i++)
	{
		surfacetmp2 = ituCreateSurface(imageIconNor[i]->surf->width,
			imageIconNor[i]->surf->height,
			imageIconNor[i]->surf->pitch,
			imageIconNor[i]->surf->format,
			NULL,
			0);
		ituBitBlt(surfacetmp2, 0, 0, surfacetmp2->width, surfacetmp2->height, imageIconNor[i]->surf, 0, 0);
		defaultMenuListIcon[i]->surf = surfacetmp2;
	}
	return true;
}
bool DefaultMenuOnGotoSprite1(ITUWidget* widget, char* param)
{
	surfacetmp = ituCreateSurface(imageIconNor[target]->surf->width,
		imageIconNor[target]->surf->height,
		imageIconNor[target]->surf->pitch,
		imageIconNor[target]->surf->format,
		NULL,
		0);
	ituBitBlt(surfacetmp, 0, 0, surfacetmp->width, surfacetmp->height, imageIconNor[target]->surf, 0, 0);

	defaultMenuSelectIcon->surf = surfacetmp;

	ituTextSetString(defaultMenuProgramNameText, fConfig[target].name);

	return true;
}
bool DefaultMenuSelectConfirmButtonOnMouseUp(ITUWidget* widget, char* param)
{
	for (i = 0; i < 16; i++){
		if (ituCheckBoxIsChecked(defaultMenuListRadioBox[i])){
			fConfig[target].sequence = i;
			target = i;
			break;
		}
	}
	surfacetmp = ituCreateSurface(imageIconNor[target]->surf->width,
		imageIconNor[target]->surf->height,
		imageIconNor[target]->surf->pitch,
		imageIconNor[target]->surf->format,
		NULL,
		0);
	ituBitBlt(surfacetmp, 0, 0, surfacetmp->width, surfacetmp->height, imageIconNor[target]->surf, 0, 0);

	defaultMenuSelectIcon->surf = surfacetmp;
	return true;
}
bool DefaultMenuSettingConfirmButtonOnMouseUp(ITUWidget* widget, char* param)
{
	char msg[10],tmp[3];
	fConfig[target].cook_time[atoi(param)][0] = defaultMenuWheel[atoi(param)][0]->focusIndex;
	fConfig[target].cook_time[atoi(param)][1] = defaultMenuWheel[atoi(param)][1]->focusIndex;
	fConfig[target].cook_power[atoi(param)] = defaultMenuWheel[atoi(param)][2]->focusIndex;
	sprintf(tmp, "%02d", defaultMenuWheel[atoi(param)][0]->focusIndex);
	strcpy(msg, tmp);
	strcat(msg, ":");
	sprintf(tmp, "%02d", defaultMenuWheel[atoi(param)][1]->focusIndex);
	strcat(msg, tmp);
	ituTextSetString(defaultMenuText[atoi(param)][0], msg);

	sprintf(tmp, "%d", defaultMenuWheel[atoi(param)][2]->focusIndex * 10);
	strcpy(msg, tmp);
	strcat(msg, "%");
	ituTextSetString(defaultMenuText[atoi(param)][1], msg);
	return true;
}

bool DefaultMenuSetCookProcessButtonOnMouseUp(ITUWidget* widget, char* param)
{
	ituTrackBarSetValue(defaultMenuTrackBar[0], fConfig[target].total_time);
	ituTrackBarSetValue(defaultMenuTrackBar[1], fConfig[target].upper_temp);
	ituTrackBarSetValue(defaultMenuTrackBar[2], fConfig[target].bottom_temp);
	return true;
}

bool DefaultMenuSetProcessBackButtonOnMouseUp(ITUWidget* widget, char* param)
{
	fConfig[target].total_time = defaultMenuTrackBar[0]->value;
	fConfig[target].upper_temp = defaultMenuTrackBar[1]->value;
	fConfig[target].bottom_temp = defaultMenuTrackBar[2]->value;
	return true;
}

bool DefaultMenuSetCookingProcessConFirmButtonOnMouseUp(ITUWidget* widget, char* param)
{
	for (i = 0; i < 8; i++)
	{
		surfacetmp = ituCreateSurface(imageIconNor[fConfig[i].sequence]->surf->width,
			imageIconNor[fConfig[i].sequence]->surf->height,
			imageIconNor[fConfig[i].sequence]->surf->pitch,
			imageIconNor[fConfig[i].sequence]->surf->format,
			NULL,
			0);
		ituBitBlt(surfacetmp, 0, 0, surfacetmp->width, surfacetmp->height, imageIconNor[fConfig[i].sequence]->surf, 0, 0);
		defaultMenuDisplayIcon[i]->surf = surfacetmp;
	}
	ituTextSetString(defaultMenuText[0][0], "First");
	ituTextSetString(defaultMenuText[1][0], "Second");
	ituTextSetString(defaultMenuText[2][0], "Third");
	for (i = 0; i < 3; i++)
		ituTextSetString(defaultMenuText[i][1], "MWO");

	ConfigSave();

	return true;
}

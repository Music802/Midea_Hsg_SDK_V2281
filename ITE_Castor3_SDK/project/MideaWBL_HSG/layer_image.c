#include <assert.h>
#include "scene.h"
#define MENU_PATH (CFG_PUBLIC_DRIVE ":/media/menu/food")

ITUIcon *imageIconNor[16] = { 0 };
ITUIcon *imageIconSel[16] = { 0 };

bool ImageOnEnter(ITUWidget* widget, char* param)
{
	int i;
	char pathN[100], pathS[100], tmp[2];
	if (!imageIconNor[0])
	{
		imageIconNor[0] = ituSceneFindWidget(&theScene, "imageIcon00");
		assert(imageIconNor[0]);

		imageIconNor[1] = ituSceneFindWidget(&theScene, "imageIcon10");
		assert(imageIconNor[1]);

		imageIconNor[2] = ituSceneFindWidget(&theScene, "imageIcon20");
		assert(imageIconNor[2]);

		imageIconNor[3] = ituSceneFindWidget(&theScene, "imageIcon30");
		assert(imageIconNor[3]);

		imageIconNor[4] = ituSceneFindWidget(&theScene, "imageIcon40");
		assert(imageIconNor[4]);

		imageIconNor[5] = ituSceneFindWidget(&theScene, "imageIcon50");
		assert(imageIconNor[5]);

		imageIconNor[6] = ituSceneFindWidget(&theScene, "imageIcon60");
		assert(imageIconNor[6]);

		imageIconNor[7] = ituSceneFindWidget(&theScene, "imageIcon70");
		assert(imageIconNor[7]);

		imageIconNor[8] = ituSceneFindWidget(&theScene, "imageIcon80");
		assert(imageIconNor[8]);

		imageIconNor[9] = ituSceneFindWidget(&theScene, "imageIcon90");
		assert(imageIconNor[9]);

		imageIconNor[10] = ituSceneFindWidget(&theScene, "imageIcon100");
		assert(imageIconNor[10]);

		imageIconNor[11] = ituSceneFindWidget(&theScene, "imageIcon110");
		assert(imageIconNor[11]);

		imageIconNor[12] = ituSceneFindWidget(&theScene, "imageIcon120");
		assert(imageIconNor[12]);

		imageIconNor[13] = ituSceneFindWidget(&theScene, "imageIcon130");
		assert(imageIconNor[13]);

		imageIconNor[14] = ituSceneFindWidget(&theScene, "imageIcon140");
		assert(imageIconNor[14]);

		imageIconNor[15] = ituSceneFindWidget(&theScene, "imageIcon150");
		assert(imageIconNor[15]);


		imageIconSel[0] = ituSceneFindWidget(&theScene, "imageIcon01");
		assert(imageIconSel[0]);

		imageIconSel[1] = ituSceneFindWidget(&theScene, "imageIcon11");
		assert(imageIconSel[1]);

		imageIconSel[2] = ituSceneFindWidget(&theScene, "imageIcon21");
		assert(imageIconSel[2]);

		imageIconSel[3] = ituSceneFindWidget(&theScene, "imageIcon31");
		assert(imageIconSel[3]);

		imageIconSel[4] = ituSceneFindWidget(&theScene, "imageIcon41");
		assert(imageIconSel[4]);

		imageIconSel[5] = ituSceneFindWidget(&theScene, "imageIcon51");
		assert(imageIconSel[5]);

		imageIconSel[6] = ituSceneFindWidget(&theScene, "imageIcon61");
		assert(imageIconSel[6]);

		imageIconSel[7] = ituSceneFindWidget(&theScene, "imageIcon71");
		assert(imageIconSel[7]);

		imageIconSel[8] = ituSceneFindWidget(&theScene, "imageIcon81");
		assert(imageIconSel[8]);

		imageIconSel[9] = ituSceneFindWidget(&theScene, "imageIcon91");
		assert(imageIconSel[9]);

		imageIconSel[10] = ituSceneFindWidget(&theScene, "imageIcon101");
		assert(imageIconSel[10]);

		imageIconSel[11] = ituSceneFindWidget(&theScene, "imageIcon111");
		assert(imageIconSel[11]);

		imageIconSel[12] = ituSceneFindWidget(&theScene, "imageIcon121");
		assert(imageIconSel[12]);

		imageIconSel[13] = ituSceneFindWidget(&theScene, "imageIcon131");
		assert(imageIconSel[13]);

		imageIconSel[14] = ituSceneFindWidget(&theScene, "imageIcon141");
		assert(imageIconSel[14]);

		imageIconSel[15] = ituSceneFindWidget(&theScene, "imageIcon151");
		assert(imageIconSel[15]);
	}

	for (i = 0; i < 8; i++)
	{
		sprintf(tmp, "%d", i);
		strcpy(pathN, MENU_PATH);
		strcat(pathN, tmp);
		strcpy(pathS, pathN);
		strcat(pathN, "/food.png");
		strcat(pathS, "/food_sel.png");
		ituIconLoadPngFile(imageIconNor[i], pathN);
		ituIconLoadPngFile(imageIconSel[i], pathS);
	}

    return true;
}


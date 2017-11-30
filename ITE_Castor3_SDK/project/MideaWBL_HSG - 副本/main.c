#include <unistd.h>
#include "SDL/SDL.h"
#include "ite/itp.h"
#include "project.h"
#include "scene.h"

//kenny
#if CFG_MIDEA_HSG_SPECIAL_SET
void PowerEnableAll(void)
{
	 //USB1 Power Enable
    ithGpioSetMode(CFG_GPIO_WIFI_SWITCH, ITH_GPIO_MODE0);
   ithGpioSetOut(CFG_GPIO_WIFI_SWITCH);
   ithGpioSet(CFG_GPIO_WIFI_SWITCH);	   
   ithGpioEnable(CFG_GPIO_WIFI_SWITCH);
   //Speaker
    ithGpioSetMode(CFG_GPIO_AUDIO_AMP_POWER, ITH_GPIO_MODE0);
   ithGpioSetOut(CFG_GPIO_AUDIO_AMP_POWER);
   ithGpioSet(CFG_GPIO_AUDIO_AMP_POWER);	   
   ithGpioEnable(CFG_GPIO_AUDIO_AMP_POWER);   
   printf("Power Enable All\r\n");
}
#endif
int SDL_main(int argc, char *argv[])
{
    int ret = 0;
#if CFG_MIDEA_HSG_SPECIAL_SET
	PowerEnableAll();
#endif
	
#ifdef CFG_CHECK_FILES_CRC_ON_BOOTING
    int restryCount = 0;

    BackupInit();
retry_backup:
    ret = UpgradeInit();
    if (ret)
    {
        if (++restryCount > 2)
        {
            printf("cannot recover from backup....\n");
            goto end;
        }
        BackupRestore();
        goto retry_backup;
    }
    BackupSyncFile();
#else
    ret = UpgradeInit();
    if (ret)
        goto end;
#endif

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        printf("Couldn't initialize SDL: %s\n", SDL_GetError());
    
    ConfigInit();
#ifdef CFG_NET_ENABLE
    NetworkInit();
#endif    
    SceneInit();
    AudioInit();
    SceneLoad();
    ret = SceneRun();

    SceneExit();
    AudioExit();
    ConfigExit();
    SDL_Quit();

end:
    ret = UpgradeProcess(ret);
    exit(ret);
    return ret;
}

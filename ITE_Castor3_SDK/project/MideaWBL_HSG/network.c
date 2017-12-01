#include <sys/ioctl.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>
#include "lwip/ip.h"
#include "lwip/dns.h"
#include "ite/itp.h"
#include "iniparser/iniparser.h"
#include "ctrlboard.h"
#include "wifiMgr.h"
#ifdef CFG_NET_FTP_SERVER
#include <unistd.h>
#include "ftpd.h"
#endif

#include "curl/curl.h"
#include "yajl/yajl_tree.h"

#define OTA_TEST_OLNY

#define DHCP_TIMEOUT_MSEC (60 * 1000) //60sec

static struct timeval tvStart = {0, 0}, tvEnd = {0, 0};
static bool networkIsReady, networkToReset;
static int networkSocket;

#ifdef CFG_NET_WIFI
static WIFI_MGR_SETTING gWifiSetting;
static bool wifi_dongle_hotplug, need_reinit_wifimgr;
static int gInit =0; // wifi init
#endif

//////////
/*Kenny porting for Nick's Patch*/
//  Get Json Data
#ifdef OTA_TEST_OLNY
#define HTTP_OTA_URL_TEST_ADDRESS  "http://iot2.midea.com.cn:8080/ota/v1/pro2app/version/by/app/get?sn=EGE04GH7-S00L00&version=0.9.9"
#else
#define HTTP_OTA_URL_STR_SN "http://iot2.midea.com.cn:8080/ota/v1/pro2app/version/by/app/get?sn="
#define HTTP_OTA_URL_STR_VER "&version="
#endif
struct MemoryStruct {
  char *memory;
  size_t size;
};

static unsigned char urlOTA[2048]; /*进行OTA请求的URL*/
static unsigned char fileData[2048];
// keep ota url
static unsigned char urlPKG[2048]; /*OTA请求后，返回pkg升级的URL*/

// return 1, get ota url, return 0, no need upgrade
int ParseConfig(char* pData,int nSize)
{
    size_t rd;
    yajl_val node;
    char errbuf[1024];
    char tmpbuf[256];
    FILE* f;
    const char * path[] = { "errorCode", (const char *) 0 };  
    const char * path1[] = { "result","url", (const char *) 0 };
    yajl_val v,v1 ; 
    int *errorCode;
    /* null plug buffers */ 
    fileData[0] = errbuf[0] = 0; 
    
    memset(urlPKG,0,sizeof(urlPKG));
    /* read the entire data */
    memcpy(fileData,pData,nSize);
    
    /* we have the whole config file in memory.  let's parse it ... */
    node = yajl_tree_parse((const char *) fileData, errbuf, sizeof(errbuf)); 
    /* parse error handling */    
    if (node == NULL) 
    {     
        printf("parse_error: ");
        if (strlen(errbuf)) 
            printf(" %s", errbuf); 
        else 
            printf("unknown error"); 
        
        printf(stderr, "\n");   
        return 1;
    } 
    
    /* ... and extract a nested value from the config file */
    v = yajl_tree_get(node, path, yajl_t_string); 
    if (v)  
        printf("%s: %s\n", path[0], YAJL_GET_STRING(v));
    else 
        printf("no such node: %s/%s\n", path[0], path[1]);
    
    errorCode = YAJL_GET_NUMBER(v); 
    strcpy(tmpbuf,YAJL_GET_STRING(v));
    if (tmpbuf[0]=='0')
    { 
        printf("error code 0 ,get url\n"); 
        v1 = yajl_tree_get(node, path1, yajl_t_string); 
        if (v1) {
           strcpy(urlPKG,YAJL_GET_STRING(v1));            
            printf("%s: %s\n", path1[1], urlPKG); 
        }
        yajl_tree_free(node); 

        return 1;
    } 
    else
    {
        //printf("error code %d %d %d\n",YAJL_GET_NUMBER(v),YAJL_IS_NUMBER(v),v->type); 
        yajl_tree_free(node); 
        
    } 
    return 0;
}

 
static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
 
  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL) {
    /* out of memory! */ 
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }
 
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
 
  return realsize;
}


// return 1, get ota url, return 0, no need upgrade  
static int OTACheck(void)
{
  CURL *curl_handle;
  CURLcode res;
  int nResult = 0;
 
  struct MemoryStruct chunk;
 
  chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */ 
  chunk.size = 0;    /* no data at this point */ 
 
  curl_global_init(CURL_GLOBAL_ALL);
 
  /* init the curl session */ 
  curl_handle = curl_easy_init();
 
  /* specify URL to get */ 
  curl_easy_setopt(curl_handle, CURLOPT_URL, urlOTA);
 
  /* send all data to this function  */ 
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
 
  /* we pass our 'chunk' struct to the callback function */ 
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
 
  /* some servers don't like requests that are made without a user-agent
     field, so we provide one */ 
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
 
  /* get it! */ 
  res = curl_easy_perform(curl_handle);
 
  /* check for errors */ 
  if(res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));
  }
  else {
    /*
     * Now, our chunk.memory points to a memory block that is chunk.size
     * bytes big and contains the remote file.
     *
     * Do something nice with it!
     */ 
 
    printf("%lu bytes retrieved %s \n", (long)chunk.size,chunk.memory);
    nResult = ParseConfig(chunk.memory,chunk.size);
  }
 
  /* cleanup curl stuff */ 
  curl_easy_cleanup(curl_handle);
 
  free(chunk.memory);
 
  /* we're done with libcurl, so clean it up */ 
  curl_global_cleanup();
 
  return nResult;
}
/*
检查SERVER上是否有新版本
返回1：有新版本，用urlPKG进行升级
返回0：当前软件为最新版本
*/
static int UpgradeSetOTAUrl(char *sn,char *version)
{
	int nResult = 0;
    memset(urlOTA,0,sizeof(urlOTA));
	#ifdef OTA_TEST_OLNY
	memcpy(urlOTA,HTTP_OTA_URL_TEST_ADDRESS,sizeof(urlOTA));
	#else
	 sprintf(urlOTA,"HTTP_OTA_URL_STR_SN%sHTTP_OTA_URL_STR_VER%s",sn,version);
	#endif
	 printf("OTA Upgrade  URL>>%s\r\n",urlOTA);
	    nResult = OTACheck();
    if (nResult) {
        printf("OTA New Verison URL>> %s \n",urlPKG);
    } else {
        printf("OTA Latest Version  %s\n",urlPKG);
   }
}
///////////////////////////////////////////

static void ResetEthernet(void)
{
    ITPEthernetSetting setting;
    ITPEthernetInfo info;
    unsigned long mscnt = 0;
    char buf[16], *saveptr;

    memset(&setting, 0, sizeof (ITPEthernetSetting));

    setting.index = 0;

    // dhcp
    setting.dhcp = theConfig.dhcp;

    // autoip
    setting.autoip = 0;

    // ipaddr
    strcpy(buf, theConfig.ipaddr);
    setting.ipaddr[0] = atoi(strtok_r(buf, ".", &saveptr));
    setting.ipaddr[1] = atoi(strtok_r(NULL, ".", &saveptr));
    setting.ipaddr[2] = atoi(strtok_r(NULL, ".", &saveptr));
    setting.ipaddr[3] = atoi(strtok_r(NULL, " ", &saveptr));

    // netmask
    strcpy(buf, theConfig.netmask);
    setting.netmask[0] = atoi(strtok_r(buf, ".", &saveptr));
    setting.netmask[1] = atoi(strtok_r(NULL, ".", &saveptr));
    setting.netmask[2] = atoi(strtok_r(NULL, ".", &saveptr));
    setting.netmask[3] = atoi(strtok_r(NULL, " ", &saveptr));

    // gateway
    strcpy(buf, theConfig.gw);
    setting.gw[0] = atoi(strtok_r(buf, ".", &saveptr));
    setting.gw[1] = atoi(strtok_r(NULL, ".", &saveptr));
    setting.gw[2] = atoi(strtok_r(NULL, ".", &saveptr));
    setting.gw[3] = atoi(strtok_r(NULL, " ", &saveptr));

    ioctl(ITP_DEVICE_ETHERNET, ITP_IOCTL_RESET, &setting);

    printf("Wait ethernet cable to plugin");
    while (!ioctl(ITP_DEVICE_ETHERNET, ITP_IOCTL_IS_CONNECTED, NULL))
    {
        sleep(1);
        putchar('.');
        fflush(stdout);
    }

    printf("\nWait DHCP settings");
    while (!ioctl(ITP_DEVICE_ETHERNET, ITP_IOCTL_IS_AVAIL, NULL))
    {
        usleep(100000);
        mscnt += 100;

        putchar('.');
        fflush(stdout);

        if (mscnt >= DHCP_TIMEOUT_MSEC)
        {
            printf("\nDHCP timeout, use default settings\n");
            setting.dhcp = setting.autoip = 0;
            ioctl(ITP_DEVICE_ETHERNET, ITP_IOCTL_RESET, &setting);
            break;
        }
    }
    puts("");

    if (ioctl(ITP_DEVICE_ETHERNET, ITP_IOCTL_IS_AVAIL, NULL))
    {
        char* ip;

        info.index = 0;
        ioctl(ITP_DEVICE_ETHERNET, ITP_IOCTL_GET_INFO, &info);
        ip = ipaddr_ntoa((const ip_addr_t*)&info.address);

        printf("IP address: %s\n", ip);

        networkIsReady = true;
    }
}


#ifdef CFG_NET_WIFI
static ResetWifi()
{
//    ITPEthernetSetting setting;
    char buf[16], *saveptr;

    memset(&gWifiSetting.setting, 0, sizeof (ITPEthernetSetting));

    gWifiSetting.setting.index = 0;

    // dhcp
    gWifiSetting.setting.dhcp = 1;

    // autoip
    gWifiSetting.setting.autoip = 0;

    // ipaddr
    strcpy(buf, theConfig.ipaddr);
    gWifiSetting.setting.ipaddr[0] = atoi(strtok_r(buf, ".", &saveptr));
    gWifiSetting.setting.ipaddr[1] = atoi(strtok_r(NULL, ".", &saveptr));
    gWifiSetting.setting.ipaddr[2] = atoi(strtok_r(NULL, ".", &saveptr));
    gWifiSetting.setting.ipaddr[3] = atoi(strtok_r(NULL, " ", &saveptr));

    // netmask
    strcpy(buf, theConfig.netmask);
    gWifiSetting.setting.netmask[0] = atoi(strtok_r(buf, ".", &saveptr));
    gWifiSetting.setting.netmask[1] = atoi(strtok_r(NULL, ".", &saveptr));
    gWifiSetting.setting.netmask[2] = atoi(strtok_r(NULL, ".", &saveptr));
    gWifiSetting.setting.netmask[3] = atoi(strtok_r(NULL, " ", &saveptr));

    // gateway
    strcpy(buf, theConfig.gw);
    gWifiSetting.setting.gw[0] = atoi(strtok_r(buf, ".", &saveptr));
    gWifiSetting.setting.gw[1] = atoi(strtok_r(NULL, ".", &saveptr));
    gWifiSetting.setting.gw[2] = atoi(strtok_r(NULL, ".", &saveptr));
    gWifiSetting.setting.gw[3] = atoi(strtok_r(NULL, " ", &saveptr));
}

static int wifiCallbackFucntion(int nState)
{
    switch (nState)
    {
        case WIFIMGR_STATE_CALLBACK_CONNECTION_FINISH:
            printf("[Indoor]WifiCallback connection finish \n");
            WebServerInit();

#ifdef CFG_NET_FTP_SERVER
		    ftpd_setlogin(theConfig.user_id, theConfig.user_password);
		    ftpd_init();
#endif
        break;

        case WIFIMGR_STATE_CALLBACK_CLIENT_MODE_DISCONNECT_30S:
            printf("[Indoor]WifiCallback connection disconnect 30s \n");
        break;

        case WIFIMGR_STATE_CALLBACK_CLIENT_MODE_RECONNECTION:
            printf("[Indoor]WifiCallback connection reconnection \n");
        break;

        case WIFIMGR_STATE_CALLBACK_CLIENT_MODE_TEMP_DISCONNECT:
            printf("[Indoor]WifiCallback connection temp disconnect \n");
        break;

        case WIFIMGR_STATE_CALLBACK_CLIENT_MODE_CONNECTING_FAIL:
            printf("[Indoor]WifiCallback connecting fail, please check ssid,password,secmode \n");
        break;

        default:
            printf("[Indoor]WifiCallback unknown %d state  \n",nState);
        break;
    }
}

void NetworkWifiModeSwitch(void)
{
	int ret;

	ret = WifiMgr_Switch_ClientSoftAP_Mode(gWifiSetting);
}
#endif

static void* NetworkTask(void* arg)
{
#ifdef CFG_NET_WIFI
    int nTemp;
#else
    ResetEthernet();
#endif

#if defined(CFG_NET_FTP_SERVER) && !defined(CFG_NET_WIFI)
    ftpd_setlogin(theConfig.user_id, theConfig.user_password);
    ftpd_init();
#endif

    for (;;)
    {
#ifdef CFG_NET_WIFI
        gettimeofday(&tvEnd, NULL);

        nTemp = itpTimevalDiff(&tvStart, &tvEnd);
        if (nTemp>5000 && gInit == 0){
            printf("[%s] Init wifimgr \n", __FUNCTION__);

            while(!ioctl(ITP_DEVICE_WIFI, ITP_IOCTL_IS_DEVICE_READY, NULL)){
                sleep(1);
            }

			WifiMgr_clientMode_switch(theConfig.wifi_status);

            if (theConfig.wifi_mode == WIFI_CLIENT){
				ioctl(ITP_DEVICE_WIFI, ITP_IOCTL_ENABLE, NULL); //determine wifi client mode
                nTemp = wifiMgr_init(WIFIMGR_MODE_CLIENT, 0, gWifiSetting);
			}else if (theConfig.wifi_mode == WIFI_SOFTAP){
				ioctl(ITP_DEVICE_WIFI, ITP_IOCTL_WIFIAP_ENABLE, NULL); //determine wifi softAP mode
				nTemp = wifiMgr_init(WIFIMGR_MODE_SOFTAP, 0, gWifiSetting);
			}

            gInit = 1;
        } else if (gInit == 1){
            networkIsReady = wifiMgr_is_wifi_available(&nTemp);
            networkIsReady = (bool)nTemp;

    	    /* Run Wifi dongle hot plug-in process, in order to re-init wifimgr. */
			//Step 1: check dongle is plug-in or not
            wifi_dongle_hotplug = ioctl(ITP_DEVICE_WIFI, ITP_IOCTL_IS_DEVICE_READY, NULL);

			//Step 2: If not plug-in then reinit flag set true
			if (!wifi_dongle_hotplug)
				need_reinit_wifimgr = true;

			//Step 3: If dongle hot plug-in and reinit flag is true, do wifimgr re-init
			if (wifi_dongle_hotplug && need_reinit_wifimgr){
				wifiMgr_init(WIFIMGR_MODE_CLIENT, 0, gWifiSetting);
				need_reinit_wifimgr = false;
			}
        }
#else
        networkIsReady = ioctl(ITP_DEVICE_ETHERNET, ITP_IOCTL_IS_CONNECTED, NULL);
#endif

        if (networkToReset)
        {
#ifndef CFG_NET_WIFI
            ResetEthernet();
#endif
            networkToReset = false;
        }
        sleep(1);
    }
    return NULL;
}

void NetworkInit(void)
{
    pthread_t task;

    networkIsReady = false;
    networkToReset = false;
    networkSocket = -1;
#ifdef CFG_NET_WIFI
    snprintf(gWifiSetting.ssid , WIFI_SSID_MAXLEN, theConfig.ssid);
    snprintf(gWifiSetting.password, WIFI_PASSWORD_MAXLEN, theConfig.password);
    snprintf(gWifiSetting.secumode, WIFI_SECUMODE_MAXLEN, theConfig.secumode);
    gWifiSetting.wifiCallback = wifiCallbackFucntion;
    ResetWifi();

    gettimeofday(&tvStart, NULL);
#endif

    pthread_create(&task, NULL, NetworkTask, NULL);
}

bool NetworkIsReady(void)
{
    return networkIsReady;
}

void NetworkReset(void)
{
    networkToReset  = true;
}

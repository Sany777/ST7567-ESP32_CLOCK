#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H


#ifdef __cplusplus
extern "C" {
#endif



int connect_sta(const char *ssid, const char *pwd);
int start_ap();
int wifi_init(void) ;
void wifi_stop();




#ifdef __cplusplus
}
#endif




#endif
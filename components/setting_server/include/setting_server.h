#ifndef SETTING_SERVER_H
#define SETTING_SERVER_H



void init_dns_server_task();
int init_server(char *server_buf);
int deinit_server();
void deinit_dns_server();

#endif
#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>

void initWebServer();
bool checkAuthentication(AsyncWebServerRequest* request);

#endif

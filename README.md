# esp8266-websocketclient

ESP8266 WebsocketClient

Un simple cliente basado en eventos de websocket para el ESP8266, con soporte de los protocolos ws y wss.

Ejemplo:

```
#include <Arduino.h>
#include "WebSocketClient.h"
#include "ESP8266WiFi.h"

WebSocketClient ws(true);

void wListener(char *payload, size_t length)
{
	for (int i = 0; i < length; i++)
	{
		Serial.print(payload[i]);
	}
	Serial.println();

	ws.send("Response");
}

void setup()
{
	Serial.begin(115200);
	WiFi.begin("MyWifi", "secret");

	Serial.print("Connecting");
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		Serial.print(".");
	}
	ws.begin("echo.websocket.org", "/", 443);
	ws.setTimeOut(15000);
	ws.setEvent(wListener);
}

void loop()
{
	ws.loop();
}

```

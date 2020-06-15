#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H
#include <Arduino.h>
#include <WiFiClient.h>
#include <Ticker.h>

#define WEBSOCKETS_YIELD() delay(0);

#define MAX_MSG_SIZE 128
#define TEXT 1
#define PING 2
#define PONG 3

class WebSocketClient
{

public:
    //std::function<void(void)>
    //typedef std::function<void(WStype_t type, uint8_t *payload, size_t length)> WebSocketClientEvent;
    typedef void (*WSEvent)(char *payload, size_t length);
    WebSocketClient(bool secure = false);

    ~WebSocketClient();

    void begin(String host, String path, int port);

    bool connect(String host, String path, int port);
    bool connect();
    bool isConnected();
    bool getMessage(String &message);
    bool setEvent(WSEvent call_back);

    void setTimeOut(unsigned int ms);
    void disconnect();
    void send(const String &str);
    void send(const String &str, uint8_t type);
    void setAuthorizationHeader(String header);
    void loop();

private:
    int timedRead();

    void write(unsigned char data);

    void write(const char *str);
    void sendPONG();

    String generateKey();

    WiFiClient *client;

    String authorizationHeader = "";
    String server_url;
    String path_url;
    String cKey = "";
    String pongPayload = "";

    char message[MAX_MSG_SIZE];

    int server_port;

    bool websocketEstablished = false;
    bool bPING = false;

    unsigned long _reconnectInterval = 0;
    unsigned long _lastConnectionFail = 0;

    // Listener structure and corresponding array
    struct ListenerItem
    {
        WSEvent callback; // The listener function
        int eventCode;    // The event code
        boolean enabled;  // Each listener can be enabled or disabled
    };
    ListenerItem mListeners;
    //[kMaxListeners];
    String base64_encode(uint8_t *data, size_t len);

protected:
    WSEvent _callback_function = nullptr;
};

#endif //WEBSOCKETCLIENT_H
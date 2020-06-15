#define DEBU 6

#include "WebSocketClient.h"
#include <WiFiClientSecure.h>

#define WS_FIN 0x80

#define WS_OPCODE_TEXT 0x01
#define WS_OPCODE_BINARY 0x02
#define WS_OPCODE_PING 0x09
#define WS_OPCODE_PONG 0x0A

#define WS_MASK 0x80
#define WS_SIZE16 126

#ifdef DEBU
#define DEBUG_WS Serial.println
#else
#define DEBUG_WS(MSG)
#endif /*DEBUG*/

void WebSocketClient::begin(String server_url, String path_url, int port)
{
    this->server_url = server_url;
    this->path_url = path_url;
    this->server_port = port;
    this->websocketEstablished = false;
}
WebSocketClient::WebSocketClient(bool secure)
{
    if (secure)
        this->client = new WiFiClientSecure;
    else
        this->client = new WiFiClient;
}

WebSocketClient::~WebSocketClient()
{
    delete this->client;
}

void WebSocketClient::setAuthorizationHeader(String header)
{
    this->authorizationHeader = header;
}

const char *B64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

const int B64index[256] =
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 63, 62, 62, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0, 0, 0, 0, 0, 0,
        0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0, 0, 0, 0, 63,
        0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51};

String WebSocketClient::base64_encode(uint8_t *data, size_t len2)
{
    int len = 16;
    //int n = len;
    int result_len = 24; //(n + 2 - ((n + 2) % 3)) / 3 * 4;
    String result = "01234567890123456789234";
    char *p = (char *)data, *str = &result[0];
    size_t j = 0, pad = len % 3;
    const size_t last = len - pad;

    for (size_t i = 0; i < last; i += 3)
    {
        int n = int(p[i]) << 16 | int(p[i + 1]) << 8 | p[i + 2];
        str[j++] = B64chars[n >> 18];
        str[j++] = B64chars[n >> 12 & 0x3F];
        str[j++] = B64chars[n >> 6 & 0x3F];
        str[j++] = B64chars[n & 0x3F];
    }
    if (pad) /// set padding
    {
        int n = --pad ? int(p[last]) << 8 | p[last + 1] : p[last];
        str[j++] = B64chars[pad ? n >> 10 & 0x3F : n >> 2];
        str[j++] = B64chars[pad ? n >> 4 & 0x03F : n << 4 & 0x3F];
        str[j++] = pad ? B64chars[n << 2 & 0x3F] : '=';
    }
    return result;
}

String WebSocketClient::generateKey()
{
    // String key = "";
    // for (int i = 0; i < 23; ++i)
    // {
    //     int r = random(0, 3);
    //     if (r == 0)
    //         key += (char)random(48, 58);
    //     else if (r == 1)
    //         key += (char)random(65, 91);
    //     else if (r == 2)
    //         key += (char)random(97, 128);
    // }

    uint8_t randomKey[16] = {0};

    for (uint8_t i = 0; i < sizeof(randomKey); i++)
    {
        randomKey[i] = random(0xFF);
    }

    this->cKey = base64_encode(&randomKey[0], 16);

    return this->cKey;
}
void WebSocketClient::write(unsigned char data)
{
    if (client->connected())
        client->write(data);
    else
    {
        this->_lastConnectionFail = millis();
    }
}

void WebSocketClient::write(const char *data)
{
    if (client->connected())
        client->write(data);
    else
    {
        this->_lastConnectionFail = millis();
    }
}

bool WebSocketClient::connect()
{
    return connect(server_url.c_str(), path_url.c_str(), server_port);
}
bool WebSocketClient::connect(String host, String path, int port)
{
    if (!client->connect(host.c_str(), port))
        return false;

    // send handshake
    String handshake = "GET " + path + " HTTP/1.1\r\n"
                                       "Host: " +
                       host + "\r\n"
                              "Connection: Upgrade\r\n"
                              "Upgrade: websocket\r\n"
                              "Sec-WebSocket-Version: 13\r\n"
                              "Sec-WebSocket-Key: " +
                       generateKey() + "=\r\n";

    if (authorizationHeader != "")
        handshake += "Authorization: " + authorizationHeader + "\r\n";

    handshake += "\r\n";

    DEBUG_WS("[WS] sending handshake");
    DEBUG_WS(handshake);

    write(handshake.c_str());

    // success criteria
    bool hasCorrectStatus = false;
    bool isUpgrade = false;
    bool isWebsocket = false;
    bool hasAcceptedKey = false;

    bool endOfResponse = false;

    // handle response headers
    String s;
    while (!endOfResponse && (s = client->readStringUntil('\n')).length() > 0)
    {
        DEBUG_WS("[WS][RX] " + s);
        // HTTP Status
        if (s.indexOf("HTTP/") != -1)
        {
            auto status = s.substring(9, 12);
            if (status == "101")
                hasCorrectStatus = true;
            else
            {
                DEBUG_WS("[WS] wrong status: " + status);
                return false;
            }
        }
        // Headers
        else if (s.indexOf(":") != -1)
        {
            auto col = s.indexOf(":");
            auto key = s.substring(0, col);
            auto value = s.substring(col + 2, s.length() - 1);

            if (key == "Connection" && (value == "Upgrade" || value == "upgrade"))
                isUpgrade = true;

            else if (key == "Sec-WebSocket-Accept")
                hasAcceptedKey = true;

            else if (key == "Upgrade" && value == "websocket")
                isWebsocket = true;
        }

        else if (s == "\r")
            endOfResponse = true;
    }

    bool success = hasCorrectStatus && isUpgrade && isWebsocket && hasAcceptedKey;

    if (success)
    {
        DEBUG_WS("[WS] sucessfully connected");
        this->websocketEstablished = true;
    }
    else
    {
        DEBUG_WS("[WS] could not connect");
        this->disconnect();
    }

    return success;
}

bool WebSocketClient::isConnected()
{
    return this->websocketEstablished && client->connected();
}

void WebSocketClient::disconnect()
{
    client->stop();
    this->websocketEstablished = false;
}
void WebSocketClient::send(const String &str)
{
    send(str, TEXT);
}
void WebSocketClient::send(const String &str, uint8_t type)
{
    DEBUG_WS("[WS] sending: " + str);
    if (!client->connected())
    {
        DEBUG_WS("[WS] not connected...");
        return;
    }

    // 1. send fin and type
    if (type == TEXT)
        write(WS_FIN | WS_OPCODE_TEXT);
    else if (type == PING)
        write(WS_FIN | WS_OPCODE_PING);
    else if (type == PONG)
        write(WS_FIN | WS_OPCODE_PONG);
    else
        write(WS_FIN | WS_OPCODE_TEXT);

    // 2. send length
    int size = str.length();
    if (size > 125)
    {
        write(WS_MASK | WS_SIZE16);
        write((uint8_t)(size >> 8));
        write((uint8_t)(size & 0xFF));
    }
    else
    {
        write(WS_MASK | (uint8_t)size);
    }

    // 3. send mask
    uint8_t mask[4];
    mask[0] = random(0, 256);
    mask[1] = random(0, 256);
    mask[2] = random(0, 256);
    mask[3] = random(0, 256);

    write(mask[0]);
    write(mask[1]);
    write(mask[2]);
    write(mask[3]);

    //4. send masked data
    for (int i = 0; i < size; ++i)
    {
        write(str[i] ^ mask[i % 4]);
    }
}

void WebSocketClient::sendPONG()
{
    DEBUG_WS("[WS] Send pong command");
    this->bPING = false;
    send(this->pongPayload, PONG);
}

int WebSocketClient::timedRead()
{
    while (!client->available())
    {
        delay(20);
    }
    return client->read();
}

bool WebSocketClient::getMessage(String &message)
{
    if (!client->connected())
    {
        return false;
    }

    // 1. read type and fin
    unsigned int msgtype = timedRead();
    if (!client->connected())
    {
        DEBUG_WS("Step 1");
        return false;
    }

    // 2. read length and check if masked
    int length = timedRead();
    bool hasMask = false;
    if (length & WS_MASK)
    {
        hasMask = true;
        length = length & ~WS_MASK;
    }

    if (length == WS_SIZE16)
    {
        length = timedRead() << 8;
        length |= timedRead();
    }

    // 3. read mask
    if (hasMask)
    {
        uint8_t mask[4];
        mask[0] = timedRead();
        mask[1] = timedRead();
        mask[2] = timedRead();
        mask[3] = timedRead();

        // 4. read message (masked)
        message = "";
        for (int i = 0; i < length; ++i)
        {
            message += (char)(timedRead() ^ mask[i % 4]);
        }
    }
    else
    {
        // 4. read message (unmasked)
        message = "";
        for (int i = 0; i < length; ++i)
        {
            message += (char)timedRead();
        }
    }

    return true;
}

void WebSocketClient::loop()
{
    WEBSOCKETS_YIELD();
    if (!isConnected())
    {
        DEBUG_WS("No conectado");
        // do not flood the server
        if ((millis() - _lastConnectionFail) < _reconnectInterval)
        {
            return;
        }

        if (!connect())
        {
            DEBUG_WS("[WS-Client] Conexion fallida gil!");
            _lastConnectionFail = millis();
        }
        else
        {
            _lastConnectionFail = 0;
        }
    }
    else
    {

        // 1. read type and fin
        if (client->available() > 0)
        {
            unsigned char msgtype = client->read();
            //El opcode contiene el tipo de función recibida
            unsigned char opcode = msgtype & 0x0F;
            //0x09 -> Ping function
            //0x0A -> Pong function
            if (opcode == WS_OPCODE_PING)
                this->bPING = true;
            if (!client->connected())
            {
                DEBUG_WS("Step 1");
                return;
            }

            DEBUG_WS("[WS] msgtype:");
            DEBUG_WS(msgtype);

            // 2. read length and check if masked
            int length = client->read();
            DEBUG_WS("[WS] length:");
            DEBUG_WS(length);
            bool hasMask = false;
            if (length & WS_MASK)
            {
                hasMask = true;
                length = length & ~WS_MASK;
            }

            if (length == WS_SIZE16)
            {
                length = client->read() << 8;
                length |= client->read();
            }

            // 3. read mask
            if (hasMask)
            {
                uint8_t mask[4];
                mask[0] = client->read();
                mask[1] = client->read();
                mask[2] = client->read();
                mask[3] = client->read();

                // 4. read message (masked)
                //Vaciar cadena de msessage
                for (int i = 0; i < length && i < MAX_MSG_SIZE; ++i)
                {
                    message[i] = (char)(client->read() ^ mask[i % 4]);
                }
            }
            else
            {
                // 4. read message (unmasked)
                //Vaciar cadena de msessage
                for (int i = 0; i < length && i < MAX_MSG_SIZE; ++i)
                {
                    message[i] = (char)client->read();
                }
            }
            Serial.println("MSG:");
            for (int i = 0; i < length && i < MAX_MSG_SIZE; ++i)
            {

                Serial.print(message[i], HEX);
                Serial.print(" ");
            }
            Serial.println();

            if (this->bPING)
            {
                this->pongPayload = "";
                for (int i = 0; i < length && i < MAX_MSG_SIZE; ++i)
                {
                    this->pongPayload += message[i];
                }
            }
            else if (mListeners.callback != 0)
            {
                //Con esta linea de comando se ejecuta lo que vos queres, si queres matar a sid, tenes que ejecutar sid.exe
                (*mListeners.callback)(message, length);
            }
        }

        if (this->bPING)
        {
            this->sendPONG();
        }
    }
}

bool WebSocketClient::setEvent(WSEvent listener)
{
    // EVTMGR_DEBUG_PRINT("addListener() enter ")
    // EVTMGR_DEBUG_PRINT(eventCode)
    // EVTMGR_DEBUG_PRINT(", ")
    // EVTMGR_DEBUG_PRINTLN_PTR(listener)

    // Argument check
    if (!listener)
    {
        return false;
    }

    // // Check for full dispatch table
    // if (isFull())
    // {
    //     EVTMGR_DEBUG_PRINTLN("addListener() list full")
    //     return false;
    // }

    // mListeners[mNumListeners].callback = listener;
    // mListeners[mNumListeners].eventCode = eventCode;
    // mListeners[mNumListeners].enabled = true;
    // mNumListeners++;

    mListeners.callback = listener;

    // EVTMGR_DEBUG_PRINTLN("addListener() listener added")

    return true;
}

void WebSocketClient::setTimeOut(unsigned int ms)
{
    _reconnectInterval = ms;
}
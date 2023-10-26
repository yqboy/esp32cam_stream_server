#include <Arduino.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <HTTPClient.h>
#include <AsyncUDP.h>
#include <LittleFS.h>
#include "soc/rtc_wdt.h"
#include "config.h"

#define CAMERA_MODEL_ESP_EYE

#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

#define MAX_UDP 1430
#define LED 4
AsyncUDP udp;

static const char *_JPG_HEADER = "HTTP/1.1 200 OK\r\n"
                                 "Content-disposition: inline; filename=cam.jpg\r\n"
                                 "Content-type: image/jpeg\r\n\r\n";
static const char *_STREAM_HEADER = "HTTP/1.1 200 OK\r\n"
                                    "Access-Control-Allow-Origin: *\r\n"
                                    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n";
static const char *_STREAM_BOUNDARY = "\r\n--frame\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n";

camera_fb_t *CAMFB;
String hostName = "AP-";

void setup()
{
    pinMode(LED, OUTPUT);

    char buf[2];
    for (int i = 24; i < 41; i = i + 8)
    {
        sprintf(buf, "%02X", (ESP.getEfuseMac() >> i) & 0xff);
        hostName += String(buf);
    }

    fsRead();
    WifiInit();
    CameraInit();

    httpServer.on("/", handleRoot);
    httpServer.on("/led", handleLED);
    httpServer.on("/jpg", handleJPG);
    httpServer.on("/stream", handleStream);
    httpServer.on("/save", handleSave);
    httpServer.on("/upload", HTTP_POST, handleFirmware, handleUpload);
    httpServer.onNotFound(handleRoot);
    httpServer.begin();
    dnsServer.start(53, "*", IPAddress(192, 168, 4, 1));

    if (!WiFi.isConnected())
    {
        digitalWrite(LED, HIGH);
        WiFi.softAP(hostName);
    }

    if (udp.listen(9122))
        udp.onPacket(onPacketCallBack);
}

void loop()
{
    dnsServer.processNextRequest();
    httpServer.handleClient();
    delay(10);
}

void WifiInit()
{
    WiFi.disconnect(true, true);
    delay(100);
    WiFi.hostname(hostName);
    WiFi.mode(WIFI_AP_STA);
    WiFi.enableAP(false);
    if (mConfig.ssid == "")
        return;
    WiFi.begin(mConfig.ssid, mConfig.pwd);
    WiFi.waitForConnectResult();
}

void onPacketCallBack(AsyncUDPPacket packet)
{
    uint8_t state[1];
    packet.readBytes(state, 1);

    digitalWrite(LED, state[0]);

    IPAddress ip = packet.remoteIP();
    u16_t port = packet.remotePort();
    uint8_t data[MAX_UDP];
    esp_camera_fb_return(CAMFB);
    CAMFB = esp_camera_fb_get();
    if (CAMFB)
    {
        int len = CAMFB->len;
        uint8_t *buf = CAMFB->buf;
        int count = len / MAX_UDP;
        int extra = len % MAX_UDP;
        char a8 = add8(buf, len);
        for (int i = 0; i < count; i++)
        {
            udp.writeTo(buf, MAX_UDP, ip, port);
            buf += MAX_UDP;
            delay(1);
        }
        udp.writeTo(buf, extra, ip, port);
        delay(1);
        u8_t end[] = {a8, 0x23, 0x23, 0x23};
        udp.writeTo(end, 4, ip, port);
        delay(20);
    }
    esp_camera_fb_return(CAMFB);
}

void handleRoot()
{
    String page = FPSTR(HTTP_HTML);
    page.replace("{container}", HTTP_FORM_DATA);
    page.replace("{version}", VERSION);
    page.replace("{hostname}", hostName);
    page.replace("{ssid}", mConfig.ssid);
    page.replace("{pwd}", mConfig.pwd);
    httpServer.sendHeader("Connection", "close");
    httpServer.send(200, "text/html", page);
}

void handleSave()
{
    mConfig.ssid = httpServer.arg("ssid");
    mConfig.pwd = httpServer.arg("pwd");
    fsWrite();
    String page = FPSTR(HTTP_HTML);
    page.replace("{container}", HTTP_SAVED);
    httpServer.sendHeader("Connection", "close");
    httpServer.send(200, "text/html", page);
    delay(100);
    ESP.restart();
}

void handleFirmware()
{
    String page = FPSTR(HTTP_HTML);
    page.replace("{container}", HTTP_FIRMWARE);
    page.replace("{firmware}", (Update.hasError()) ? "失败" : "成功");
    httpServer.sendHeader("Connection", "close");
    httpServer.send(200, "text/html", page);
    delay(100);
    ESP.restart();
}

void handleUpload()
{
    HTTPUpload &upload = httpServer.upload();
    if (upload.status == UPLOAD_FILE_START)
        Update.begin();
    else if (upload.status == UPLOAD_FILE_WRITE)
        Update.write(upload.buf, upload.currentSize);
    else if (upload.status == UPLOAD_FILE_END)
        Update.end(true);
}

void handleLED()
{
    String led = httpServer.arg("state");
    digitalWrite(LED, led == "0" ? 0 : 1);
    httpServer.send(200);
}

void handleJPG()
{
    WiFiClient client = httpServer.client();
    if (!client.connected())
        return;
    esp_camera_fb_return(CAMFB);
    CAMFB = esp_camera_fb_get();
    if (CAMFB)
    {
        client.write(_JPG_HEADER, strlen(_JPG_HEADER));
        client.write((char *)CAMFB->buf, CAMFB->len);
    }
    esp_camera_fb_return(CAMFB);
}

void handleStream()
{
    char part_buf[64];
    WiFiClient client = httpServer.client();
    client.write(_STREAM_HEADER, strlen(_STREAM_HEADER));
    while (true)
    {
        if (!client.connected())
            break;
        esp_camera_fb_return(CAMFB);
        CAMFB = esp_camera_fb_get();
        if (!CAMFB)
            continue;
        client.write(_STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        size_t hlen = snprintf(part_buf, 64, _STREAM_PART, CAMFB->len);
        client.write(part_buf, hlen);
        client.write((char *)CAMFB->buf, CAMFB->len);
        esp_camera_fb_return(CAMFB);
        delay(20);
    }
}

void CameraInit()
{
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    config.frame_size = FRAMESIZE_HVGA;
    // config.frame_size = FRAMESIZE_VGA;
    // config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = 10;
    config.fb_count = 2;

    esp_camera_init(&config);
    sensor_t *s = esp_camera_sensor_get();
    // s->set_saturation(s, -2);
    // s->set_contrast(s, 2);
    s->set_ae_level(s, 2);
    // s->set_aec2(s, 0);
    s->set_gainceiling(s, GAINCEILING_2X);
}

u8_t add8(u8_t *buf, size_t size)
{
    u8_t add8 = 0x00;
    for (size_t i = 0; i < size; i++)
        add8 += buf[i];
    return add8 & 0xFF;
}

void fsRead()
{
    if (!LittleFS.begin(true))
        return;
    if (!LittleFS.exists("/cfg"))
        return;

    size_t i = 0;
    File fs = LittleFS.open("/cfg", "r");

    while (fs.available())
    {
        i++;
        String data = fs.readStringUntil('\n');
        data.trim();
        switch (i)
        {
        case 1:
            mConfig.ssid = data;
            break;
        case 2:
            mConfig.pwd = data;
            break;
        }
    }
    fs.close();
    LittleFS.end();
}

void fsWrite()
{
    LittleFS.begin(true);
    File fs = LittleFS.open("/cfg", FILE_WRITE);
    fs.println(mConfig.ssid);
    fs.println(mConfig.pwd);
    fs.close();
    LittleFS.end();
}
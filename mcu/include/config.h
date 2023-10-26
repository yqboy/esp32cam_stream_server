#ifndef CONFIG_H_
#define CONFIG_H_

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Update.h>

#define VERSION "v0.1"

DNSServer dnsServer;
WebServer httpServer(80);

typedef struct
{
    String ssid;
    String pwd;
} Config;
Config mConfig;

static const char HTTP_HTML[] PROGMEM = R"(
<!DOCTYPE html>
<html lang='zh-CN'>
<head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1, user-scalable=no'/>
    <style>
        h4 {
            margin: 20px 0 0;
        }

        h5 {
            margin: 10px 0 0;
        }

        label {
            font-size: .6rem;
        }

        select {
            padding: 5px;
            font-size: 1em;
            margin-bottom: 5px;
            width: 99%;
        }

        input {
            padding: 5px;
            font-size: 1em;
            margin-bottom: 5px;
            width: 95%
        }

        body {
            text-align: center;
        }

        button {
            border: 0;
            border-radius: 1rem;
            background-color: #1fa3ec;
            color: #fff;
            line-height: 2rem;
            font-size: 1rem;
            width: 100%;
            margin: 0.5rem 0;
        }

        .file {
            position: relative;
            text-align: center;
            border-radius: 1rem;
            background-color: #1fa3ec;
            color: #fff;
            line-height: 2rem;
            margin: 0.5rem 0;
        }

        .file input {
            position: absolute;
            right: 0;
            top: 0;
            opacity: 0;
        }

        .container{
            text-align:left;
            display:inline-block;
            min-width:260px;
        }
    </style>
</head>
<body>
<div class='container'>
{container}
</div>
</body>
</html>
)";
const char HTTP_FORM_DATA[] PROGMEM = R"(
    <h4>固件版本: {version}</h4>
    <h5>主机名: {hostname}</h5>
    <form action='save' method='GET'>
        <h4>WiFi 配置</h4>
        <label>SSID:</label>
        <input name='ssid' value='{ssid}'>
        <br/>
        <label>密码:</label>
        <input name='pwd' value='{pwd}'>
        <br/>
        <button type='submit'>保存</button>
    </form>
    <form action='upload' method='POST' enctype='multipart/form-data'>
        <div class="file">
            <input type='file' accept='.bin' name='firmware' onchange='submit()'>升级固件</input>
        </div>
    </form>
)";
const char HTTP_SAVED[] PROGMEM = "<div><br/>正在保存配置并重启...</div>";
const char HTTP_FIRMWARE[] PROGMEM = "<div><br/>升级{firmware}并重启...</div>";

#endif /* CONFIG_H_ */
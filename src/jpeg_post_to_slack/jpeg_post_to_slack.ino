#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_camera.h>
// #include <SD.h>
// #include <SPI.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "camera_pins.h"

// // SDカードのチップセレクトピン
// #define SD_CS 9
// #define SD_MOSI 38
// #define SD_CLK 39
// #define SD_MISO 40

#define BAURATE 115200

#define SSID0 "******Y"     //WiFiのSSIDです
#define PASSWORD0 "******"  //WiFiのパスワードです
#define SSID1 "******"      //WiFiのSSIDです
#define PASSWORD1 "******"  //WiFiのパスワードです

#define CHANNEL "******"                                             //スラックのチャンネルIDです
#define TAKEN "xoxb-**********************************************"  //スラックアプリ用に取得したトークンです
#define CHUNKSIZE 5120;                                              // 5KB chunks

camera_fb_t* Camera_fb;

void setup() {
  Serial.begin(BAURATE);
  //// SPIピンの設定
  //SPI.begin(SD_CLK, SD_MISO, SD_MOSI, SD_CS);
  // // SDカードの初期化
  // if (!SD.begin(SD_CS)) {
  //   Serial.println("SDカードのマウントに失敗しました");
  // } else {
  //   IsSD_Card = true;
  //   Serial.println("SDカードがマウントされました");
  // }

  //WiFiルーターに接続します
  Connection();

  // カメラ初期化
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  //config.frame_size = FRAMESIZE_QVGA;  // フレームサイズをQVGAに設定
  //config.frame_size = FRAMESIZE_VGA;
  config.frame_size = FRAMESIZE_XGA;
  config.jpeg_quality = 12;  // JPEG品質を調整
  config.fb_count = 2;
  config.grab_mode = CAMERA_GRAB_LATEST;

  while (true) {
    if (esp_camera_init(&config) != ESP_OK) {
      Serial.println("カメラの初期化に失敗しました");
    } else {
      Serial.println("カメラが初期化されました");
      break;
    }
    delay(1000);
  }

  // カメラセンサーの取得
  sensor_t* s = esp_camera_sensor_get();
  if (s == NULL) {
    Serial.println("センサーの取得に失敗しました");
    return;
  }

  // ホワイトバランスを自動に設定
  s->set_whitebal(s, 1);  // 0 = Off, 1 = On

  s->set_vflip(s, 0);  // 0 = 正常, 1 = 上下反転
                       //s->set_hmirror(s, 0);  // 0 = 正常, 1 = 左右反転

  // 他の設定例
  // s->set_brightness(s, 1);  // -2 to 2
  // s->set_contrast(s, 1);    // -2 to 2
  // s->set_saturation(s, 1);  // -2 to 2

  //16枚を空撮影、ホワイトバランス確保のため
  for (int i = 0; i < 16; i++) {
    // カメラから画像を取得
    Camera_fb = esp_camera_fb_get();
    if (!Camera_fb) {
      Serial.println("画像の取得に失敗しました");
      delay(1000);
      continue;
    }
    esp_camera_fb_return(Camera_fb);  // フレームバッファを解放
    delay(100);
  }

  //WriteSlackChnnel("書き込みのテストです。\r\n");
}

void loop() {

  // カメラから画像を取得
  Camera_fb = esp_camera_fb_get();
  if (!Camera_fb) {
    Serial.println("画像の取得に失敗しました");
    return;
  }

  JpegSlackChnnelUpLoad("sample.jpg", Camera_fb->buf, Camera_fb->len, "M5 Stack CamS3 Unit の画像です", "タイトル");
  esp_camera_fb_return(Camera_fb);

  //60秒待つ
  delay(60000);
}

//*************************
// WiFiに接続します
//*************************
void Connection() {
  int cnt;
  if (WiFi.isConnected()) {
    WiFi.disconnect();
    delay(3000);
  }

  while (!WiFi.isConnected()) {
    Serial.print("\r\nWaiting connect to WiFi: " SSID0);
    WiFi.begin(SSID0, PASSWORD0);

    cnt = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      cnt += 1;
      if (cnt == 10) {
        Serial.println("Conncet failed!");
        break;
      }
    }

    Serial.print("\r\nWaiting connect to WiFi: " SSID1);
    WiFi.begin(SSID1, PASSWORD1);

    cnt = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      cnt += 1;
      if (cnt == 10) {
        Serial.println("Conncet failed!");
        break;
      }
    }
  }
  Serial.print("\nIP address: ");
  Serial.println(WiFi.localIP());
}

//*************************
// スラックにjpeg画像をアップロードします
//*************************
void JpegSlackChnnelUpLoad(String jpegFilename, uint8_t* jpegData, size_t len, String comment, String title) {
  WiFiClientSecure client;

  client.setInsecure();  // Skip SSL certificate verification

  if (!client.connect("slack.com", 443)) {
    Serial.println("Connection failed!");
    client.stop();
    return;
  }

  String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
  String startBoundary = "--" + boundary + "\r\n";
  String endBoundary = "\r\n--" + boundary + "--\r\n";
  String tokenPart = "Content-Disposition: form-data; name=\"token\"\r\n\r\n" + String(TAKEN) + "\r\n";
  String channelsPart = "Content-Disposition: form-data; name=\"channels\"\r\n\r\n" + String(CHANNEL) + "\r\n";
  String commentPart = "Content-Disposition: form-data; name=\"initial_comment\"\r\n\r\n" + comment + "\r\n";
  String titlePart = "Content-Disposition: form-data; name=\"title\"\r\n\r\n" + title + "\r\n";
  String filePartHeader = "Content-Disposition: form-data; name=\"file\"; filename=\"" + jpegFilename + "\"\r\n";

  filePartHeader += "Content-Type: image/jpeg\r\n\r\n";

  int contentLength = startBoundary.length() + tokenPart.length() + startBoundary.length() + channelsPart.length() + startBoundary.length() + commentPart.length() + startBoundary.length() + titlePart.length() + startBoundary.length() + filePartHeader.length() + len + endBoundary.length();

  client.println("POST /api/files.upload HTTP/1.1");
  client.println("Host: slack.com");
  client.println("Authorization: Bearer " + String(TAKEN));
  client.println("Content-Type: multipart/form-data; boundary=" + boundary);
  client.print("Content-Length: ");
  client.println(contentLength);
  client.println();

  // Send token part
  client.print(startBoundary);
  client.print(tokenPart);

  // Send channels part
  client.print(startBoundary);
  client.print(channelsPart);

  // Send initial comment part
  client.print(startBoundary);
  client.print(commentPart);

  // Send title part
  client.print(startBoundary);
  client.print(titlePart);

  // Send file part
  client.print(startBoundary);
  client.print(filePartHeader);

  // Send the binary data in chunks
  size_t chunkSize = CHUNKSIZE;
  size_t dataSize = len;
  size_t sent = 0;

  while (sent < dataSize) {
    size_t remaining = dataSize - sent;
    size_t toSend = remaining < chunkSize ? remaining : chunkSize;
    client.write(jpegData + sent, toSend);
    sent += toSend;
  }

  client.print(endBoundary);

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
    if (line == "\r") {
      break;
    }
  }

  while (client.available()) {
    char c = client.read();
    Serial.write(c);
  }
  client.stop();
}

//*************************
// スラックに書き込みます
//*************************
void WriteSlackChnnel(String message) {
  WiFiClientSecure client;

  client.setInsecure();  //skip verification

  if (!client.connect("slack.com", 443)) {
    Serial.println("Connection to Slack failed!");
    return;
  }

  String postData = "token=" + String(TAKEN) + "&channel=" + String(CHANNEL) + "&text=" + String(message);

  client.println("POST /api/chat.postMessage HTTP/1.1");
  client.println("Host: slack.com");
  client.println("Content-Type: application/x-www-form-urlencoded");
  client.print("Content-Length: ");
  client.println(postData.length());
  client.println();
  client.print(postData);

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }

  while (client.available()) {
    String line = client.readStringUntil('\n');
    Serial.println(line);
  }
  client.stop();
}
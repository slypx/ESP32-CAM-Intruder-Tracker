#include <WebServer.h>
#include <WiFi.h>
#include <esp32cam.h>
#include <SD_MMC.h>

#include "secrets.h"

WebServer server(80);

static auto lowResolution = esp32cam::Resolution::find(640, 480);
static auto mediumResolution = esp32cam::Resolution::find(1280, 720);
static auto highResolution = esp32cam::Resolution::find(1920, 1080);

bool isRecording = false;
bool isSdMounted = false;
File recordingFile;

bool ensureSdCard() {
  if (isSdMounted) {
    return true;
  }

  isSdMounted = SD_MMC.begin();
  return isSdMounted;
}

void serveJpeg() {
  auto frame = esp32cam::capture();
  if (frame == nullptr) {
    Serial.println("CAPTURE FAIL");
    server.send(503, "text/plain", "Camera capture failed");
    return;
  }

  Serial.printf(
      "CAPTURE OK %dx%d %db\n",
      frame->getWidth(),
      frame->getHeight(),
      static_cast<int>(frame->size()));

  server.setContentLength(frame->size());
  server.send(200, "image/jpeg");
  WiFiClient client = server.client();
  frame->writeTo(client);
}

void handleJpegLow() {
  if (!esp32cam::Camera.changeResolution(lowResolution)) {
    Serial.println("SET-LOW-RES FAIL");
  }
  serveJpeg();
}

void handleJpegMedium() {
  if (!esp32cam::Camera.changeResolution(mediumResolution)) {
    Serial.println("SET-MEDIUM-RES FAIL");
  }
  serveJpeg();
}

void handleJpegHigh() {
  if (!esp32cam::Camera.changeResolution(highResolution)) {
    Serial.println("SET-HIGH-RES FAIL");
  }
  serveJpeg();
}

void handleStream() {
  if (!esp32cam::Camera.changeResolution(highResolution)) {
    Serial.println("SET-STREAM-RES FAIL");
    server.send(503, "text/plain", "Streaming setup failed");
    return;
  }

  WiFiClient client = server.client();
  constexpr char streamHeader[] =
      "--frame\r\nContent-Type: image/jpeg\r\n\r\n";

  server.sendContent(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n");

  while (client.connected()) {
    auto frame = esp32cam::capture();
    if (frame == nullptr) {
      Serial.println("STREAM CAPTURE FAIL");
      break;
    }

    server.sendContent(streamHeader);
    frame->writeTo(client);
    server.sendContent("\r\n");

    if (isRecording && recordingFile) {
      frame->writeTo(recordingFile);
    }

    delay(100);
  }
}

void handleStartRecording() {
  if (!ensureSdCard()) {
    Serial.println("SD CARD MOUNT FAIL");
    server.send(500, "text/plain", "SD card mount failed");
    return;
  }

  const String videoPath = "/video-" + String(millis()) + ".mjpeg";
  recordingFile = SD_MMC.open(videoPath.c_str(), FILE_WRITE);
  if (!recordingFile) {
    Serial.println("FILE OPEN FAIL");
    server.send(500, "text/plain", "Recording file open failed");
    return;
  }

  isRecording = true;
  Serial.printf("RECORDING STARTED: %s\n", videoPath.c_str());
  server.send(200, "text/plain", "Recording started: " + videoPath);
}

void handleStopRecording() {
  if (!isRecording || !recordingFile) {
    server.send(400, "text/plain", "Not recording");
    return;
  }

  recordingFile.close();
  isRecording = false;
  Serial.println("RECORDING STOPPED");
  server.send(200, "text/plain", "Recording stopped");
}

void handleSavePhoto() {
  if (!ensureSdCard()) {
    Serial.println("SD CARD MOUNT FAIL");
    server.send(500, "text/plain", "SD card mount failed");
    return;
  }

  auto frame = esp32cam::capture();
  if (frame == nullptr) {
    Serial.println("CAPTURE FAIL");
    server.send(503, "text/plain", "Camera capture failed");
    return;
  }

  const String photoPath = "/photo-" + String(millis()) + ".jpg";
  File photoFile = SD_MMC.open(photoPath.c_str(), FILE_WRITE);
  if (!photoFile) {
    Serial.println("FILE OPEN FAIL");
    server.send(500, "text/plain", "Photo file open failed");
    return;
  }

  frame->writeTo(photoFile);
  photoFile.close();

  Serial.printf("PHOTO SAVED: %s\n", photoPath.c_str());
  server.send(200, "text/plain", "Photo saved: " + photoPath);
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  using namespace esp32cam;

  Config config;
  config.setPins(pins::AiThinker);
  config.setResolution(highResolution);
  config.setBufferCount(2);
  config.setJpeg(80);

  const bool cameraReady = Camera.begin(config);
  Serial.println(cameraReady ? "CAMERA OK" : "CAMERA FAIL");
  if (!cameraReady) {
    return;
  }

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  Serial.print("Camera server: http://");
  Serial.println(WiFi.localIP());
  Serial.println("  /cam-lo.jpg");
  Serial.println("  /cam-mid.jpg");
  Serial.println("  /cam-hi.jpg");
  Serial.println("  /stream");
  Serial.println("  /save-photo");
  Serial.println("  /start-recording");
  Serial.println("  /stop-recording");

  server.on("/cam-lo.jpg", handleJpegLow);
  server.on("/cam-mid.jpg", handleJpegMedium);
  server.on("/cam-hi.jpg", handleJpegHigh);
  server.on("/stream", handleStream);
  server.on("/save-photo", handleSavePhoto);
  server.on("/start-recording", handleStartRecording);
  server.on("/stop-recording", handleStopRecording);
  server.begin();
}

void loop() {
  server.handleClient();
}

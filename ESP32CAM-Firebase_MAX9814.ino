#include <Base64.h>
#include <WiFi.h>
#include <esp_camera.h>
#include "FirebaseESP32.h"

// Camera Model Configuration
#define CAMERA_MODEL_AI_THINKER

// Camera pin definitions
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

// Microphone pin
#define MIC_PIN 12  // Define the pin for the microphone (e.g., MAX9814)

// Wi-Fi Credentials
const char* ssid = "your-ssid";          // Replace with your Wi-Fi SSID
const char* password = "your-password";  // Replace with your Wi-Fi password

// Firebase Credentials
const char* firebaseHost = "your-realtime-database-url"; // Replace with your Firebase database URL (.firebasedatabase.app/)
const char* firebaseToken = "your-database-secrets";  // Open Project settings -> Service accounts -> Database secrets 

// Firebase objects
FirebaseData firebaseData;
FirebaseConfig config;
FirebaseAuth auth;

bool wifiConnected = false;

// Sound detection threshold
const int soundThreshold = 1550;  // Adjust this threshold based on your environment

void ensureWiFiConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconnecting to Wi-Fi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("Reconnected to Wi-Fi!");
  }
}

void setupCamera() {
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

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 8;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }

  sensor_t* s = esp_camera_sensor_get();
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
  s->set_dcw(s, 1);

  // Configure lightning settings
  s->set_exposure_ctrl(s, 1);    
  s->set_gain_ctrl(s, 1);                      
  s->set_awb_gain(s, 1);                        
  s->set_brightness(s, 2); 
  s->set_contrast(s, -2);
}


String photoToBase64() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return "";
  }

  // Convert the captured image to Base64
  String base64Image = "data:image/jpeg;base64,";
  base64Image += base64::encode(fb->buf, fb->len);

  esp_camera_fb_return(fb);
  return base64Image;
}

int photoCount = 0;  

void uploadPhotoToFirebase() {
    ensureWiFiConnected(); // Ensure Wi-Fi is connected before proceeding.

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        return;
    }

    // Debugging: Print the image length to verify if a new image is captured
    Serial.print("Captured Image Length: ");
    Serial.println(fb->len);

    // Encode image to Base64
    String base64Image = base64::encode(fb->buf, fb->len);
    esp_camera_fb_return(fb); // Release the frame buffer

    Serial.print("Base64 Image Size: ");
    Serial.println(base64Image.length());  // Debugging the size of the image

    if (!base64Image.isEmpty()) {
        // String path = String("/esp32cam/") + "data"; // Fixed path for overwriting the image data

        bool soundDetectedStatus = detectSound();

        FirebaseJson json;
        json.set("image", base64Image);      // Store the Base64 encoded image
        json.set("soundDetected", soundDetectedStatus); // Set the soundDetected field to true or false
        json.set("counter", String(photoCount));  // Convert the photoCount to string and store it
     

        // Increment photo count after storing the image
        photoCount++;  

        Serial.print("Uploading to path: ");
        // Serial.println(path);

        // Upload data to Firebase (overwriting any previous data at '/Frame')
        Firebase.setJSON(firebaseData, "/esp32camdata", json);

        if (firebaseData.errorReason() != "") {
            Serial.print("Error uploading photo: ");
            Serial.println(firebaseData.errorReason());
            Serial.print("Error code: ");
            Serial.println(firebaseData.httpCode());
        } else {
            Serial.println("Photo and sound data successfully uploaded to Firebase!");
        }

    } else {
        Serial.println("Failed to convert photo to Base64.");
    }
}

// Function to detect sound from the microphone
bool detectSound() {
  int soundLevel = analogRead(MIC_PIN);  // Read sound level from the microphone pin
  Serial.print("Sound Level: ");
  Serial.println(soundLevel);

  // Return true if sound level exceeds the threshold
  return soundLevel > soundThreshold;
}

void setup() {
  Serial.begin(115200);

  // Wait for sound detection before connecting to Wi-Fi
  Serial.println("Waiting for sound to connect to Wi-Fi...");
  while (!detectSound()) {
    delay(50);  // Check for sound every 50ms
    Serial.print(".");
  }

  Serial.println("Sound detected, connecting to Wi-Fi...");

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Wi-Fi connected!");

  config.database_url = firebaseHost;
  config.signer.tokens.legacy_token = firebaseToken;
  Firebase.begin(&config, &auth);

  setupCamera();

  Serial.println("Setup complete!");
}

void loop() {
  uploadPhotoToFirebase();  // Capture and upload the image immediately
  delay(500); 
}

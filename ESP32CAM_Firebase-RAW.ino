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

// Wi-Fi Credentials
const char* ssid = "katulistiwa65";          // Replace with your Wi-Fi SSID
const char* password = "gujarat98";  // Replace with your Wi-Fi password

// Firebase Credentials
const char* firebaseHost = "https://esp32cam-9b78b-default-rtdb.asia-southeast1.firebasedatabase.app/"; // Replace with your Firebase database URL
const char* firebaseToken = "JST4nrln2gQRgGKsVIDiTaMdJjOmtpc1DU5qjaYu";  // 

// Firebase objects
FirebaseData firebaseData;
FirebaseConfig config;
FirebaseAuth auth;

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
        config.frame_size = FRAMESIZE_QVGA;
        config.jpeg_quality = 10;
        config.fb_count = 2;
    } else {
        config.frame_size = FRAMESIZE_QVGA;
        config.jpeg_quality = 8;
        config.fb_count = 1;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        ESP.restart();
    }

    sensor_t *s = esp_camera_sensor_get();
    s->set_vflip(s, 1);            // Enable vertical flip (Main thing)
    s->set_dcw(s, 1); 

    // Configure sensor settings
    s->set_exposure_ctrl(s, 1);    // Disable automatic exposure control
    s->set_gain_ctrl(s, 1);                       // auto gain on
    s->set_awb_gain(s, 1);                        // Auto White Balance enable (0 or 1)
    s->set_brightness(s, 2); 
    s->set_contrast(s, -2);

    
}

String photoToBase64() {
    camera_fb_t *fb = esp_camera_fb_get();
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

void uploadPhotoToFirebase() {
    ensureWiFiConnected(); // Ensure Wi-Fi is connected before proceeding.

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        return;
    }

    // Encode image to Base64
    String base64Image = base64::encode(fb->buf, fb->len);
    esp_camera_fb_return(fb); // Release the frame buffer

    Serial.print("Base64 Image Size: ");
    Serial.println(base64Image.length());  // Debugging the size of the image

    if (!base64Image.isEmpty()) {
        // Generate a timestamp
        String timestamp = String(millis());

        // Create the Firebase path
        String path = "/photos/" + timestamp;

        Serial.print("Uploading to path: ");
        Serial.println(path);  // Debugging the upload path

        // Upload the Base64 image string to Firebase
        Firebase.setString(firebaseData, path, base64Image);

        if (firebaseData.errorReason() != "") {
            Serial.print("Error uploading photo: ");
            Serial.println(firebaseData.errorReason());
            Serial.print("Error code: ");
            Serial.println(firebaseData.httpCode());
        } else {
            Serial.println("Photo successfully uploaded to Firebase!");
        }
    } else {
        Serial.println("Failed to convert photo to Base64.");
    }
}



void setup() {
    Serial.begin(115200);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi connected");

    config.database_url = firebaseHost;
    config.signer.tokens.legacy_token = firebaseToken;
    Firebase.begin(&config, &auth);

    setupCamera();

    Serial.println("Setup complete!");
}

void loop() {
    uploadPhotoToFirebase();
    delay(5000);
}

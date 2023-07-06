#include <FirebaseESP32.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP_Mail_Client.h>

// Set your Wi-Fi credentials
#define WIFI_SSID "Network"
#define WIFI_PASSWORD "2023unicor"
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465

// Set your Firebase project's API Key and Firestore URL
#define API_KEY "AIzaSyCIZmSGZVMMG7jO-MAG9vJvAFZofx0Tq7E"
#define DATABASE_URL "https://piralarm-df5d9-default-rtdb.firebaseio.com"
/* The sign in credentials */
#define AUTHOR_EMAIL "unicorpirpas@gmail.com"
#define AUTHOR_PASSWORD "depmfpsdfeiaagvi"
/* Recipient's email */
#define RECIPIENT_EMAIL "francodisel12@gmail.com"

const int alarmPin = 5;

FirebaseData firebaseData;
SMTPSession smtp;

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status);

void sendEmail(SMTPSession* smtp) {
  /* Declare the message class */
  SMTP_Message message;

  /* Set the message headers */
  message.sender.name = "ESP Mail";
  message.sender.email = AUTHOR_EMAIL;

  message.subject = F("Sensor Notification");
  message.addRecipient(F("Sara"), RECIPIENT_EMAIL);

  /* Two alternative content versions are sending in this example: plain text and html */
  String textMsg = "One of the sensors in your prototype has been activated.";
  message.text.content = textMsg.c_str();
  message.text.charSet = "utf-8";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_qp;

  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_normal;
  message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;

  /* Start sending the Email */
  if (!MailClient.sendMail(smtp, &message, true))
    Serial.println("Error sending Email, " + smtp->errorReason());
}

void setup() {
  Serial.begin(115200);
  pinMode(alarmPin, OUTPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to Wi-Fi...");
  }
  Serial.println("Wi-Fi connection established");

  // Initialize Firebase
  Firebase.begin(DATABASE_URL, API_KEY);
}

void loop() {
  Firebase.getBool(firebaseData, "/alarm/state");
  bool currentState = firebaseData.boolData();
  static bool previousState = false;
  digitalWrite(alarmPin, currentState);

  if (currentState) {
    if (!previousState) {
      // Init filesystem
      ESP_MAIL_DEFAULT_FLASH_FS.begin();

      /** Enable the debug via Serial port
       * none debug or 0
       * basic debug or 1
       */
      smtp.debug(1);

      /* Set the session config */
      Session_Config config;
      config.server.host_name = SMTP_HOST;
      config.server.port = SMTP_PORT;
      config.login.email = AUTHOR_EMAIL;
      config.login.password = AUTHOR_PASSWORD;
      config.login.user_domain = "mydomain.net";

      /*
      Set the NTP config time
      For times east of the Prime Meridian use 0-12
      For times west of the Prime Meridian add 12 to the offset.
      Ex. American/Denver GMT would be -6. 6 + 12 = 18
      See https://en.wikipedia.org/wiki/Time_zone for a list of the GMT/UTC timezone offsets
      */
      config.time.ntp_server = F("pool.ntp.org,time.nist.gov");
      config.time.gmt_offset = 3;
      config.time.day_light_offset = 0;

      /* Connect to server with the session config */
      if (!smtp.connected()) {
        if (!smtp.connect(&config)) {
          ESP_MAIL_PRINTF("Connection error, Status Code: %d, Error Code: %d, Reason: %s", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
          return;
        }
      }

      if (!smtp.isLoggedIn()) {
        Serial.println("\nNot yet logged in.");
      } else {
        if (smtp.isAuthenticated())
          Serial.println("\nSuccessfully logged in.");
        else
          Serial.println("\nConnected with no Auth.");
      }

      /* Set the callback function to get the sending results */
      smtp.callback(smtpCallback);

      sendEmail(&smtp);
    }
  }

  previousState = currentState;

  delay(1000);
}

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status) {
  /* Print the current status */
  Serial.println(status.info());

  /* Print the sending result */
  if (status.success()) {
    Serial.println("----------------");
    ESP_MAIL_PRINTF("Message sent success: %d\n", status.completedCount());
    ESP_MAIL_PRINTF("Message sent failed: %d\n", status.failedCount());
    Serial.println("----------------\n");
    struct tm dt;

    for (size_t i = 0; i < smtp.sendingResult.size(); i++) {
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);
      time_t ts = (time_t)result.timestamp;
      localtime_r(&ts, &dt);

      ESP_MAIL_PRINTF("Message No: %d\n", i + 1);
      ESP_MAIL_PRINTF("Status: %s\n", result.completed ? "success" : "failed");
      ESP_MAIL_PRINTF("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      ESP_MAIL_PRINTF("Recipient: %s\n", result.recipients.c_str());
      ESP_MAIL_PRINTF("Subject: %s\n", result.subject.c_str());
    }
    Serial.println("----------------\n");

    // You need to clear sending result as the memory usage will grow up.
    smtp.sendingResult.clear();
  }
}
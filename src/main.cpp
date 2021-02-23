#include <Arduino.h>
// #include <Adafruit_Sensor.h>
#include "DHT.h"
#include <WiFi.h>
#include "secrets.h"
#include "ThingSpeak.h" // always include thingspeak header file after other header files and custom macros
#include <ESP_Mail_Client.h>

#define DHTPIN 15
#define DHTTYPE DHT22

#define emailRecipient ""
#define emailSubject "Warning from IoT_Cloud"

#define SMTP_HOST "smtp.gmail.com"

/** The smtp port e.g. 
 * 25  or esp_mail_smtp_port_25
 * 465 or esp_mail_smtp_port_465
 * 587 or esp_mail_smtp_port_587
*/
#define SMTP_PORT 25

/* The log in credentials */
#define AUTHOR_EMAIL "iotcloud.esp32@gmail.com"
#define AUTHOR_PASSWORD "IotCloudESP32"

char ssid[] = SECRET_SSID; // your network SSID (name)
char pass[] = SECRET_PASS; // your network password
int keyIndex = 0;          // your network key Index number (needed only for WEP)
int measurePin = 34;
int ledPower = 32;

unsigned int samplingTime = 280, send_time = 0;
unsigned int deltaTime = 40;
unsigned int sleepTime = 9680;

float voMeasured = 0, h, t, f;
float calcVoltage = 0;
float dustDensity = 0;
float t_dustDensity = 0;
unsigned long myChannelNumber = SECRET_CH_ID;
const char *myWriteAPIKey = SECRET_WRITE_APIKEY;
int valid_mail = 0;
String colour[5] = {"", "", "", "", ""};
WiFiClient client;
DHT dht(DHTPIN, DHTTYPE);
SMTPSession smtp;
ESP_Mail_Session session;
SMTP_Message message;

void smtpCallback(SMTP_Status status);
void send_mail();
void setup()
{
  Serial.begin(9600);
  Serial.println(F("DHTxx test!"));
  WiFi.mode(WIFI_STA);
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(SECRET_SSID);
    while (WiFi.status() != WL_CONNECTED)
    {
      WiFi.begin(ssid, pass); // Connect to WPA/WPA2 network. Change this line if using open or WEP network
      Serial.print(".");
      delay(5000);
    }
    Serial.println("\nConnected.");
  }
  ThingSpeak.begin(client); // Initialize ThingSpeak
  dht.begin();
  pinMode(ledPower, OUTPUT);

  // String msg = "";
  //   if (1)
  //   {
  //     msg+="<p>Alert! High Temperature Detected, Take necessary step.</p><p>Sensor Value: "+String(t)+" °C</p>";
  //   }
  //   if (1)
  //   {
  //     msg+="<p>Alert! High Humidity Detected, Take necessary step.</p><p>Sensor Value: "+String(h)+" %</p>";
  //   }
  //   if (1)
  //   {
  //     msg+="<p>Alert! High Dust Concentration Detected, Take necessary step.</p><p>Sensor Value: "+String(t_dustDensity)+" µg/m³</p>";
  //   }
  //   message.html.content =msg.c_str();
  //   send_mail();

  for (int ch = 4; ch < 9; ch++)
  {
    String tmp = ThingSpeak.readStringField(SECRET_CH_ID, ch, SECRET_WRITE_APIKEY);
    if (tmp.indexOf("@") > 0)
    {

      // Serial.println(tmp);
      colour[valid_mail] = tmp;
      valid_mail++;
      // int tmp_len = tmp.length() + 1;
      // char tmp_array[tmp_len];
      // tmp.toCharArray(tmp_array,tmp_len);
      // colour[valid_mail]= tmp_array;
    }
  }
  Serial.println("Valid mail found: " + String(valid_mail));
  for (int hh = 0; hh < valid_mail; hh++)
  {
    Serial.println(colour[hh]);
  }

  send_time = millis();
}
int i;

void loop()
{
  t_dustDensity = 0;
  for (i = 0; i < 20; i++)
  {

    digitalWrite(ledPower, LOW);
    delayMicroseconds(samplingTime);

    voMeasured = analogRead(measurePin);

    delayMicroseconds(deltaTime);
    digitalWrite(ledPower, HIGH);
    delayMicroseconds(sleepTime);

    calcVoltage = voMeasured * (3.3 / 4095.0);
    dustDensity = (0.17 * calcVoltage - 0.1) * 1000; //

    if (dustDensity < 0)
    {
      dustDensity = 0.00;
    }
    t_dustDensity += dustDensity;
    Serial.println("Raw Signal Value (0-4095):");
    Serial.println(voMeasured);

    Serial.println("Voltage:");
    Serial.println(calcVoltage);

    Serial.println("Dust Density:");
    Serial.println(dustDensity);

    delay(1000);
  }
  t_dustDensity = t_dustDensity / 20;
  // Wait a few seconds between measurements.

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(SECRET_SSID);
    while (WiFi.status() != WL_CONNECTED)
    {
      WiFi.begin(ssid, pass); // Connect to WPA/WPA2 network. Change this line if using open or WEP network
      Serial.print(".");
      delay(5000);
    }
    Serial.println("\nConnected.");
  }

  // set the fields with the values

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f))
  {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);
  ThingSpeak.setField(1, t);
  ThingSpeak.setField(2, h);
  ThingSpeak.setField(3, t_dustDensity);
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if (x == 200)
  {
    Serial.println("Channel update successful.");
  }
  else
  {
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }
  if (((t > 21) | (h > 55) | (t_dustDensity > 30)) && (millis() - send_time > 300000))
  {
    String msg = "";
    if (t > 21)
    {
      msg += "<p>Alert! High Temperature Detected, Take necessary step.</p><p>Sensor Value: " + String(t) + " °C</p>";
    }
    if (h > 55)
    {
      msg += "<p>Alert! High Humidity Detected, Take necessary step.</p><p>Sensor Value: " + String(h) + " %</p>";
    }
    if (t_dustDensity > 30)
    {
      msg += "<p>Alert! High Dust Concentration Detected, Take necessary step.</p><p>Sensor Value: " + String(t_dustDensity) + " µg/m³</p>";
    }
    message.html.content = msg.c_str();
    send_mail();
    send_time = millis();
  }
  Serial.print("Avg_dustDensity");
  Serial.print(t_dustDensity / 20);
  Serial.print(F("  Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.print(F("°C "));
  Serial.print(f);
  Serial.print(F("°F  Heat index: "));
  Serial.print(hic);
  Serial.print(F("°C "));
  Serial.print(hif);
  Serial.println(F("°F"));
}
void smtpCallback(SMTP_Status status)
{
  /* Print the current status */
  Serial.println(status.info());

  /* Print the sending result */
  if (status.success())
  {
    Serial.println("----------------");
    Serial.printf("Message sent success: %d\n", status.completedCount());
    Serial.printf("Message sent failled: %d\n", status.failedCount());
    Serial.println("----------------\n");
    struct tm dt;

    for (size_t i = 0; i < smtp.sendingResult.size(); i++)
    {
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);
      localtime_r(&result.timesstamp, &dt);

      Serial.printf("Message No: %d\n", i + 1);
      Serial.printf("Status: %s\n", result.completed ? "success" : "failed");
      Serial.printf("Date/Time: %d/%d/%d %d:%d:%d\n", dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
      Serial.printf("Recipient: %s\n", result.recipients);
      Serial.printf("Subject: %s\n", result.subject);
    }
    Serial.println("----------------\n");
  }
}
void send_mail()
{
  /** Enable the debug via Serial port
   * none debug or 0
   * basic debug or 1
  */
  smtp.debug(1);

  /* Set the callback function to get the sending results */
  smtp.callback(smtpCallback);

  /* Declare the session config data */

  /* Set the session config */
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  session.login.password = AUTHOR_PASSWORD;
  session.login.user_domain = "mydomain.net";

  /* Declare the message class */

  /* Set the message headers */
  message.sender.name = "ESP Mail";
  message.sender.email = AUTHOR_EMAIL;
  message.subject = "IoT Warning !!";
  message.addRecipient("Admin", emailRecipient);

  // message.html.content = "<p>This is the <span style=\"color:#ff0000;\">html text</span> message.</p><p>The message was sent via ESP device.</p>";

  /** The html text message character set e.g.
   * us-ascii
   * utf-8
   * utf-7
   * The default value is utf-8
  */
  message.html.charSet = "us-ascii";

  /** The content transfer encoding e.g.
   * enc_7bit or "7bit" (not encoded)
   * enc_qp or "quoted-printable" (encoded)
   * enc_base64 or "base64" (encoded)
   * enc_binary or "binary" (not encoded)
   * enc_8bit or "8bit" (not encoded)
   * The default value is "7bit"
  */
  message.html.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

  /** The message priority
   * esp_mail_smtp_priority_high or 1
   * esp_mail_smtp_priority_normal or 3
   * esp_mail_smtp_priority_low or 5
   * The default value is esp_mail_smtp_priority_low
  */
  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;

  /** The Delivery Status Notifications e.g.
   * esp_mail_smtp_notify_never
   * esp_mail_smtp_notify_success
   * esp_mail_smtp_notify_failure
   * esp_mail_smtp_notify_delay
   * The default value is esp_mail_smtp_notify_never
  */
  message.response.notify = esp_mail_smtp_notify_success | esp_mail_smtp_notify_failure | esp_mail_smtp_notify_delay;

  /* Set the custom message header */
  message.addHeader("Message-ID: <Warning>");

  /* Connect to server with the session config */
  if (!smtp.connect(&session))
    return;

  /* Start sending Email and close the session */
  for (int hh = 0; hh < valid_mail; hh++)
  {
    int tmp_len = colour[hh].length() + 1;
    char tmp_array[tmp_len];
    colour[hh].toCharArray(tmp_array,tmp_len);
    message.addRecipient("Admin", tmp_array);
    if (!MailClient.sendMail(&smtp, &message))
      Serial.println("Error sending Email, " + smtp.errorReason());
  }
}
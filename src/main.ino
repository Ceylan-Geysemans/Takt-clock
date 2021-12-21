#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

#define wifi_ssid "GL-AR750S-0c6" // wifi ssid
#define wifi_password "Ceylan07"  // wifi password

#define mqtt_server "192.168.8.130" // server name or IP
#define mqtt_user "mqttbroker"      // username
#define mqtt_password "admin123"    // password

#define actual_topic "takt/actual"
#define time_topic "takt/time"
#define live_topic "takt/livetijd"

// declare 7 segment displays
Adafruit_7segment matrix1 = Adafruit_7segment();
Adafruit_7segment matrix2 = Adafruit_7segment();
Adafruit_7segment matrix3 = Adafruit_7segment();

void setup_wifi();
void reconnect();

WiFiClient espClient;
PubSubClient client(espClient);

// declare i/o pons
#define led LED_BUILTIN
#define led1 26
#define led2 27
#define button1 14 // start knop
#define button2 15 // stop knop
#define button3 4  // pauze timer knop

// check variables
boolean startTimer = false;
boolean goalactual = true;
boolean countgoal = true;
boolean pauzeTimer = false;

// timing variables
int tijd;
int lastTrigger = 0;
int displaytime;
int pauzeTrigger = 0;
int pauzeTime = 0;
uint32_t h, m, s, rem, secs;

// goal actual variables
int goal;
int actual = 0;

// start intterupt
void IRAM_ATTR start()
{
  if (pauzeTimer == false)
  {
    startTimer = true;
    countgoal = true;
    lastTrigger = millis();
  }
  if (pauzeTimer == true)
  {
    pauzeTimer = false;
    pauzeTime = millis() - pauzeTrigger;
    Serial.print("einde pauze !!! pauzetime: ");
    Serial.println(pauzeTime / 1000);
    lastTrigger += pauzeTime;
    delay(10);
  }
}

// tijd = (millis() - lastTrigger) / 1000;

// stop interrupt
void IRAM_ATTR endtimer() {
  startTimer = false;
  goalactual = true;
}

// pauze interrupt
void IRAM_ATTR pauze()
{
  if (pauzeTimer == false)
  {
    pauzeTimer = true;
    pauzeTrigger = millis();
    Serial.println("pauze !!!");
  }
}

// declare loops
void printtime();
void timetel();
void countgoaler();
void printgoalactual();

void setup()
{
  Serial.begin(115200);

  setup_wifi(); // Connect to Wifi network

  client.setServer(mqtt_server, 8883); // Configure MQTT connection, change port if needed.

  client.setCallback(callback);

  if (!client.connected())
  {
    reconnect();
  }

  // initialise buttons and led
  pinMode(button1, INPUT_PULLUP);
  pinMode(button2, INPUT_PULLUP);
  pinMode(button3, INPUT_PULLUP);
  pinMode(led, OUTPUT);
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);

  // Start matrix displays
  matrix1.begin(0x70);
  matrix2.begin(0x71);
  matrix3.begin(0x72);

  // Interrupt service routine to start and stop timer
  attachInterrupt(digitalPinToInterrupt(button1), start, HIGH);
  attachInterrupt(digitalPinToInterrupt(button2), endtimer, HIGH);
  attachInterrupt(digitalPinToInterrupt(button3), pauze, HIGH);

  matrix3.print("B UP");
  matrix3.writeDisplay();
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();

  // calculate current time by timer
  timetel();
  // Print timer on matrix if timer is running
  if (startTimer == true && pauzeTimer == false)
  {
    printtime();
    digitalWrite(led1, HIGH);
    digitalWrite(led2, LOW);
  }
  else
  {
    digitalWrite(led1, LOW);
    digitalWrite(led2, HIGH);
  }
  // calculate actual worked pieces (under 5 seconds/piece does not count)
  if (goalactual == true && countgoal == true)
  {
    countgoaler();
    countgoal = false;
  }
  // Print goal
  if (goalactual == true)
  {
    printgoalactual();
    goalactual = false;
  }
}

void callback(char *topic, byte *message, unsigned int length)
{
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++)
  {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
  matrix2.print(messageTemp.toInt());
  matrix2.writeDisplay();
}

void printtime()
{
  // print serial monitor
  // Serial.print("Tijd: ");
  // Serial.println(tijd);

  // print matrix
  matrix3.print(displaytime, DEC);
  matrix3.drawColon(tijd % 2 == 0);
  matrix3.writeDisplay();
  client.publish(live_topic, String(String(h) + ":" + String(m) + ":" + String(s)).c_str(), true);
}

void countgoaler()
{
  if (tijd >= 5)
  {
    actual++;
  }
  matrix1.print(actual);
  matrix1.writeDisplay();

  client.publish(time_topic, String(String(h) + ":" + String(m) + ":" + String(s)).c_str(), true);

  client.publish(actual_topic, String(actual).c_str(), true);
}

void printgoalactual()
{
  // print serial monitor
  Serial.print("Goal: ");
  Serial.println(goal);
  Serial.print("Actual: ");
  Serial.println(actual);
  Serial.print("Time: ");
  Serial.println(tijd);
  // print matrix
}

void timetel()
{
  // Time counted by timer
  tijd = (millis() - lastTrigger) / 1000;
  // Convert time to hh:mm:ss
  secs = tijd;
  h = secs / 3600;
  rem = secs % 3600;
  m = rem / 60;
  s = rem % 60;
  // Display minutes and seconds
  displaytime = m * 100 + s;
  // Longer as 60 minutes display hour + minutes
  if (h >= 1)
  {
    displaytime = h * 100 + m;
  }
}

void setup_wifi()
{
  delay(20);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi is OK ");
  Serial.print("=> ESP32 new IP address is: ");
  Serial.print(WiFi.localIP());
  Serial.println("");
}

// Reconnect to wifi if connection is lost
void reconnect()
{

  while (!client.connected())
  {
    Serial.print("Connecting to MQTT broker ...");
    if (client.connect("ESP32Client", mqtt_user, mqtt_password))
    {
      Serial.println("OK");
      client.subscribe("takt/goal");
    }
    else
    {
      Serial.print("[Error] Not connected: ");
      Serial.print(client.state());
      Serial.println("Wait 5 seconds before retry.");
      delay(5000);
    }
  }
}
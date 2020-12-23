#include <Arduino.h>
#include <M5Stack.h>
#include <WiFi.h>
#include <Time.h>
#include <HTTPClient.h>
#include <BleKeyboard.h>
#include <ArduinoJson.h>
#include <secrets.h>
#include "Free_Fonts.h"


struct Episode {
  String topic;
  String title;
  String description;
  int timestamp;
  String url;
  int totalResults;
};

BleKeyboard bleKeyboard("Mediathek Shuffler V2.0");
Episode currentEpisode;
int latestCounter = 0;

const String topics[] = {"Soko", "Tatort", "Die Rosenheim-Cops", "Die Bergretter", "Die Chefin", "Wilsberg", "Gib mir irgendetwas!"};
const int numberOfTopics = 7;
int currentIndex = 0;
String topic = "";

char ssid[] = WIFI_SSID;
char pass[] = WIFI_PASS;

void connectToNetwork()
{
  WiFi.begin(ssid, pass);
  while ( WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect to Network named: ");
    Serial.println(ssid); // print the network name (SSID);
    delay(1000);
    WiFi.begin(ssid, pass);
  }

  Serial.println("Connected to the WiFi network");
}

void establishBluetoothConnection()
{
  bleKeyboard.begin();

  while (!bleKeyboard.isConnected())
  {
    Serial.println("Attempting to establishing bluetooth connection...");
    delay(1000);
  }

  Serial.println("Bluetooth connection established!");
}

void showButtonLabels(String btnA, String btnB, String btnC){
  M5.Lcd.fillRect(0, 220, 320, 30, YELLOW);

  M5.Lcd.setFreeFont(FF21);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(BLACK, WHITE);
  
  M5.Lcd.setCursor(40, 235);
  M5.Lcd.print(btnA);

  M5.Lcd.setCursor(130, 235);
  M5.Lcd.print(btnB);

  M5.Lcd.setCursor(235, 235);
  M5.Lcd.print(btnC);
}

String replaceSpecialChars(String text) {
  text.replace("ä", "ae");
  text.replace("ö", "oe");
  text.replace("ü", "ue");
  text.replace("Ä", "Ae");
  text.replace("Ö", "Oe");
  text.replace("Ü", "Ue");
  text.replace("ß", "ss");
  text.replace("\"", "");
  return text;
}

void displayEpisode()
{
  M5.Lcd.clearDisplay();
  showButtonLabels("latest", "random", "play");
  
  M5.Lcd.setFreeFont(FF1);

  //display topic and date
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(0, 25);
  String date = " (" + String(day(currentEpisode.timestamp)) + "." + String(month(currentEpisode.timestamp)) + "." + String(year(currentEpisode.timestamp)).substring(2, 4) + ")";
  M5.Lcd.println(replaceSpecialChars(currentEpisode.topic.substring(0,18)) + date);

  //display title
  M5.Lcd.setCursor(0, M5.Lcd.getCursorY()+20);
  M5.Lcd.setTextSize(2);
  String dsiplayTitle = (currentEpisode.title.length() > 28) ? currentEpisode.title.substring(0, 28) : currentEpisode.title;
  M5.Lcd.println(replaceSpecialChars(dsiplayTitle));

  int maxDescriptionLength = 140;
  if (M5.Lcd.getCursorY() < 120) {
    maxDescriptionLength = 190;
  }
  //display description
  M5.Lcd.setCursor(0, M5.Lcd.getCursorY()-5);
  M5.Lcd.setTextSize(1);
  String displayDescription = (currentEpisode.description.length() > maxDescriptionLength) ? currentEpisode.description.substring(0, maxDescriptionLength) + "..." : currentEpisode.description;
  M5.Lcd.println(replaceSpecialChars(displayDescription));
}

void initDisplay()
{
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(10, 120);
  M5.Lcd.setTextColor(WHITE, BLACK);
  M5.Lcd.setFreeFont(FF1);
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("Getting Ready!");
}

void showEpisodeScreen(String topic = "", int offset = 0)
{
  Serial.println("Fetching episode for topic \"" + topic + "\" with offset " + String(offset));

  HTTPClient http;
  http.begin("https://mediathekviewweb.de/api/query");
  http.addHeader("Content-Type", "text/plain");

  String postData = String("{\"queries\":[{\"fields\":[\"title\",\"topic\"],\"query\":\"") + topic + String("\"}],\"sortBy\":\"timestamp\",\"sortOrder\":\"desc\",\"future\":false,\"offset\":") + String(offset) + String(",\"size\":2}");
  if (topic == "Gib mir irgendetwas!") {
    postData = String("{\"queries\":[],\"sortBy\":\"timestamp\",\"sortOrder\":\"desc\",\"future\":false,\"offset\":") + String(offset) + String(",\"size\":2}");
  }

  int statusCode = http.POST(postData);
  if (statusCode == 200)
  {
    String response = http.getString();
    Serial.println(response);

    StaticJsonDocument<4000> doc;
    DeserializationError error = deserializeJson(doc, response);

    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return;
    }

    int index = 0;
    String tmpTitle = doc["result"]["results"][index]["title"];
    if (tmpTitle.indexOf("Audiodeskription") > 0) {
      index = 1;
      latestCounter +=1;
    }

    String topic = doc["result"]["results"][index]["topic"];
    String title = doc["result"]["results"][index]["title"];
    String description = doc["result"]["results"][index]["description"];
    int timestamp = doc["result"]["results"][index]["timestamp"];
    String url = doc["result"]["results"][index]["url_video_hd"];
    int totalResults = doc["result"]["queryInfo"]["totalResults"];

    currentEpisode = { topic, title, description, timestamp, url, totalResults };
    displayEpisode();

  }
  else
  {
    Serial.println("Error fetching episode");
  }
}

void showSelectTopicScreen() {
  M5.Lcd.clearDisplay();
  showButtonLabels("up", "down", "select");
  M5.Lcd.setFreeFont(FF1);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(WHITE, BLACK);
  for (int i=0; i<numberOfTopics; i++) {
    M5.Lcd.setCursor(0, 20+(i*25));
    M5.Lcd.print(topics[i]);
  }
  M5.Lcd.setTextColor(YELLOW, BLACK);
  M5.Lcd.setCursor(0, 20+(currentIndex*25));
  M5.Lcd.print(topics[currentIndex]);
}

void setup() {
  M5.begin();
  M5.Power.begin();
  Serial.begin(9600);
  Serial.begin(115200);
  initDisplay();
  connectToNetwork();
  establishBluetoothConnection();
  randomSeed(analogRead(0));
  topic = "";
  showSelectTopicScreen();
}

void loop() {
  M5.update();
  if (topic == "") {
    if (M5.BtnA.wasPressed()) {
      currentIndex = (currentIndex - 1) < 0 ? 0 : (currentIndex - 1);
      showSelectTopicScreen();
    }
    if (M5.BtnB.wasPressed()) {
      currentIndex = (currentIndex + 1) > numberOfTopics-1 ? numberOfTopics-1 : (currentIndex + 1);
      showSelectTopicScreen();
    }
    if (M5.BtnC.wasPressed()) {
        topic = topics[currentIndex];
        Serial.println("topic" + topic);
        showEpisodeScreen(topic, 0);
    }
  } else {
    if (M5.BtnA.wasReleased()) 
    {
      int latestMax = currentEpisode.totalResults-1  < 10 ? currentEpisode.totalResults-1 : 10;
      latestCounter = (latestCounter + 1) % latestMax;
      showEpisodeScreen(topic, latestCounter);
    }
    if (M5.BtnA.pressedFor(1500)) 
    {
      topic =  "";
      currentIndex = 0;
      showSelectTopicScreen();
    }
    if (M5.BtnB.wasPressed())
    {
      int totalResults = currentEpisode.totalResults  > 10000 ? 10000 : currentEpisode.totalResults;
      int offset = random(0, totalResults-1);
      showEpisodeScreen(topic, offset);
    }
    if (M5.BtnC.wasPressed())
    {
      bleKeyboard.write(KEY_F5);
      delay(1000);
      for (int i = 0; i < 10; i++)
    {
      bleKeyboard.write(KEY_RIGHT_ARROW);
      delay(100);
    }
      bleKeyboard.write(KEY_RETURN);
      delay(5000);
      String currentUrl = currentEpisode.url;
      for (int i = 0; i < currentUrl.length(); i++)
    {
      bleKeyboard.print(currentUrl[i]);
      delay(100);
    }
      bleKeyboard.write(KEY_RETURN);
    } 
  }
  delay(100);
}
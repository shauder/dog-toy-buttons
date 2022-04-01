#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include "secrets.h"

// Turn on more serial messages for debuging
bool debug_msg = true;

// WiFi variables
const char * wifi_ssid = SECRET_SSID;
const char * wifi_pass = SECRET_PASS;

// MQTT variables
const char * mqtt_client   = SECRET_MQTT_CLIENT;
const char * mqtt_user     = SECRET_MQTT_USER;
const char * mqtt_password = SECRET_MQTT_PASSWORD;
const char * mqtt_server   = SECRET_MQTT_SERVER;
const int  mqtt_port     = SECRET_MQTT_HOST;

// MQTT topic base
const String button_base_topic = "homeassistant/binary_sensor/";

WiFiClient wifiClient;
PubSubClient client(wifiClient);

// Define the button array size
const int button_rows = 6;
const int button_columns = 4;

// Button attribute definitions
// <button_id>, <button_name>, <button_pin>, <button_state>
String button_def[button_rows][button_columns] = { 
    {"button_one", "Button One", "0"}, 
    {"button_two", "Button Two", "12", "0"}, 
    {"button_three", "Button Three", "2", "0"}, 
    {"button_four", "Button Four", "3", "0"},
    {"button_five", "Button Five", "4", "0"},
    {"button_six", "Button Six", "5", "0"} 
  };

void create_mqtt_button_config(String button_id, String button_name)
{
  // Variables for storing the serialized json and the size
  char message[256];
  size_t b = 0;

  // Define the config and state topics
  String button_config_topic = button_base_topic + button_id + "/config";
  String button_state_topic = button_base_topic + button_id + "/state";

  // Define the button config json structure and values
  StaticJsonDocument<256> ButtonConfig;
  ButtonConfig["name"] = button_name;
  ButtonConfig["dev_cla"] = "connectivity";
  ButtonConfig["pl_on"] = "1";
  ButtonConfig["pl_off"] = "0";
  ButtonConfig["stat_t"] = button_base_topic + button_id + "/state";
  ButtonConfig["val_tpl"] = "{{ value_json.state }}";
  ButtonConfig["uniq_id"] = button_id + "_state";

  // Serialize the JSON to the 'message' variable and the size to the 'b' variable
  b = serializeJson(ButtonConfig, message);
  if (debug_msg) {
    Serial.print("\nSize: ");
    Serial.println(b);
    Serial.println("Message:");
    serializeJsonPretty(ButtonConfig, Serial);
  }

  // Publish the message
  client.publish(button_config_topic.c_str(), message, true);

  // Define the initial button state json structure and values
  StaticJsonDocument<256> ButtonState;
  ButtonState["state"] = "0";

  // Serialize the JSON to the 'message' variable and the size to the 'b' variable
  b = serializeJson(ButtonState, message);
  if (debug_msg) {
    Serial.print("\nSize: ");
    Serial.println(b);
    Serial.println("Message:");
    serializeJsonPretty(ButtonState, Serial);
  }

  // Publish the message
  client.publish(button_state_topic.c_str(), message, true);
}

void update_mqtt_button_state(int id, int button_pin, String button_id, String button_name)
{
  // Variables for storing the serialized json and the size
  char message[128];
  size_t b = 0;
  
  // Define the state topics
  String button_state_topic = button_base_topic + button_id + "/state";
  String button_state = "0";

  // Read the state of the current PIN and set the button_state variable accordingly
  if (digitalRead(button_pin) == LOW) {
    button_state = "1";
  } else {
    button_state = "0";
  }

  // If the button_state does not match our current state, send update to MQTT topic
  if (button_state != button_def[id][3]) {
    StaticJsonDocument<256> ButtonState;
    ButtonState["state"] = button_state;
    
    // Serialize the JSON to the 'message' variable and the size to the 'b' variable
    b = serializeJson(ButtonState, message);
    if (debug_msg) {
      Serial.print("\nSize: ");
      Serial.println(b);
      Serial.println("Message:");
      serializeJsonPretty(ButtonState, Serial);
    }
    
    // Publish the message
    client.publish(button_state_topic.c_str(), message);

    // Set the new state in the button array
    button_def[id][3] = button_state;
  }
}

// If we want to add callback and subscribe if we need to check state in realtime
//void mqtt_callback()
//{
//  if (strcmp(topic,"button_one") == 0) {
//    // Get the state of button one
//  }
// 
//  if (trcmp(topic,"button_one") == 0) {
//    // Get the state of button two
//  }
//}

void connect_wifi()
{
  // Check to see if the WiFi is already connected
  if (WiFi.status() != WL_CONNECTED) {
    // Start WiFi connection
    WiFi.begin(wifi_ssid, wifi_pass);
    
    Serial.println("Beginning WiFi connection");
    Serial.print("AP: ");
    Serial.print(wifi_ssid);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

    // Set auto connect and persitent WiFi to true
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);

    Serial.println("WiFi connected successfully");

    // Connect to MQTT once WiFi is connected
    connect_mqtt();
  } else {
    Serial.println("WiFi is connected already");
  }
}

void connect_mqtt()
{
  // Check to see if MQTT is connected already
  if (!client.connected()) {
    // Set the server and port for the MQTT server
    client.setServer(mqtt_server, mqtt_port);
  
    // Connect to the server
    while (!client.connected()) {
      Serial.println("Connecting to MQTT...");
   
      if (client.connect(mqtt_client, mqtt_user, mqtt_password )) {
        client.setBufferSize(1024); // Increase the buffer size from the default 256
        // client.onMessage(mqtt_callback); // Set the MQTT callback function
        Serial.println("You're connected to the MQTT broker!");
      } else {
        Serial.print("failed with state ");
        Serial.println(client.state());
        delay(2000);
      }
    }
  } else {
    Serial.println("MQTT is connected already");
  }
}

void setup()
{
  // Set the serial console
  Serial.begin(115200);
  delay(100);

  Serial.println("---------------------------------------------");

  // Connect to Wi-Fi network
  connect_wifi();

  if (WiFi.status() == WL_CONNECTED) {
    for ( int i = 0; i < button_rows; ++i ) {
      pinMode(button_def[i][2].toInt(), INPUT_PULLUP);
      create_mqtt_button_config(button_def[i][0], button_def[i][1]);
    }
  } else {
    Serial.println("WiFi disconnected, attempting to reconnect");
    connect_wifi();
  }
}

void loop () {
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      Serial.println("MQTT Disconnected");
      Serial.print("failed with state ");
      Serial.print(client.state());
      Serial.println("Attempting to reconnect MQTT");
      
      // Connect to MQTT once WiFi is connected
      connect_mqtt();
    } else {
      client.loop();
    }
    for ( int i = 0; i < button_rows; ++i ) {
      update_mqtt_button_state(i, button_def[i][2].toInt(), button_def[i][0], button_def[i][1]);
    }
  } else {
    Serial.println("WiFi disconnected, attempting to reconnect");
    connect_wifi();
  }
}

#include <sha256.h>
#include <rBase64.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "DHT.h"

// wifi credentials
const char* wifi_ssid = "REDE WIFI";
const char* wifi_password = "senha WIFI";

// example: <myiothub>.azure-devices.net
const char* iothub_url = "URL AZURE IOT HUB";

// primary key device iothub, usada para criar o SAS Token
char* iothub_key = "PRIMARY KEY DEVICE";

// this is the id of the device created in Iot Hub
// example: myCoolDevice
const char* iothub_deviceid = "ID do Dispositivo";

// <myiothub>.azure-devices.net/<myCoolDevice>
const char* iothub_user = "URL AZURE IOT HUB / ID do Dispositivo ";

// default topic feed for subscribing is "devices/<myCoolDevice>/messages/devicebound/#""
const char* iothub_subscribe_endpoint = "devices/ID do Dispositivo/messages/devicebound/#";

// default topic feed for publishing is "devices/<myCoolDevice>/messages/events/"
const char* iothub_publish_endpoint = "devices/ ID do Dispositivo/messages/events/";


WiFiClientSecure espClient;
PubSubClient client(espClient);

DHT sensor_dht(14, DHT22);
long lastMsg = 0;

// function to connect to the wifi
void setup_wifi() {
  delay(10);

  Serial.println();
  Serial.print("Connecting to wifi");

  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // debug wifi via serial
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// function to connect to MQTT server
void connect_mqtt() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    String passwordMqtt = createIotHubSASToken(iothub_key, String(iothub_url),0);
    
    if (client.connect(iothub_deviceid, iothub_user, passwordMqtt.c_str())) {
      Serial.println("connected");
      // ... and resubscribe
      client.subscribe(iothub_subscribe_endpoint);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println("try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// callback function for when a message is dequeued from the MQTT server
void callback(char* topic, byte* payload, unsigned int length) {
  // print message to serial for debugging
  Serial.print("Message arrived: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println('---');
}

void setup() {
  // begin serial for debugging purposes
  Serial.begin(115200);

  Serial.println("Init DHT 22");
  sensor_dht.begin();

  // connect to wifi
  setup_wifi();

  // set up connection and callback for MQTT server
  client.setServer(iothub_url, 8883);
  client.setCallback(callback);

  // connect to MQTT
  connect_mqtt();
}


void loop() {
  client.loop();
  long now = millis();

  // publish data and debug mqtt connection every 10 seconds
  if (now - lastMsg > 10000) {
    lastMsg = now;

    delay(2000); //tempo recomendado para pela lib para efetuar a proxima medicao

    Serial.println("Coleta DHT");
    float coletaTemperatura = sensor_dht.readTemperature();
    float coletaUmidade = sensor_dht.readHumidity();
  
      if (isnan(coletaTemperatura) || isnan(coletaUmidade)) {
        coletaTemperatura = -10000;
        coletaUmidade = -10000;
    
        Serial.println("Falha ao efetuar a leitura do sensor DHT");
    }

    Serial.print("is MQTT client is still connected: ");
    Serial.println(client.connected());

    // set up json object
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();

    root["idDevice"] = String(ESP.getChipId());
    root["temperatura"] = coletaTemperatura;
    root["umidade"] = coletaUmidade;

    // convert json to buffer for publishing
    char buffer[256];
    root.printTo(buffer, sizeof(buffer));

    // publish
    client.publish(iothub_publish_endpoint, buffer);
  }
}


//------------------------------------------------------------------ Azure Create SAS Token

String createIotHubSASToken(char *key, String url, long expire){
   
    url.toLowerCase();
    if (expire == 0) {
        expire = 1737504000; //hardcoded expire
    }

    String stringToSign = url + "\n" + String(expire);
    
    int keyLength = strlen(key);

    int decodedKeyLength = rbase64_dec_len(key, keyLength);
    char decodedKey[decodedKeyLength];  //allocate char array big enough for the base64 decoded key

    rbase64_decode(decodedKey, key, keyLength);  //decode key

    Sha256.initHmac((const uint8_t*)decodedKey, decodedKeyLength);
    Sha256.print(stringToSign);
    char* sign = (char*) Sha256.resultHmac();
    // END: Create signature

    // START: Get base64 of signature
    int encodedSignLen = rbase64_enc_len(HASH_LENGTH);
    char encodedSign[encodedSignLen];
    rbase64_encode(encodedSign, sign, HASH_LENGTH);

    String retorno = "SharedAccessSignature sr=" + url + "&sig="+ urlEncode(encodedSign) + "&se=" + String(expire);
    Serial.print(retorno);

    return retorno;
}

String urlEncode(const char* msg)
{
    const char *hex = "0123456789abcdef";
    String encodedMsg = "";

    while (*msg!='\0'){
        if( ('a' <= *msg && *msg <= 'z')
            || ('A' <= *msg && *msg <= 'Z')
            || ('0' <= *msg && *msg <= '9') ) {
            encodedMsg += *msg;
        } else {
            encodedMsg += '%';
            encodedMsg += hex[*msg >> 4];
            encodedMsg += hex[*msg & 15];
        }
        msg++;
    }
    return encodedMsg;
}

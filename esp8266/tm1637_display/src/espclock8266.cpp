#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266mDNS.h>   //https://tttapa.github.io/ESP8266/Chap08%20-%20mDNS.html
#include <FS.h>            //for file handling
#include <LittleFS.h>      //to access to filesystem
#include "TM1637Display.h"
#include <time.h>
#include <math.h>

//JSON optimizations
#define ARDUINOJSON_SLOT_ID_SIZE 1
#define ARDUINOJSON_STRING_LENGTH_SIZE 1
#define ARDUINOJSON_USE_DOUBLE 0
#define ARDUINOJSON_USE_LONG_LONG 0
#include "ArduinoJson.h"

//NON-blocking timer function (delay() is EVIL)
unsigned long myTimer(unsigned long everywhen){

        static unsigned long t1, diff_time;
        int ret=0;
        diff_time= millis() - t1;
          
        if(diff_time >= everywhen){
            t1= millis();
            ret=1;
        }
        return ret;
}

//GLOBAL vars
const char* ssid;
const char* password;
bool creds_available=false;
bool connected=false;   //wifi connection state

#ifndef DEVICE_ID
#define DEVICE_ID "0000"
#endif

const char *esp_ssid = "ESPclock-" DEVICE_ID;
const char *mdns_name = "espclock-" DEVICE_ID;

//AP pw must be at least 8 chars, otherwise AP won't be customized 
const char *esp_password =  "waltwhite64";

//when true, ESP scan for networks again and overrides the previous networks on net_list
bool newScan = false;

//connection attempts --> when it is set to 0 again, it means pw is wrong
uint8_t attempts = 0;

bool setup_mode = true;           //true = no config saved yet (setup mode); false = normal mode
unsigned long ap_shutdown_start = 0; //millis() snapshot when AP shutdown was scheduled
bool ap_shutdown_pending = false; //true = AP shutdown timer is active

//creating an Asyncwebserver object
AsyncWebServer server(80);

//TM1637 DISPLAY SETUP
#define CLK 5 //D1 pin 5
#define DIO 4 //D2 pin 4

TM1637Display mydisplay(CLK, DIO);
bool colon=true;
bool blink=true;
bool br_auto=false;
bool twelve=false;
uint8_t brightness=7;

const uint8_t SEG_try[]={
  SEG_D | SEG_E | SEG_F | SEG_G,  //t
  SEG_E | SEG_G,                  //r
  SEG_B | SEG_C | SEG_D | SEG_F | SEG_G  //Y
};

const uint8_t SEG_Err[]={
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G, //E
  SEG_E | SEG_G, SEG_E | SEG_G          //rr
};

uint8_t px=4;                   //used by displayAnim()
const uint8_t SEG_WAIT[] = {     //used by displayAnim()
	 SEG_G
};

bool forw = true;               //used by displayAnim()

void displayAnim(void){
   if(myTimer(500)){
        if(forw==true){ // 4 -> 0
            mydisplay.clear();
            mydisplay.setSegments(SEG_WAIT, 1, px); 
            --px;          

            if(px==0){
              forw=false;
            }
        }

        else if(forw==false){ //0 -> 4

            mydisplay.clear();
            mydisplay.setSegments(SEG_WAIT, 1, px); 
            ++px;          

            if(px==3){
            forw= true;
            }
        }
  }
  return;
}
//END TM1637 DISPLAY SETUP

//NTP SETUP
struct tm timeinfo;

/*
void printLocalTime(){
    struct tm timeinfo;

    if(!getLocalTime(&timeinfo)){
      Serial.println("Failed to obtain time 1");
      return;
    }

    Serial.println(&timeinfo, "%H:%M:%S zone %Z %z ");
    //Serial.println(timeinfo.tm_hour); //access to single time vars
}
*/

const char *ntp_addr = "pool.ntp.org";
const char *tz_posix = "UTC0";
bool start_NtpClient = false;


void wifiScan(){
    //---------------------------------------------x
    //start wifiSCAN
    //---------------------------------------------x
    //Serial.println("PHASE 1.1: scanning in STA_MODE ");
    WiFi.disconnect();

    byte n = WiFi.scanNetworks();
    Serial.print(n);
    Serial.println(" network(s) found.");

    //---------------------------------------------x
    //SSIDs found are stored in json
    //sort by RSSI (strongest first) and remove duplicate SSID names
    //---------------------------------------------x
    JsonDocument net_list;
    JsonArray network = net_list["network"].to<JsonArray>();

    bool *used = new bool[n]();

    for(byte picked = 0; picked < n; picked++){
      int bestIndex = -1;
      int32_t bestRssi = -1000;

      for(byte i = 0; i < n; i++){
        if(used[i]){
          continue;
        }

        int32_t currentRssi = WiFi.RSSI(i);
        if(bestIndex == -1 || currentRssi > bestRssi){
          bestIndex = i;
          bestRssi = currentRssi;
        }
      }

      if(bestIndex < 0){
        break;
      }

      used[bestIndex] = true;

      String ssidName = WiFi.SSID(bestIndex);
      if(ssidName.length() == 0){
        continue;
      }

      bool duplicate = false;
      for(JsonVariant entry : network){
        const char* existingSsid = entry["credentials"][0] | "";
        if(ssidName.equals(existingSsid)){
          duplicate = true;
          break;
        }
      }

      if(duplicate){
        continue;
      }

      JsonArray network_n_credentials = network[network.size()]["credentials"].to<JsonArray>();
      network_n_credentials.add(ssidName);
      network_n_credentials.add("");
    }

    delete[] used;

    net_list["found"] = network.size();

    File fx = LittleFS.open("/network_list.json", "w");
    serializeJsonPretty(net_list, fx);
    fx.close();

    WiFi.scanDelete();
}

void checkConfig(void){

    if(LittleFS.exists("/config.json")){
      Serial.println(F("config esists, trying to restore it"));
      creds_available=true;

      File fld = LittleFS.open("/config.json", "r");
      JsonDocument load_cf;
      
      DeserializationError error = deserializeJson(load_cf, fld);

      if (error) {
        fld.close();
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }

      ssid = load_cf[F("ssid")];
      password = load_cf[F("pw")]; 
      
      WiFi.begin(ssid, password);

      //if it restores wifi connection, then it restore the other settings too
      //if it can't restore wifi, then user must go to webUI to make a new config
      while(WiFi.status() != WL_CONNECTED){
          delay(100);
          mydisplay.setSegments(SEG_try, 3, 0);
          //Serial.print("+");

        if(myTimer(3000)){  
          
          ++attempts;
          mydisplay.showNumberDec(attempts, true, 1, 3);
        }

        else if(attempts==4){
          attempts=0;
          creds_available = false;
          //Serial.println(F("Can't connect. Goto webUI"));
          break;
        }
      }

      if(WiFi.status() == WL_CONNECTED){

        attempts=0;
        connected=true;
        setup_mode=false;
        Serial.println("WIFI RESTORED");

        if(load_cf["ntp_ad"].is<const char*>()){
          ntp_addr = strdup(load_cf["ntp_ad"]);
        }

        if(load_cf["tz"].is<const char*>()){
          tz_posix = strdup(load_cf["tz"]);
          configTzTime(tz_posix, ntp_addr);
          start_NtpClient=true;
        }
        else{
          start_NtpClient=false;
          Serial.println(F("Missing timezone in config.json. Re-save time settings from WebUI."));
        }
  
        brightness = (uint8_t)load_cf["br"];
        mydisplay.setBrightness(brightness);
        blink=  load_cf[F("blink")];
        br_auto = load_cf[F("br_auto")];
        twelve= load_cf[F("twelve")];
        fld.close();
      }
  }
  return;
}

//this is called when you request resources from esp webserver that don't exists
void notFound(AsyncWebServerRequest *request){
    request->send(404, "text/plain", "NOT FOUND");
}

void initMDNS(){

  MDNS.end();

  if (MDNS.begin(mdns_name)) {
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("mDNS fail");
  }
}

void setup() {
  Serial.begin(115200);
  
  //display
  mydisplay.setBrightness(7); 
  mydisplay.clear();

  //to format FS
  //LittleFS.format();
  
  if(!LittleFS.begin()){  //LittleFS.begin() can throw Err0
    mydisplay.setSegments(SEG_Err, 3, 0);
    mydisplay.showNumberDec(0, false, 1, 3);
    //Serial.println("An Error has occurred while mounting LittleFS");
    delay(10000);
    return;
  }
  
  //can throw Err1
  if(!LittleFS.exists("/index.html")){
    mydisplay.setSegments(SEG_Err, 3, 0);
    mydisplay.showNumberDec(1, false, 1, 3);
    //Serial.println("\nSetup Html page NOT FOUND!");
    delay(10000);
    return;
  }
  
  checkConfig();
  
  //PHASE1 - AP_STA_MODE + WIFI SCAN
  //here scans for networks, and as already said, networks are then stored in json
  //Serial.println("\nPHASE 1.0: AP_STA_MODE + WIFI SCAN");

  WiFi.mode(WIFI_AP_STA);   
  WiFi.setAutoReconnect(true);

  initMDNS();

  delay(100);

  if(WiFi.status() != WL_CONNECTED){
    wifiScan();
  }
  
  //PHASE 2: start AP only in setup mode (no saved config)
  if(!connected){
    WiFi.softAP(esp_ssid, esp_password, false, 2);     //Starting AP on given credential
  }

  //Route for root index.html
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ 
    request->send(LittleFS, "/index.html", "text/html" ); 
    Serial.println("Connection detected");
  });

  //this is triggered when entering to the webUI after the clock is set. It checks the status of all of the UI elements and updates it
  server.on("/uicheck", HTTP_GET, [](AsyncWebServerRequest *request){
       
    JsonDocument uicheck_json;

    uicheck_json["conn"] = connected;
    uicheck_json["bright"]= brightness; 
    uicheck_json["br_auto"] = br_auto;
    uicheck_json["blink"] = blink;
    uicheck_json["twelve"] = twelve;
    uicheck_json["config"] = (LittleFS.exists("/config.json")) ? 1 : 0;
    uicheck_json["ntp"] = ntp_addr;
    uicheck_json["tz"] = tz_posix;
    uicheck_json["setup_mode"] = setup_mode;
    uicheck_json["ap_ssid"] = esp_ssid;

    String uc_str;
    serializeJson(uicheck_json, uc_str);

    request->send(200,  "application/json", uc_str);
  });

  //client requests list of ssids and server sends it to client
  server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request) {

    File f = LittleFS.open("/network_list.json", "r");

    //checks json integrity
    if(!f) {
      //Serial.println("Error opening /network_list.json");
      request->send(500, "application/json", "{\"error\":\"Failed to open network_list.json\"}");
      f.close();
    }

    else{
      request->send(LittleFS,  "/network_list.json", "application/json");
      newScan = true;
      f.close();
    }
  });

  //---------------------------------------------x
  //client(JS) sends http POST req with wifi credentials (inside the body) to server
  //---------------------------------------------x
  server.on("/sendcreds", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
           
    //deserializes http POST req body (has creds inside) from client
    JsonDocument thebody;
    deserializeJson(thebody, data);

    const char* user_ssid_str = thebody["ssid"];  
    const char* user_pw = thebody["pw"];

    ssid = strdup(user_ssid_str);
    password = strdup(user_pw);
    creds_available = true;
    
    request->send(200, "application/json", "{\"creds\":\"OK\"}");
  }); 

  //refresh SSID list on frontend
  server.on("/refresh", HTTP_GET, [](AsyncWebServerRequest *request) {
           
    File f = LittleFS.open("/network_list.json", "r");

    //check json integrity
    if(!f) {
      Serial.println(F("Error opening /network_list.json"));
      request->send(500, "application/json", "{\"error\":\"Failed to open network_list.json\"}");
      f.close();
      return;
    }

    else{
      request->send(LittleFS,  "/network_list.json", "application/json");
      f.close();
      newScan =true;
    }
  });

  //HTTP GET req from client, in order to know if connection attempt was successful
  server.on("/wifi_status", HTTP_GET, [](AsyncWebServerRequest *request) {
          
    if(attempts == 4){
      creds_available = false;
      //attempts=0;
      Serial.println(password);
      Serial.println(F("handler says: 5 attempts->WRONG PASSWORD - RESET attempts to 0"));
      request->send(200, "application/json", "{\"stat\":\"fail\"}");
    }

    else{
      ++attempts;
      if(WiFi.status() == WL_CONNECTED){
        //attempts=0;
        Serial.println(password);
        String ip = WiFi.localIP().toString();
        String resp = "{\"stat\":\"ok\",\"ip\":\"" + ip + "\",\"mdns\":\"" + String(mdns_name) + "\"}";
        request->send(200, "application/json", resp);
      }

      else{
        Serial.println("PLEASE WAIT");
        request->send(200, "application/json", "{\"stat\":\"wait\"}");
      }
    }
  });


  server.on("/updatetime", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){

    JsonDocument ntp_json;
    deserializeJson(ntp_json, data);

    if(ntp_json["ntp_addr"].is<const char*>()){
      ntp_addr = strdup(ntp_json["ntp_addr"]);
    }

    if(ntp_json["tz"].is<const char*>()){
      tz_posix = strdup(ntp_json["tz"]);
    }

    configTzTime(tz_posix, ntp_addr);
  
    if(start_NtpClient == false){
      start_NtpClient=true;
    }
    
    request->send(200, "application/json", "{\"ntp\":\"OK\"}");
  });

  server.on("/setup_timezone", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){

    JsonDocument tz_json;
    deserializeJson(tz_json, data);

    if(tz_json["tz"].is<const char*>()){
      tz_posix = strdup(tz_json["tz"]);
    }

    // Update config.json with chosen timezone
    if(LittleFS.exists("/config.json")){
      File fr = LittleFS.open("/config.json", "r");
      JsonDocument saved_cf;
      deserializeJson(saved_cf, fr);
      fr.close();
      saved_cf[F("tz")] = tz_posix;
      saved_cf.shrinkToFit();
      File fw = LittleFS.open("/config.json", "w+");
      serializeJsonPretty(saved_cf, fw);
      fw.close();
    }

    configTzTime(tz_posix, ntp_addr);
    start_NtpClient = true;

    // Schedule AP shutdown after 15-second grace period
    ap_shutdown_start = millis();
    ap_shutdown_pending = true;

    request->send(200, "application/json", "{\"status\":\"ok\"}");
  });


  server.on("/slider", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
          
          //optimization: maybe i shouldn't use JSON for this (?)
          JsonDocument bgt_json;
          deserializeJson(bgt_json, data);

          //extract light value
          brightness =(uint8_t)atoi(bgt_json["bgt"]);
          mydisplay.setBrightness(brightness); 
          //Serial.println((uint8_t)atoi(bgt_json["bgt"]));
          request->send(200, "application/json", "{\"status\":\"BGT OK\"}");
  });

  
  server.on("/br_auto", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){

            JsonDocument br_auto_json;
            deserializeJson(br_auto_json, data);
            br_auto = br_auto_json["br"];

            /*to get single time vars 
            Serial.println("Time variables");
            char timeHour[3];
            strftime(timeHour,3, "%H", &timeinfo); https://cplusplus.com/reference/ctime/strftime/*/
            
            //optimization: should i replace it with a switch-case (?)
            if(timeinfo.tm_hour >= 0 && timeinfo.tm_hour < 9){
              brightness=0;
              mydisplay.setBrightness(0);
              request->send(200, "application/json", "{\"status\":\"0\"}");
            }

            else if(timeinfo.tm_hour >= 7 && timeinfo.tm_hour < 17){
              brightness=6;
              mydisplay.setBrightness(6);
              request->send(200, "application/json", "{\"status\":\"6\"}");
            }

            else if(timeinfo.tm_hour >= 17 && timeinfo.tm_hour < 20){
              brightness=3;
              mydisplay.setBrightness(3);
              request->send(200, "application/json", "{\"status\":\"3\"}");
            }

            else if(timeinfo.tm_hour >= 20){
              brightness=2;
              mydisplay.setBrightness(2);
              request->send(200, "application/json", "{\"status\":\"2\"}");
            }
  });

  server.on("/blink", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){

            JsonDocument blink_json;
            deserializeJson(blink_json, data);
            blink = (uint8_t)blink_json["bl"];  //update blink var
            request->send(200, "application/json", "{\"status\":\"updated\"}");
  });

   server.on("/twelve", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){

            JsonDocument twelve_json;
            deserializeJson(twelve_json, data);
            twelve = (uint8_t)twelve_json["tw"];  //update blink var
            request->send(200, "application/json", "{\"status\":\"updated\"}");
  });

  server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){

    JsonDocument scf_json;
    deserializeJson(scf_json, data);
    bool saveconfig = scf_json["save"];
    //Serial.println(saveconfig);

    if(saveconfig==1){ //user wants to save/overwrite config
              
      JsonDocument config;

      if(!setup_mode && LittleFS.exists("/config.json")){
        // Normal mode: read existing Wi-Fi credentials from file to preserve them
        File existing = LittleFS.open("/config.json", "r");
        JsonDocument existing_cf;
        deserializeJson(existing_cf, existing);
        existing.close();
        config[F("ssid")] = existing_cf[F("ssid")];
        config[F("pw")] = existing_cf[F("pw")];
      } else {
        config[F("ssid")] = ssid;           //const *char
        config[F("pw")] = password;         //const *char
      }
      config[F("ntp_ad")] = ntp_addr;     //const *char
      config[F("tz")] = tz_posix;         //POSIX timezone string
      config[F("br_auto")] = br_auto;     //bool as 1 or 0
      config[F("br")] = brightness;       //uint8_t
      config[F("blink")] = blink;        //bool as 1 or 0
      config[F("twelve")] = twelve;      
      config.shrinkToFit();
              
      File fc = LittleFS.open("/config.json", "w+");

      //serializes json and passes it to "fc" var, in order to store it in FS 
      serializeJsonPretty(config, fc);
      fc.close();
      Serial.println(F("\nCONFIG SAVED"));
    }

                
    else if(LittleFS.exists("/config.json") && saveconfig==0){     //if user wants to delete config
              
      LittleFS.remove("/config.json");
     
      WiFi.disconnect();
      connected=false;
      creds_available = false;
      start_NtpClient=false;
      attempts=0;
      setup_mode=true;
      ap_shutdown_pending=false;
      WiFi.mode(WIFI_AP_STA);
      WiFi.softAP(esp_ssid, esp_password, false, 2);
      Serial.println(F("\n*Config.json DELETED*"));
    }

    request->send(200, "application/json", "{\"status\":\"updated\"}");
  });

  server.onNotFound(notFound);

  //start server
  server.begin();
}


void loop() {
  
  MDNS.update();

  //shut down AP after setup mode transition (15 second grace period for user to read IP/mDNS)
  if(ap_shutdown_pending && (millis() - ap_shutdown_start) >= 15000UL){
    WiFi.mode(WIFI_STA);
    ap_shutdown_pending = false;
  }
  if(newScan==true){
    wifiScan();
    newScan=false;
  }

  if(start_NtpClient==true){
    getLocalTime(&timeinfo);
   
    if(myTimer(1000)){
        //printLocalTime();

        if(br_auto==true){
             
            switch(timeinfo.tm_hour){
    
              case 0 || 00: 
              brightness=0;
              mydisplay.setBrightness(0);
              break;

              case 9:
              brightness=6;
              mydisplay.setBrightness(6);
              break;

              case 17:
              brightness=3;
              mydisplay.setBrightness(3);
              break;

              case 20: //maybe i can remove this one and put brightness=2 at 17:00
              brightness=2; 
              mydisplay.setBrightness(2);
              break;
            }
        }   
           
        
        if(blink==1){
            if(colon==true){   //colon is ON
              if(!twelve){
                mydisplay.showNumberDecEx(timeinfo.tm_hour, 0b01000000, false, 2, 0);
                mydisplay.showNumberDecEx(timeinfo.tm_min, 0b01000000, true, 2, 2);
              }

              else{
                if(timeinfo.tm_hour <= 12){
                  mydisplay.showNumberDecEx(timeinfo.tm_hour, 0b01000000, false, 2, 0);
                 
                }

                else{
                  mydisplay.showNumberDecEx(timeinfo.tm_hour-12, 0b01000000, false, 2, 0);
                }

                mydisplay.showNumberDecEx(timeinfo.tm_min, 0b01000000, true, 2, 2);
              }

           
              colon=false;  
          }

          else if(colon==false){
            //colon is OFF
            
              if(!twelve){
                mydisplay.showNumberDecEx(timeinfo.tm_hour, 0, false, 2, 0);
                mydisplay.showNumberDecEx(timeinfo.tm_min, 0, true, 2, 2);
              }

              else{ //if 12hr mode is active
                if(timeinfo.tm_hour <= 12){
                  mydisplay.showNumberDecEx(timeinfo.tm_hour, 0, false, 2, 0);
                }
                
                else{
                  mydisplay.showNumberDecEx(timeinfo.tm_hour-12, 0, false, 2, 0);
                }

                mydisplay.showNumberDecEx(timeinfo.tm_min, 0, true, 2, 2);
              }

            colon=true;
          }
        }
        
        else{
          mydisplay.showNumberDecEx(timeinfo.tm_hour, 0b01000000, false, 2, 0);
            mydisplay.showNumberDecEx(timeinfo.tm_min, 0b01000000, true, 2, 2);
        }
        
    }
  }     

  else{
    displayAnim();
  }


  //optimization: instead of using "bool connected", i can only use WL_CONNECTED
  if(connected == false && creds_available == true ){
    
    displayAnim();
    /* Serial.print("SSID chosen: ");
    Serial.println(ssid);
    Serial.print("PW is: ");
    Serial.println(password); */

    WiFi.begin(ssid, password);
    
    while(1){

      displayAnim();
            
      //cycles here until it's connected to wifi
      if (WiFi.status() != WL_CONNECTED && creds_available==true){
          delay(200);
      }
    
      //once connected, exit form while(1) with break, and then from first if since "connected==true" now
      else if(WiFi.status() == WL_CONNECTED){
        connected = true;
        initMDNS();

        //first-time setup: auto-save credentials + defaults, defer NTP/AP-shutdown to /setup_timezone
        if(setup_mode){
          JsonDocument config;
          config[F("ssid")] = ssid;
          config[F("pw")] = password;
          config[F("ntp_ad")] = ntp_addr;
          config[F("tz")] = tz_posix;
          config[F("br_auto")] = br_auto;
          config[F("br")] = brightness;
          config[F("blink")] = blink;
          config[F("twelve")] = twelve;
          config.shrinkToFit();
          File fc = LittleFS.open("/config.json", "w+");
          serializeJsonPretty(config, fc);
          fc.close();
          setup_mode = false;
        }
        break;
      }

      else if(attempts == 4){
        attempts=0;  //reset "attempts", so it can try a new connection
        creds_available=false;
        Serial.println("RESET Attempts from LOOP");
        Serial.println(password);
        break; //exit from while(1)
      }
    }
  }
}

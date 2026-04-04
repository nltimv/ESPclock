#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFiClient.h>
#include <FS.h>            //for file handling
#include <LittleFS.h>      //to access to filesystem
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <TM1652.h>
#include <TM16xxDisplay.h>

//JSON optimizations
#define ARDUINOJSON_SLOT_ID_SIZE 1
#define ARDUINOJSON_STRING_LENGTH_SIZE 1
#define ARDUINOJSON_USE_DOUBLE 0
#define ARDUINOJSON_USE_LONG_LONG 0

//NON-blocking timer function (delay() is EVIL). only accepts milliseconds
unsigned long myTimer(unsigned long everywhen){ //millis overflow-safe!

        static unsigned long t1, diff_time;
        bool ret=0;
        diff_time= millis() - t1;
          
        if(diff_time >= everywhen){
            t1= millis();
            ret=1;
        }
        return ret;
}

//4294967295 ms == 49d 17h 5m

const char* ssid;
const char* password;
bool creds_available=false;
bool connected=false;   //wifi connection state

const char *esp_ssid = "ESPclock32";
const char *esp_password =  "waltwhite64"; //AP pw must be at least 8 chars, otherwise AP won't be customized 

bool newScan = false; //if true, ESP scans for networks again and overrides the previous networks on net_list
uint8_t attempts = 0; //connection attempts --> when it's set to 0 again, it means pw is wrong

AsyncWebServer server(80);

//TM1652 DISPLAY SETUP

TM1652 module(6, 4);                 //module(GPIOpin, n_ofdigits); --> creates the low-level driver object for the TM1652 chip
TM16xxDisplay display(&module, 4);   //TM16xxDisplay display(&module, n_ofdigits);

// 7-segment character map (A-F, 0-9, space, dash)
const byte SEG_CHAR_MAP[] = {
  0x77, // A
  0x7C, // b
  0x39, // C
  0x5E, // d
  0x79, // E
  0x71, // F
  0x3F, // 0
  0x06, // 1
  0x5B, // 2
  0x4F, // 3
  0x66, // 4
  0x6D, // 5
  0x7D, // 6
  0x07, // 7
  0x7F, // 8
  0x6F, // 9
  0x00, // space
  0x40  // -
};

//global vars
bool colon=true;
bool blink=true;
bool br_auto=false;
bool twelve=false;
uint8_t brightness=7;
uint8_t ms_ovfl=0;

uint8_t px=4;  
bool forw = true;               //used by displayAnim()

void displayAnim(void){
   if(myTimer(500)){
        if(forw==true){ // 4 -> 0
          display.clear();
          module.setSegments(0x40, px);
          --px;          
          if(px==0){
            forw=false;
          }
        }

        else if(forw==false){ //0 -> 4
          display.clear();
          module.setSegments(0x40, px);
          ++px;          

          if(px==3){
            forw= true;
          }
        }
  }
  return;
}

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
}*/
const char *ntp_addr;
int gmt_offset;
bool start_NtpClient = false;

//ALARM setup
uint8_t hh, mm; //hour and minutes

//all entries are initialized to 0
bool days[7] = {0};   //in this case mon=days[0], tue=days[1], wed=days[2], thu=days[3], fri=days[4], sat=days[5], sun=days[6]

/*should i use 
  switch(currentday)
    case ???*/

/*
arrays are guaranteed to be contiguous.(there's no gap between elements) 
while structs are not, so it may be that a struct wastes more memory.
THEN, ARRAY WINS.
oppure creare un struttura days con dentro i giorni (less confusing to handle than array)
struct week{
  bool mon=0;
  bool tue=0;
  bool wed=0;
  bool thu=0;
  bool fri=0;
  bool sat=0;
  bool sun=0;
}
*/

uint8_t snooze;
//uint8_t ringtone;


void wifiScan(){
    //---------------------------------------------x
    //start wifiSCAN
    WiFi.disconnect();

    byte n = WiFi.scanNetworks();
    Serial.print(n);
    Serial.println(" network(s) found. Displaying the first 5");

    //---------------------------------------------x
    //SSIDs found are stored in json
    //arduinoJson7 doesn't use static/dynamicJsonDocument anymore, but it only uses JsonDocument
    //---------------------------------------------x    
      
    //If json doesn't exists yet, it creates it
    if(!LittleFS.exists("/network_list.json")){
        JsonDocument net_list;
        //Serial.println("Network list doesn't exists. Creating it now..."); 🟠

        //if the number of networks found is <5 (so from [0-4])...
        if(n<5){
            
            //stores number of found networks in json
            net_list["found"] = n;
            JsonArray network = net_list["network"].to<JsonArray>();

            for(byte j = 0; j < n; j++){
              JsonArray network_n_credentials = network[j]["credentials"].to<JsonArray>();
              network_n_credentials.add(WiFi.SSID(j));
              network_n_credentials.add("");
            }
        }

        //if it finds >5 networks, it will display only the top five networks, with index: [0-4]
        else{
          
          net_list["found"] = 5;
          JsonArray network = net_list["network"].to<JsonArray>();

          for(byte j = 0; j < 5; j++){
              JsonArray network_n_credentials = network[j]["credentials"].to<JsonArray>();
              network_n_credentials.add(WiFi.SSID(j));
              network_n_credentials.add("");
          }
      }

      //---------------------------------------------x
      //After creating JSON file (jsondocument), it must be stored in FS
      //---------------------------------------------x
      File fx = LittleFS.open("/network_list.json", "w");

      //serializes json and passes it to "fx" var
      serializeJsonPretty(net_list, fx);
      fx.close();
    }


    //---------EXISTING JSON---------------------
    //2. IF JSON ALREADY EXISTS: access to json, reset it, then add new networks to it
    else{
      //Serial.println("Network list already exists! Updating it..."); 🟠
      JsonDocument net_listUp;
     
      //1. fetch and open json from FS, then deserializes it
      File fxup = LittleFS.open("/network_list.json", "w+");
      deserializeJson(net_listUp, fxup);

      //if there are n<5 networks
      if(n<5){
        //updates the values of the entries of the older one
        net_listUp["found"] = n;
        JsonArray network = net_listUp["network"].to<JsonArray>();

        for(byte k = 0; k < n; k++){
            JsonArray network_n_credentials = network[k]["credentials"].to<JsonArray>();
            network_n_credentials.add(WiFi.SSID(k));
            network_n_credentials.add("");    //
        }
      }

      //if there are n>5 networks -> it truncates the list to only 5 ssids
      else{
            net_listUp["found"] = 5;
            JsonArray network = net_listUp["network"].to<JsonArray>();
            
            for(byte k = 0; k < 5; k++){
              //dynamically adds, to each entry "k", a new array to the main array "network"
              JsonArray network_n_credentials = network[k]["credentials"].to<JsonArray>();

              //adds SSID name to the entry[k][0]
              network_n_credentials.add(WiFi.SSID(k));

              //adds pw field (initially empty) to entry[k][1] 
              network_n_credentials.add("");
            }
      }

      //3. serializing
      serializeJsonPretty(net_listUp, fxup);
      fxup.close();
    }     
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
          delay(50);
          //Serial.print("+");
          display.setDisplayToString("trY", 0, 0);

        if(myTimer(3000)){  
          
          ++attempts;
          module.setDisplayDigit(attempts,3,false);  // show number 7 at position 1 with dot: 7.
         
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
        Serial.println("WIFI RESTORED");

        start_NtpClient=true;
        ntp_addr= strdup(load_cf["ntp_ad"]); 
        gmt_offset = load_cf["offset"]; 
        //Serial.println("NTP server: " + String(ntp_addr));
        //Serial.println("OFFSET: " + String(gmt_offset));
        configTime(gmt_offset*3600, 3600, ntp_addr);
  
        brightness = (uint8_t)load_cf["br"];
        module.setupDisplay(true, brightness, 6);
        //display.setIntensity(brightness);
        
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
  if (MDNS.begin("espclock")) {
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("mDNS fail");
  }
}

void setup() {
  
  Serial.begin(115200);
  module.begin(true, 4, 6);   //4=brightness level

  //LittleFS.format();

  //Begin LittleFS can throw Err0
  if(!LittleFS.begin()){
    display.setDisplayToString("Err", 0, 0);
    module.setDisplayDigit(0,3,false);
    Serial.println("An Error has occurred while mounting LittleFS");
    delay(10000);
    return;
  }
  
  //can throw Err1
  if(!LittleFS.exists("/index.html")){
    display.setDisplayToString("Err", 0, 0);
    module.setDisplayDigit(1,3,false);
    Serial.println("\nSetup Html page NOT FOUND!");
    delay(10000);
    return;
  }
  
  checkConfig();
  
  //PHASE1 - AP_STA_MODE + WIFI SCAN
  //here scans for networks, and as already said, networks are then stored in json

  WiFi.mode(WIFI_AP_STA);   
  WiFi.setAutoReconnect(true);
  initMDNS();
  delay(100);

  if(WiFi.status() != WL_CONNECTED){
    wifiScan();
  }
  
  //---------------------------------------------x
  //PHASE 2: here user choose its ssid and enters pw
  WiFi.softAP(esp_ssid, esp_password, false, 2);     //Starting AP on given credential

  //Serial.print("AP IP address: ");  🟠
  //Serial.println(WiFi.softAPIP());  🟠           //Default AP-IP is 192.168.4.1
  //Serial.println(esp_ssid);         🟠

  //Route for root index.html
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ 
    request->send(LittleFS, "/index.html", "text/html" ); 
    Serial.println("Device detected");
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
    uicheck_json["millis"] = millis();
    uicheck_json["msovfl"] = ms_ovfl;

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
      request->send(500, "application/json", "{\"error\":\"Can't open network_list.json\"}");
      f.close();
    }

    else{
      request->send(LittleFS,  "/network_list.json", "application/json");
      newScan = true;
      f.close();
    }
  });

  //client(JS) sends http POST req with wifi credentials (inside the body) to server
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
        request->send(200, "application/json", "{\"stat\":\"ok\"}");
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

    ntp_addr = strdup(ntp_json["ntp_addr"]); 
    gmt_offset = (int)atoi(ntp_json["offset"]);
    configTime(gmt_offset*3600, 3600, ntp_addr); 
    
      Serial.println("NTP server: " + String(ntp_addr));
        Serial.println("OFFSET: " + String(gmt_offset));

    if(start_NtpClient == false){
      start_NtpClient=true;
    }
    
    request->send(200, "application/json", "{\"ntp\":\"OK\"}");
  });

  server.on("/slider", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
          
          //optimization: maybe i shouldn't use JSON for this (?)
          JsonDocument bgt_json;
          deserializeJson(bgt_json, data);

          //extract light value
          brightness =(uint8_t)atoi(bgt_json["bgt"]);
          module.setupDisplay(true, brightness, 6);
          //display.setIntensity(brightness);
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
              module.setupDisplay(true, brightness, 6);
              request->send(200, "application/json", "{\"status\":\"0\"}");
            }

            else if(timeinfo.tm_hour >= 7 && timeinfo.tm_hour < 17){
              brightness=6;
              module.setupDisplay(true, brightness, 6);
              request->send(200, "application/json", "{\"status\":\"6\"}");
            }

            else if(timeinfo.tm_hour >= 17 && timeinfo.tm_hour < 20){
              brightness=3;
              module.setupDisplay(true, brightness, 6);
              request->send(200, "application/json", "{\"status\":\"3\"}");
            }

            else if(timeinfo.tm_hour >= 20){
              brightness=2;
              module.setupDisplay(true, brightness, 6);
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

  /*
  server.on("/alarm", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  
  });*/

  /*
  server.on("/timer", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  });*/

  server.on("/config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){

    JsonDocument scf_json;
    deserializeJson(scf_json, data);
    bool saveconfig = scf_json["save"];

    if((!LittleFS.exists("/config.json") && saveconfig==1)){ //user wants to save config
              
      JsonDocument config;

      config[F("ssid")] = ssid;           //const *char
      config[F("pw")] = password;         //const *char
      config[F("ntp_ad")] = ntp_addr;     //const *char
      config[F("offset")] = gmt_offset;   //offset saved as int
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
      Serial.println(password);
      Serial.println(F("\n*Config.json DELETED*"));
    }

    request->send(200, "application/json", "{\"status\":\"updated\"}");
  });

  server.on("/uptime", HTTP_GET, [](AsyncWebServerRequest *request) {
    /*
    String json= "{";
    json+= "\"ms\":\"" + String(millis()) + "\",";
    json += "\"msovfl\":\""+ String(ms_ovfl) +"\"";
    json += "}";
    request->send(200, "application/json", json); */
    request->send(200, "application/json", "{\"ms\":\""+ String(millis()) +"\",\"msovfl\":\""+ String(ms_ovfl) + "\"}"); 
  });

  server.onNotFound(notFound);

  //start server
  server.begin();
}

char hour[2];
char minutes[2];

void loop() {
  
  if(millis() == 4294967295){
    ms_ovfl++;  //can lead to a bug because uint8_t max value is 255, but it'll reach this value after 50days*256= 35years of activity
  }

  if(newScan==true){
    wifiScan();
    newScan=false;
  }

  if(start_NtpClient==true){

    /*The getLocalTime() function has an optional timeout parameter in milliseconds 
    (with the default value being 5 seconds).  This timeout value is used in case there
    is an NTP server request being made in the background
    (for example when you just called the configTime() function before calling the getLocalTime()
    function like in this example sketch).   getLocalTime(&timeinfo, 5000);

    If needed, the getLocalTime() function will
    wait until either a valid system time is received or until the timeout occurs.  The function will return false, if no valid system time was received before the timeout duration occurs.*/
    getLocalTime(&timeinfo);

    if(myTimer(1000)){
        //printLocalTime();

        if(br_auto==true){
             
            switch(timeinfo.tm_hour){
    
              case 0 || 00: 
              brightness=0;
              module.setupDisplay(true, brightness, 6);
              break;

              case 9:
              brightness=6;
              module.setupDisplay(true, brightness, 6);
              break;

              case 17:
              brightness=3;
              module.setupDisplay(true, brightness, 6);
              break;

              case 20: //maybe i can remove this one and put brightness=2 at 17:00
              brightness=2; 
              module.setupDisplay(true, brightness, 6);
              break;
            }
        }   
           
        
        if(blink==1){
            if(colon==true){   //colon is ON
              if(!twelve){  
                //module.setDisplayToDecNumber((timeinfo.tm_hour*100)+timeinfo.tm_min, 0x04, true);
                display.setDisplayToDecNumber((timeinfo.tm_hour*100)+timeinfo.tm_min, 0x04, true);
              }

              //12hr format is on
              else{
                if(timeinfo.tm_hour <= 12){
                  //module.setDisplayToDecNumber((timeinfo.tm_hour*100)+timeinfo.tm_min, 0x04, true);
                  display.setDisplayToDecNumber((timeinfo.tm_hour*100)+timeinfo.tm_min, 0x04, true);
                }

                else{
                 // module.setDisplayToDecNumber(((timeinfo.tm_hour-12)*100)+timeinfo.tm_min, 0x04, true);
                  display.setDisplayToDecNumber(((timeinfo.tm_hour-12)*100)+timeinfo.tm_min, 0x04, true);
                }
              }
              colon=false;  
          }

          else if(colon==false){  //colon is OFF

              if(!twelve){
                //module.setDisplayToDecNumber((timeinfo.tm_hour*100)+timeinfo.tm_min, 0, false);
                display.setDisplayToDecNumber((timeinfo.tm_hour*100)+timeinfo.tm_min, 0, false);
              }

              //if 12hr mode is active
              else{ 
                if(timeinfo.tm_hour <= 12){
                  display.setDisplayToDecNumber((timeinfo.tm_hour*100)+timeinfo.tm_min, 0, true);
                  //module.setDisplayToDecNumber((timeinfo.tm_hour*100)+timeinfo.tm_min, 0, true);
                }
                else{
                  //module.setDisplayToDecNumber(((timeinfo.tm_hour-12)*100)+timeinfo.tm_min, 0, true);
                  display.setDisplayToDecNumber(((timeinfo.tm_hour-12)*100)+timeinfo.tm_min, 0, true);
                }
              }

            colon=true;
          }
        }
        
        //if blink==0
        else{

          if(!twelve){
            display.setDisplayToDecNumber((timeinfo.tm_hour*100)+timeinfo.tm_min, 0x04, true);
          }

          //if 12hr mode is active
              else{ 
                if(timeinfo.tm_hour <= 12){
                  display.setDisplayToDecNumber((timeinfo.tm_hour*100)+timeinfo.tm_min, 0x04, true);
                }
                
                else{
                  display.setDisplayToDecNumber(((timeinfo.tm_hour-12)*100)+timeinfo.tm_min, 0x04, true);
                }
              }
        }
        
    }
  }     

  else{
    displayAnim();
  }

  //optimization: instead of using "bool connected", i can only use WL_CONNECTED
  if(connected == false && creds_available == true ){
    
    displayAnim();
    WiFi.begin(ssid, password);
    
    while(1){

      displayAnim();
            
      //cycles here until it's connected to wifi
      if (WiFi.status() != WL_CONNECTED && creds_available==true){
          delay(200);
      }
    
      //once connected, exit form while(1) with break, and then from first if since "connected==true" now
      else if(WiFi.status() == WL_CONNECTED){
        //configTime(gmt_offset*3600, 3600, ntp_addr);
        connected = true;
        initMDNS();
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

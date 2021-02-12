//Biblioteca para uso da rede WiFi
#include<ESP8266WiFi.h> //Para uso no ESP8266
//#include<WiFi.h> //Para uso no ESP32
//Biblioteca para uso do sensor Max30100
#include "MAX30100_PulseOximeter.h"
//Biblioteca para uso do protocolo  MQTT
#include <PubSubClient.h>

//DEFINES:
#define LM35 A0 // define o valor de LM35 como pino analógico A0
#define TEMPO_LEITURA_MS 5000 // 5 Segundo de intervalo entre as leituras dos sensores Max30100 e lm35
#define TEMPO_ENVIO_MQTT 20000 //17  segundo é o intervalo mínimo para o envio de mensagens para o broker mqtt - thingspeak

//INICIALIZAÇÃO DE BIBLIOTESCAS E CLASSES:
  //MAX30100_PulseOximeter.h
    PulseOximeter pox; //Inizialização da Biblioteca "MAX30100_PulseOximeter.h" 
  //WiFi
    WiFiClient client ;
    PubSubClient MQTT(client);//inicializar o broker mqtt, enviando o objeto da classe wifi

//VARIAVEIS GLOBAIS
  unsigned long ultimaLeitura = 0; //Utilizado para definir o intervalo entre as leituras dos sensores Max30100 e Lm35.
  unsigned long ultimoEnvio = 0; // Utilizado para definir o tempo da última publicação ao broker
  float temperatura = 0.0;

  //MAX30100_PulseOximeter.h
    uint8_t SpO2 = 0;
    float frequenciaCardiaca = 0.0;
 
  //WiFi
    const char* ssid = "redewifi";
    const char* password = "senha";
  //MQTT
    //char* topic="channels/<channelID/publish/<channelAPI>
    char* topic = "channels/1257190/publish/DMA3KADRZZ78LX9X"; //canal e chave que pode ser adicionado em qq aplicativo compatível com a plataforma Thingspeak. 
    const char* server = "mqtt.thingspeak.com";
    long ultimaMsg = 0;
    char msg[50];
    int valor = 0;
    bool enviar;
    
//PROTÓTIPOS DAS FUNÇÕES / INTERFACES:
  //Temperatura
    float getTemperaturaCelsius();
  //MAX30100_PulseOximeter.h
    void onBeatDetected();//Mostra mensagem na detecção de batimentos
  //WiFi
    void initWiFi();//Primeira conexão com rede wifi
    void connectWiFi();//Conexão com a Rede wifi
  //MQTT
    void callback(char* topic, byte* dados, unsigned int length); //Função que retorna o que foi enviado pelo protocolo mqtt
    void initMQTT();
    void connectMQTT();
    void publishMQTT(float temp, uint8_t spO2, float fCardiaca);
    
void setup()
{
     Serial.begin(115200);
    Serial.println("Inicializando Sistema....");
     
    initWiFi();
    initMQTT();
      
    if (!pox.begin()) {
        Serial.println("[MAX30100]--FAILED");
        for(;;);
    } else {
        Serial.println("[MAX30100]--SUCCESS");
    }
    //Define a corrente, que por padrão é 50mA, neste caso estamos usando 7.6 mA
    pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA);
    //Define a função callback do sensor
    pox.setOnBeatDetectedCallback(onBeatDetected);
}

void loop() {
 //Atualizar os dados do Oximetro
 pox.update();
 enviar = false;
 if (millis() - ultimaLeitura > TEMPO_LEITURA_MS){
  if(pox.begin()){
    //Serial.println("\n[MAX 30100] LENDO OS SINAIS VITAIS \n");
   }else {
    Serial.println("[Max 30100] FALHA ");
    ESP.restart();// na falha reinicia o dispositivo
   }
   ultimaLeitura = millis();
   //Aquisição dos sinais vitais
   temperatura = getTemperaturaCelsius();
   SpO2 = pox.getSpO2();
   frequenciaCardiaca = pox.getHeartRate();
   //Mostrando os valores na porta serial
   //Serial.println();
   //Serial.println("T["+ String(temperatura) + "] SpO2[" + String(SpO2) + "] bpm["+String(frequenciaCardiaca)+"]");
   if(temperatura != 0 && SpO2 != 0 && frequenciaCardiaca !=0){
    enviar= true;
   // Serial.println("************************************");
   //Serial.println("[DADOS VALIDOS] ");
   //Serial.println("T["+ String(temperatura) + "] SpO2[" + String(SpO2) + "] bpm["+String(frequenciaCardiaca)+"]");
   //Serial.println("************************************");    
   }
 }

 if(millis() - ultimoEnvio > TEMPO_ENVIO_MQTT && enviar){
  Serial.println("\n[Enviando para o broker]\n");
  pox.shutdown();
  ultimoEnvio = millis();
  publishMQTT(temperatura, SpO2, frequenciaCardiaca);
 }
 
}


//FUNÇÕES
//Retorna temperatura
float getTemperaturaCelsius(){
  float milivolts = analogRead(LM35)* (3300/1024);
  return (milivolts/10);

}


//MAX30100
void onBeatDetected(){
 Serial.print("Tum!");
}

//WIFI
void initWiFi(){
  WiFi.begin(ssid, password);
  connectWiFi();
}

void connectWiFi(){
    while (WiFi.status() != WL_CONNECTED){
      Serial.print(".");
      delay(2500);
    }
    Serial.print("\nConectada a rede WiFi:  ");
    Serial.println(WiFi.localIP());  
}

//MQTT
void initMQTT(){
  MQTT.setServer(server, 1883);
  MQTT.setCallback(callback);
}

void connectMQTT(){
  while (!MQTT.connected()){    
    if(MQTT.connect("ESP-ThingSpeak")){
      Serial.print("[CONECTADO AO BROKER: " + String(server) + " ] ");
    }else{
      Serial.println("Falha ao reconectar no Broker!");
      Serial.println("Nova Tenativa em 2 segundos");
      delay(2000);
    }
  }
}

void publishMQTT(float temp, uint8_t spO2, float fCardiaca){
  //Encapsulamento dos dados para envio...
  String dados = "field1=";
  dados += fCardiaca;
  dados += "&field2=";
  dados += spO2;
  dados += "&field3=";
  dados += temp;
  dados += "&status=MQTTPUBLISH";

   if (MQTT.connected()){
    //Serial.println("[Eniando dados para a plataforma ThingSpeak] ");
    //Serial.println(dados);
    }else {//Caso esteja desconectado do Broker, é realizada nova conexão.
      Serial.println("[CONECTANDO AO BROKER....]");
      connectMQTT();
    }
    //Mantendo o servido conectado
    MQTT.loop();

    //Publicando os dados no broker
    if (MQTT.publish(topic, (char*) dados.c_str())) {
      Serial.println("\n---------------------------------------------------------");
      Serial.println("[MQTT - PUBLISH OK] ");
      Serial.println(">>>DADOS ENVIADOS => T["+String(temp) + "] SpO2[" + String(spO2)+ "%] Bpm[" + String(fCardiaca)+"]");
      Serial.println("---------------------------------------------------------");
      MQTT.disconnect();
    }else{ 
      Serial.println("---------------------------------------------------------");
      Serial.println("[MQTT - NO PUBLISH] FALHA NA PUBLICACAO\n\n");
      Serial.println("---------------------------------------------------------");
   }
}

void callback(char* topic, byte* dados, unsigned int length){
  //RETORNO DO QUE FOI ENVIADO PELO PROTOCOLO MQTT - mostrar na serial
}
 

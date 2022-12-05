/*
 *******************************************************************************
 * Atividade Avaliativa IT012 Alessandra de Jesus Santos Freitas
 * DOIT ESP32 DEVKIT V1
 *
 * Leitura do sensor de g_temperature & g_humidity DHT11.
 * Mostra os valores mensurados no display I²C OLED SSD1306.
 *
 *******************************************************************************
 */

/*******************************************************************************
    Inclusões
*******************************************************************************/
#include <Arduino.h>
#include "DHT.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Fonts/FreeSerif9pt7b.h>
#include <WiFi.h>
#include "LittleFS.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "ThingSpeak.h"

/*******************************************************************************
  Definições e variáveis globais
*******************************************************************************/
//Wifi
const char* WIFI_SSID = "AN-SANFREI";
const char* WIFI_PASSWORD = "020963";
WiFiClient client;

// Configurações para acesso à um canal predefinido da ThingSpeak
unsigned long THINGSPEAK_CHANNEL_ID = 1968882;
const char *THINGSPEAK_WRITE_API_KEY = "K2NYWO98G9HPDFWA";
const char *THINGSPEAK_READ_API_KEY = "1PPAMQCJWYJDC4Y8";

// Cria objeto do Webserver na porta 80 (padrão HTTP)
AsyncWebServer server(80);

// Variáveis para armazenar valores obtidos da página HTML
String g_ssid;
String g_password;
String g_channel;
String g_key;
String g_dispositivo;

// Caminhos dos arquivos criados durante a execução do exemplo,
// para salvar os valores das credenciais da Wifi,channel,key e name
const char *g_ssidPath = "/ssid.txt";
const char *g_passwordPath = "/password.txt";
const char *g_channelPath = "/channel.text";
const char *g_keyPath = "/key.text";
const char *g_dispositivoPath = "/dispositivo.text";

// Sensor DHT11
#define DHT_READ (15) // pino de leitura do sensor
#define DHT_TYPE DHT11 // tipo de sensor utilizado pela lib DHT
DHT dht(DHT_READ, DHT_TYPE); // Objeto de controle do DHT11
float g_temperature;
float g_humidity;

// Display OLED SSD1306
#define OLED_WIDTH (128) // largura do display OLED (pixels)
#define OLED_HEIGHT (64) // altura do display OLED (pixels)
#define OLED_ADDRESS (0x3C) // endereço I²C do display
static Adafruit_SSD1306 display // objeto de controle do SSD1306
    (OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// Controle de temporização periódica
unsigned long g_previousMillis = 0;
const long g_interval = 30000; // mínimo intervalo para free tier do ThingSpeak: 30000


/*******************************************************************************
  Implementação: Funções auxiliares
*******************************************************************************/
void littlefsInit()
{
  if (!LittleFS.begin(true))
  {
    Serial.println("Erro ao montar o sistema de arquivos LittleFS");
    return;
  }
  Serial.println("Sistema de arquivos LittleFS montado com sucesso.");
}

// Lê arquivos com o LittleFS
String readFile(const char *path)
{
  Serial.printf("Lendo arquivo: %s\r\n", path);

  File file = LittleFS.open(path);
  if (!file || file.isDirectory())
  {
    Serial.printf("\r\nfalha ao abrir o arquivo... %s", path);
    return String();
  }

  String fileContent;
  while (file.available())
  {
    fileContent = file.readStringUntil('\n');
    break;
  }
  return fileContent;
}

// Escreve arquivos com o LittleFS
void writeFile(const char *path, const char *message)
{
  Serial.printf("Escrevendo arquivo: %s\r\n", path);

  File file = LittleFS.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.printf("\r\nfalha ao abrir o arquivo... %s", path);
    return;
  }
  if (file.print(message))
  {
    Serial.printf("\r\narquivo %s editado.", path);
  }
  else
  {
    Serial.printf("\r\nescrita no arquivo %s falhou... ", path);
  }
}

// Callbacks para requisições de recursos do servidor
void serverOnGetRoot(AsyncWebServerRequest *request)
{
  request->send(LittleFS, "/index.html", "text/html");
}

void serverOnGetStyle(AsyncWebServerRequest *request)
{
  request->send(LittleFS, "/style.css", "text/css");
}

void serverOnGetFavicon(AsyncWebServerRequest *request)
{
  request->send(LittleFS, "/favicon.png", "image/png");
}

void serverOnPost(AsyncWebServerRequest *request)
{
  int params = request->params();

  for (int i = 0; i < params; i++)
  {
    AsyncWebParameter *p = request->getParam(i);
    if (p->isPost())
    {
      if (p->name() == "ssid")
      {
        g_ssid = p->value().c_str();
        Serial.print("SSID definido como ");
        Serial.println(g_ssid);

        // Escreve WIFI_SSID no arquivo
        writeFile(g_ssidPath, g_ssid.c_str());
      }
	  
      if (p->name() == "password")
      {
        g_password = p->value().c_str();
        Serial.print("Senha definida como ");
        Serial.println(g_password);
        // Escreve WIFI_PASSWORD no arquivo
        writeFile(g_passwordPath, g_password.c_str());
      }

	    if (p->name() == "channel")
      {
        g_channel = p->value().c_str();
        Serial.print("Channel definido como ");
        Serial.println(g_channel);

        // Escreve CHANNEL_ID no arquivo
        writeFile(g_channelPath, g_channel.c_str());
		  }

	    if (p->name() == "key")
      {
        g_channel = p->value().c_str();
        Serial.print("Key definida como ");
        Serial.println(g_key);

        // Escreve KEY no arquivo
        writeFile(g_keyPath, g_key.c_str());
		  }

	    if (p->name() == "dispositivo")
      {
        g_channel = p->value().c_str();
        Serial.print("Nome do Dispositivo definido como ");
        Serial.println(g_dispositivo);
        
        // Escreve NOME DO DISPOSITIVO no arquivo
        writeFile(g_dispositivoPath, g_dispositivo.c_str());
      }       
    }
  }

  // Após escrever no arquivo, envia mensagem de texto simples ao browser
  request->send(200, "text/plain", "Finalizado - o ESP32 vai reiniciar e se conectar ao seu AP definido.");

  // Reinicia o ESP32
  delay(2000);
  ESP.restart();
}

// Inicializa a conexão Wifi
bool initWiFi()
{
  // Se o valor de g_ssid for não-nulo, uma rede Wifi foi provida pela página do
  // servidor. Se for, o ESP32 iniciará em modo AP.
  if (g_ssid == "")
  {
    Serial.println("SSID indefinido (ainda não foi escrito no arquivo, ou a leitura falhou).");
    return false;
  }

  // Se há um SSID e PASSWORD salvos, conecta-se à esta rede.
  WiFi.mode(WIFI_STA);
  WiFi.begin(g_ssid.c_str(), g_password.c_str());
  Serial.println("Conectando à Wifi...");

  unsigned long currentMillis = millis();
  g_previousMillis = currentMillis;

  while (WiFi.status() != WL_CONNECTED)
  {
    currentMillis = millis();
    if (currentMillis - g_previousMillis >= g_interval)
    {
      Serial.println("Falha em conectar.");
      return false;
    }
  }

  // Exibe o endereço IP local obtido
  Serial.println(WiFi.localIP());
  
  //  Iniciar ThingSpeak
  ThingSpeak.begin(client);
  Serial.println("ThingSpeak Iniciado.");

  return true;
}

esp_err_t sensorRead()
{
    
    //Leitura do DHT11
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    if (isnan(humidity) || isnan(temperature))
    {
      Serial.printf("\r\n[sensorRead] DHT11 - leitura inválida...");
      return ESP_FAIL;
    }

    Serial.printf("\r\n[sensorRead] Temperatura: %2.2f°C", temperature);
    Serial.printf("\r\n[sensorRead] Umidade: %2.2f %%", humidity);
    

    g_temperature = temperature;
    g_humidity = humidity;
    
    return ESP_OK;
}

esp_err_t updateChannel()
{
    // Lê dados do sensor e publica se a leitura não falhou
    if (sensorRead() == ESP_OK)
    {
        // Envia dados à plataforma ThingSpeak. Cada dado dos sensores é setado em um campo (field) distinto.
        int errorCode;
        ThingSpeak.setField(1, g_temperature);
        ThingSpeak.setField(2, g_humidity);      
        //errorCode = ThingSpeak.writeFields((long)THINGSPEAK_CHANNEL_ID, THINGSPEAK_WRITE_API_KEY);
        errorCode = ThingSpeak.writeFields((long)g_channelPath, g_keyPath);
        if (errorCode != 200)
        {
            Serial.println("Erro ao atualizar os canais - código HTTP: " + String(errorCode));
            return ESP_FAIL;
        }
    }
    // Leitura falhou; apenas retorna
    return ESP_OK;
}


/*******************************************************************************
    Implementação
*******************************************************************************/
void setup()
{
  /*******************************************************************************
    A partir daqui é wifi webserver
  *******************************************************************************/
  // Log inicial da placa
  Serial.begin(115200);
  Serial.print("\r\n ---  webserver_provising --- \n");

  // Inicia o sistema de arquivos
  littlefsInit();

  // Configura LED_BUILTIN (GPIO2) como pino de saída
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // Carrega os valores lidos com o LittleFS
  g_ssid = readFile(g_ssidPath);
  g_password = readFile(g_passwordPath);
  g_channel = readFile(g_channelPath);
  g_key = readFile(g_keyPath);
  g_dispositivo = readFile(g_dispositivoPath);
  
  Serial.println(g_ssid);
  Serial.println(g_password);
  Serial.println(g_channel);
  Serial.println(g_key);
  Serial.println(g_dispositivo);

  // Inicializa o sensor DHT11
  dht.begin();
  
  
  if (!initWiFi())
  {
    // Seta o ESP32 para o modo AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Access Point criado com endereço IP ");
    Serial.println(WiFi.softAPIP());

    // Callbacks da página principal do servidor de provisioning
    server.on("/", HTTP_GET, serverOnGetRoot);
    server.on("/style.css", HTTP_GET, serverOnGetStyle);
    server.on("/favicon.png", HTTP_GET, serverOnGetFavicon);
    server.on("/", HTTP_POST, serverOnPost);

    // Define o modo de operação da Wifi e callbacks de eventos de conexão  
    //WiFi.mode(WIFI_STA);
    //WiFi.onEvent(wifiStationConnected, ARDUINO_EVENT_WIFI_STA_CONNECTED);
    //WiFi.onEvent(wifiGotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP);

    //WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    //Serial.println("Iniciando conexão");

    // Ao clicar no botão "Enviar" para enviar as credenciais, o servidor receberá uma
    // requisição do tipo POST, tratada a seguir
   
    // Como ainda não há credenciais para acessar a rede wifi,
    // Inicia o Webserver em modo AP
    server.begin();

    // Limpa a tela do display e mostra o nome do exemplo
    display.clearDisplay();
    display.setCursor(0, 0);

    // Inicializa o display OLED SSD1306
    display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
    display.setTextColor(WHITE);  
    display.print(g_dispositivo);
  }    
}

void loop()
{
    unsigned long currentMillis = millis();

    if (WiFi.status() == WL_CONNECTED && WiFi.getMode() == WIFI_MODE_STA)
    {
     // A cada "g_interval" ms, atualiza os canais criados na plataforma da
     // ThingsPeak, se huver uma conexão Wifi ativa
      if ((currentMillis - g_previousMillis >= g_interval) && (WiFi.status() == WL_CONNECTED))
      {
        g_previousMillis = currentMillis;
        updateChannel();
      }
   
      // Log da g_temperature e g_humidity pela serial Monitor
      Serial.printf("[DHT11] ");
      Serial.printf("g_humidity: %0.2f ", g_humidity);
      Serial.printf("g_temperature: %0.2f °C\r\n", g_temperature);

      // Limpa a tela do display e mostra o nome do exemplo
      display.clearDisplay();

      // Mostra nome do dispositivo
      display.setCursor(0, 0);
      display.printf("%s", g_dispositivo);

      // Mostra g_temperature no display OLED
      display.drawRoundRect(0, 16, 72, 40, 6, WHITE);
      display.setCursor(4, 20);
      display.printf("Temperatura");
      display.setCursor(4, 40);
      display.setFont(&FreeSerif9pt7b);
      display.printf("%0.1f", g_temperature);
      display.printf(" C");
      display.setFont();

      // Mostra g_humidity no display OLED
      display.drawRoundRect(74, 16, 54, 40, 6, WHITE);
      display.setCursor(80, 20);
      display.printf("Umidade");
      display.setCursor(78, 40);
      display.setFont(&FreeSerif9pt7b);
      display.printf("%0.1f", g_humidity);
      display.printf("%%");
      display.setFont();

      // Atualiza tela do display OLED
      display.display();

    }
}
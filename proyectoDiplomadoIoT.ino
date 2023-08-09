/*
   Codigo para el proyecto GuardianGas del Diplomado del Internet de las Cosas 2023
   Escrito por Luis Felipe Saldivar
   Fecha: 5 de agosto, 2023
   Placa de desarrollo: DOIT ESP32 DEVKIT1
   
   Este programa lee los datos del sensor de gas LP MQ6
   y los envía por MQTT hacia un flow de Node-Red que
   muestra en una gráfica el nivel de gas reportado al
   momento, así como también una gráfica histórica,
   además de contar con la posibilidad de realizar consultas
   sobre las alertas reportadas por el sistema cuando
   se detecte un nivel anormalmente alto en la concentración
   de gas LP. Además el sistema cuenta con alertas por Telegram
   para ser indicado al momento.
*/
///////////////////// Dependencias necesarias para hacer funcionar el código /////////////////////////
/*
  Librería ESP32Servo por Kevin Harrington Versión 0.13.0
  Librería UniversalTelegramBot por Brian Lough Versión 1.3.0
  Librería ArduinoJson por Benoit Blanchon Versión 6.19.4
  Librería PubSubClient para MQTT por Nick O'Leary Versión 2.8.0
 */
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <PubSubClient.h> // Biblioteca MQTT


// Variables del sensor y actuadores
#define MQ6_PIN 34
#define LED 21
#define buzzer 19
#define servo_pin 18
Servo micro;  // iniciamos el objeto servo poniendo un nombre

//////////////////////////* Variables para la lectura del sensor de gas */////////////////////////////////
float R0 = 9.83; // Valor de la resistencia del sensor en aire limpio
float sensor_volt;
float RS_gas; // Valor de la resistencia del sensor en presencia de gas
float ratio; // Relación RS/R0
float ppm_log;
float ppm;   // Concentracion de gas en partes por millón(ppm)
float val;
int valAlerta = 1; // Variable para registrar la alerta de peligro en flow de Node-Red

/////////// Credenciales de tu Wi-Fi y servidor de MQTT ///////////
const char* ssid = "************";
const char* password = "************";
const char* mqtt_server = "************";

// Variables para el manejo de tiempo
double timeLast, timeNow; // Variables para el control de tiempo no bloqueante
long lastReconnectAttempt = 0; // Variable para el conteo de tiempo entre intentos de reconexión
double WAIT_MSG = 5000;  // Espera de 5 segundo entre mensajes
double RECONNECT_WAIT = 5000; // Espera de 5 segundos entre conexiones

// Temas MQTT
const char* mqttGuardian = "TuTema/GuardianGas";   // Puedes cambiarlo a tu necesidad

/////////// Iniciar el objeto WiFiClient para la biblioteca de MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// Inicializar el Bot de Telegram //
#define BOTtoken "**************************************"  // Este es tu TOKEN para el bot, puedes solicitarlo desde BotFather en Telegram
#define CHAT_ID "**********"   // Al igual que el token BotFather te proporciona este dato
WiFiClientSecure client2;  // Iniciar el objeto WiFiClientSecure para el bot de Telegram
UniversalTelegramBot bot(BOTtoken, client2);  // Iniciar el objeto UniversalTeleGramBot con los argumentos del Token y wificlient
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

// Función de reconexión. Devuelve valor booleano para indicar exito o falla
boolean reconnect() {
  Serial.print("Intentando conexión MQTT...");
  // Generar un Client ID aleatorio
  String clientId = "ESP32Client-";
  clientId += String(random(0xffff), HEX);
  // Intentar conexión
  if (client.connect(clientId.c_str())) {
    // Una vez conectado, dar retroalimentación
    client.publish(mqttGuardian,"Conectado");
  } else {
      // En caso de error
      Serial.print("Error rc=");
      Serial.print(client.state());
      Serial.println(" Intentando de nuevo");
      // Esperar 1 segundo
      delay(1000);
    }
  return client.connected();
}

void setup() {
  Serial.begin(115200);     // Iniciamos comunicación serie
  pinMode(MQ6_PIN, INPUT);  // GPIO del sensor como entrada
  pinMode(LED, OUTPUT);     // Alerta visual
  pinMode(buzzer, OUTPUT);  // Alerta sonora
  micro.attach(servo_pin);
  digitalWrite(LED, HIGH);  // Nos aseguramos de tener los actuadores apagados
  digitalWrite(buzzer, HIGH);
  // Intentar conexión WiFi
  Serial.print("Conectando: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  client2.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Conectar bot de telegram y verificar el certificado

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  client.setServer(mqtt_server, 1883);

  bot.sendMessage(CHAT_ID, "Bot iniciado utiliza el comando /start para comenzar", "");
}

void loop() {
  alarma(); // Función para monitorear loa datos del sensor y emitir las alertas

  /////////// Codigo no bloqueante para recibir los mensajes enviados desde Telegram
  if (millis() > lastTimeBotRan + botRequestDelay)  {
    sendSensor();
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }

  /////////// Enviar mensajes por MQTT ////////////
  // Funcion principal de seguimiento de tiempo
  timeNow = millis();

  // Función de conexion y seguimiento
  if (!client.connected()) { // Se pregunta si no existe conexión
    if (timeNow - lastReconnectAttempt > RECONNECT_WAIT) { // Espera de reconexión
      // Intentar reconectar
      if (reconnect()) {
        lastReconnectAttempt = timeNow; // Actualización de seguimiento de tiempo
      }
    }
  } else {
    // Funcion de seguimiento
    client.loop();
  }
  
  // Enviar un mensaje cada WAIT_MSG
  if (timeNow - timeLast > WAIT_MSG && client.connected() == 1) { // Manda un mensaje por MQTT cada cinco segundos
    timeLast = timeNow; // Actualización de seguimiento de tiempo

    //Se construye el string correspondiente al JSON que contiene el valor en ppm del sensor más el estado de la alerta, además del usuario que envia las alertas
    String json = "{\"id\":\"maquina\",\"ppm\":"+String(ppm)+",\"estado\":"+String(valAlerta)+"}";
    Serial.println(json); // Se imprime en monitor solo para poder visualizar que el string esta correctamente creado
    int str_len = json.length() + 1;//Se calcula la longitud del string
    char char_array[str_len];//Se crea un arreglo de caracteres de dicha longitud
    json.toCharArray(char_array, str_len);//Se convierte el string a char array    
    client.publish(mqttGuardian, char_array); // Esta es la función que envía los datos por MQTT, especifica el tema y el valor
  }// fin del if (timeNow - timeLast > wait)
}

// Maneja lo que sucede cuando entra un mensaje nuevo
void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i = 0; i < numNewMessages; i++) {   // Lee el contenido del mensaje y lo reconstruye
    // Id del chat que envía mensajes
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID) {                 // Verifica que el valor del Chat ID corresponda para enviar mensajes
      bot.sendMessage(chat_id, "Usuario no autorizado", "");
      continue;
    }

    // Imprime el mensaje recibido
    String text = bot.messages[i].text;
    Serial.println(text);

    String from_name = bot.messages[i].from_name;
    /////////// Mensaje de bienvenida ///////////
    if (text == "/start") {
      String welcome = "Bienvenido, " + from_name + ".\n";
      welcome += "Usa los siguientes comandos para testear y monitorear el sistema.\n\n";
      welcome += "/led_on para encender led \n";
      welcome += "/led_off para apagar led \n";
      welcome += "/buzz_on para testear buzzer \n";
      welcome += "/buzz_off para apagar buzzer \n";
      welcome += "/abre para testear servo \n";
      welcome += "/cierra para cerrar servo \n";
      welcome += "/info para solicitar estado del sensor de gas \n";
      bot.sendMessage(chat_id, welcome, "");
    }

    /////////// Condicionales para testar cada actuador del sistema en caso de requerirlo
    if (text == "/led_on") {
      bot.sendMessage(chat_id, "LED encendido", "");
      digitalWrite(LED, LOW);
    }

    if (text == "/led_off") {
      bot.sendMessage(chat_id, "LED apagado", "");
      digitalWrite(LED, HIGH);
    }
    if (text == "/buzz_on") {
      bot.sendMessage(chat_id, "buzzer encendido", "");
      digitalWrite(buzzer, LOW);
    }
    if (text == "/buzz_off") {
      bot.sendMessage(chat_id, "buzzer apagado", "");
      digitalWrite(buzzer, HIGH);
    }
    if (text == "/abre") {
      bot.sendMessage(chat_id, "servo abierto", "");
      micro.write(110);
    }
    if (text == "/cierra") {
      bot.sendMessage(chat_id, "servo cerrado", "");
      micro.write(02);
    }

    if (text == "/info") {   // Solicita el valor actual del sensor usando la funcion sendSensor()
      String readings = sendSensor();
      bot.sendMessage(chat_id, readings, "");
    }
  }
}

String sendSensor() {
  /////////// Función para leer el valor del sensor y crear un string para enviar al bot
  val = analogRead(MQ6_PIN);
  sensor_volt = val / 4095 * 3.3; // Conversion a voltaje
  RS_gas = (5.0 - sensor_volt) / sensor_volt; // Conversion a resistencia del sensor
  ratio = RS_gas / R0; // Calculo de la relación RS/R0

  ppm_log = (log10(ratio) - 2.3) / -1.36; // Fórmula de la hoja de datos para LPG
  ppm = pow(10, ppm_log);
  String info = String(ratio) + "Rs/Ro \t";
  info += String(ppm) + "ppm \n";
  return info;
}

void alarma() {
  if (ppm >= 100.00) {    // Si la concentracion de gas(ppm) supera este límite se emite la notificación a Telegram con el Bot
    bot.sendMessage(CHAT_ID, "¡ALERTA! Niveles de Gas LP No seguros");
    valAlerta = 2;  // Cambiamos el valor de la alarma
    for (int i = 0; i <= 10; i++) {  // Hacemos un bucle para encender y apagar el led y buzzer.
      digitalWrite(LED, LOW);
      digitalWrite(buzzer, LOW);
      delay(500);
      digitalWrite(LED, HIGH);
      digitalWrite(buzzer, HIGH);
      delay(500);
    }
    micro.write(110);  // Abrir servo
  } else {
    valAlerta = 1;  // Valor normal si no se detecta peligro
    digitalWrite(LED, HIGH);
    digitalWrite(buzzer, HIGH);
    micro.write(0);   // Cerrar servo
  }
}

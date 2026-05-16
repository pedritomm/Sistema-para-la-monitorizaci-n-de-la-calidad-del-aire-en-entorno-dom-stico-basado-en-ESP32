
/*

Autor: Pedro Muela Maldonado
TFG: Sistema para la monitorización de la calidad del aire en entorno doméstico basado en ESP32
Grado Ingeniería Electrónica Industrial

*/

#include <Arduino.h> // Librería para el uso del arduino

#include <iostream>
#include <WiFi.h>             // Libreria Wifi
#include <WiFiManager.h>      //Libreria para pedir credenciales de wifi
#include <Preferences.h>      //Guardar valores en la memoria flash
#include <MQTTPubSubClient.h> // Libreria MQTT

#include "Buffer.h" // libreria para el buffer, de entrada y salida de datos (creada por mi)

#include <Wire.h> //Librería para I2C
#include <SPI.h>  //Libreria para SPI

// Librerias para el uso del BME680

#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

// El MQ5 no es necesario librerias

#include <LiquidCrystal_I2C.h> // Libreria para el uso de la pantalla LCD

LiquidCrystal_I2C lcd(0x27, 16, 2); // Inicia el LCD en la dirección 0x27, con 16 caracteres y 2 líneas

// Declaracion de los pines de MQ5

const int MQ5_AO = 2;

// Declaracion de los pines BME680

const int BME_SCK = 0;
const int BME_MISO = 6;
const int BME_MOSI = 1;
const int BME_CS = 5;

// Declaración del pin del zumbador

const int pinZumb = 7;

// Declaracion los pines de los botones

const int boton_1 = 4;
const int boton_2 = 3;
const int boton_3 = 10;

// Inicializacion del BME680 para comunicacion SPI

Adafruit_BME680 bme(BME_CS, BME_MOSI, BME_MISO, BME_SCK);

// Variables del buffer

Buffer buffer(20); // Tamaño del buffer

// Variables del BME680

volatile float temp = 0; // En ºC
volatile float hum = 0;  // En %
volatile float pres = 0; // En Pa
volatile float gas1 = 0; // En ohmios

// Variables para MQ9

volatile float gas2 = 0;
volatile float Rs2 = 0;
const float Vcc = 3.3; // Alimentación del sensor
volatile float Vout = 0;
volatile float ratio2 = 0;
volatile float ppm2 = 0; // Partes por millon de
volatile float Ro2 = 0.735;
volatile float GLPejeY = -0.338;
volatile float GLPejeX = 2.434;
volatile float GLPcurva = -0.772;
volatile float exponente = 0;
const float RL2 = 1; // Resistencia interna del sensor
const float factor = 6.5;

// Variables internas del programa

volatile int estado = 0;

volatile int iniWIFI = 0;
volatile int iniMQTT = 0;
volatile int y = 0;
volatile long intervalo = 900000; // 15 min
volatile long luz = 120000;
volatile long luz_anterior = 0;
volatile long anterior = 0;

// Variables de los botones

volatile bool cambio = false;
volatile bool mide = false;
volatile bool alarma = false;
volatile bool encendido = false;

// Credenciales del Wifi

// const char *ssid = "PMM";      // Nombre de la WIFI
// const char *pass = "cacaculo"; // Contraseña de la Wifi

// Credenciales del MQTT

WiFiClient esp32Client;                        // Nombre del cliente
MQTTPubSubClient mqtt;                         // Cliente de mqtt
const char *mqtt_server = "broker.hivemq.com"; // direccion IP del servidor de mqtt
int serverPort = 1883;                         // Puerto de la pagina web
const char *myClientID = "PMM";                // Topic
const char *username = "public";               // Usuario de la web
const char *password = "public";               // Contraseña de la web

void IRAM_ATTR ISR1()
{
    cambio = true;
} // Interrupción para detectar la pulsación del botón de cambio de pantalla

void IRAM_ATTR ISR2()
{
    mide = true;
} // Interrupción para detectar la pulsación del botón de medición

void IRAM_ATTR ISR3()
{
    alarma = true;
    encendido = true;
} // Interrupción para detectar la pulsación del botón de la alarma

void seguridad_WIFI(const char *ssid)
{

    Serial.println("Escaneando redes: ");
    int n = WiFi.scanNetworks(); // Guarda cuantas WIFIs a detectado
    for (int i = 0; i < n; i++)
    {
        Serial.print(WiFi.SSID(i)); // Nombre de la red
        Serial.print(" | Canal: ");
        Serial.print(WiFi.channel(i)); // Tipo de frecuencia
        Serial.print(" | RSSI: ");
        Serial.print(WiFi.RSSI(i)); // Intensidad de la señal
        Serial.print(" | Cifrado: ");
        Serial.println(WiFi.encryptionType(i)); // Tipo de cifrado

        if (String(WiFi.SSID(i)) == String(ssid))
        {

            wifi_auth_mode_t cifrado = (wifi_auth_mode_t)WiFi.encryptionType(i); // Busca en la librería el tipo de seguridad y guarda el nombre en cifrado

            Serial.print("Red encontrada | Cifrado: ");
            Serial.println(WiFi.encryptionType(i));

            switch (cifrado)
            {
            case WIFI_AUTH_OPEN:
                WiFi.setMinSecurity(WIFI_AUTH_OPEN);
                break;

            case WIFI_AUTH_WEP:
                WiFi.setMinSecurity(WIFI_AUTH_WEP);
                break;

            case WIFI_AUTH_WPA_PSK:
                WiFi.setMinSecurity(WIFI_AUTH_WPA_PSK);
                break;

            case WIFI_AUTH_WPA_WPA2_PSK:
                WiFi.setMinSecurity(WIFI_AUTH_WPA2_PSK);
                break;

            case WIFI_AUTH_WPA2_PSK:
                WiFi.setMinSecurity(WIFI_AUTH_WPA2_PSK);
                break;

            case WIFI_AUTH_WPA3_PSK:
                WiFi.setMinSecurity(WIFI_AUTH_WPA3_PSK);
                break;

            case WIFI_AUTH_WPA2_WPA3_PSK:
                WiFi.setMinSecurity(WIFI_AUTH_WPA2_PSK);
                break;

            default:
                WiFi.setMinSecurity(WIFI_AUTH_WPA2_PSK);
                break;
            } // Pone el tipo de seguridad para la WIFI deseada
        }
    } // Recorre todas las WIFIs para saber todas sus caracteristicas
} // Se usa para saber que encriptado tiene la red WiFi a la que hay que conectarse

void conectar_WIFI()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        lcd.noBacklight(); // Apaga la pantalla para ahorrar energia
        /*
                WiFi.mode(WIFI_STA);                // Obliga a la placa a estar en modo sta
                WiFi.enableSTA(true);               // Habilita el modo STA
                WiFi.setTxPower(WIFI_POWER_8_5dBm); // Limitar rango del WIFI
                WiFi.setSleep(false);               // Evita que se apague el WIFI
        */
        WiFi.disconnect();
        WiFi.reconnect(); // Usa las credenciales que ya tiene guardadas
        Serial.print("Conectando a: ");
        Serial.println(WiFi.SSID().c_str()); // Muestras a la señal a la que te quieres conectar
        iniWIFI = 0;
        while ((WiFi.status() != WL_CONNECTED) && (iniWIFI < 500))
        {
            Serial.print("-");
            iniWIFI++;
            delay(100);
        } // Se le da un tiempo para que se establezca la conexión

        Serial.println("");

        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("Illo la conexion no funciono");
            Serial.print("Estado:");
            Serial.println(WiFi.status()); // Dice el porque no se ha conectado
        }

        else
        {
            Serial.println("");
            Serial.println("WiFi conectado");
            Serial.println("Asignación IP: ");
            Serial.println(WiFi.localIP()); // Te dice la ip de la wifi a la que te has conectado
        }
    }
}

void conectar_MQTT()
{
    if (!mqtt.isConnected() && (WiFi.status() == WL_CONNECTED))
    {
        lcd.noBacklight(); // Apaga la luz para ahorrar energía

        esp32Client.connect(mqtt_server, serverPort); // Le dices la pagina web a la que te quieres conectar y el puerto que tiene la pagina web
        mqtt.begin(esp32Client);                      // Vincula que cliente de red debe de usar para conectarse
        Serial.println(" ");
        if (!esp32Client.connected())
        {
            Serial.println("No hay conexión TCP al broker");
        } // Si no se intenta conectar es porque las credenciales estan mal

        Serial.println("Conectando a MQTT broker");

        iniMQTT = 0;

        while ((!mqtt.isConnected()) && ((iniMQTT) < 1000))
        {
            mqtt.connect(myClientID); // Se conecta en la pagina web con el topic, el nombre de usuario, y la contraseña
            Serial.print(".");
            iniMQTT++;
            delay(10);
        }

        Serial.println("");

        if ((iniMQTT) >= 1000)
        {
            Serial.println("Illo no se puede conectar a MQTT, AYUDA");
        }

        if (mqtt.isConnected())
        {
            Serial.println(" ");
            Serial.println("Me he conectado al MQTT broker");
        }

        if (mqtt.isConnected())
        {
            mqtt.subscribe("PMM/Medir", [&](const String &payload, const size_t size)
                           {
                Buffer::Message msg;
                msg.topic = "PMM/Medir"; // Topic al que suscribirse
                msg.payload = payload.c_str(); //Mensaje recibido
                buffer.setReceivedMessage(msg); }); // Para subcribirse al topic saludo
        } // Me subscribo al topic que quiera de MQTT
    }

    mqtt.update(); // Actualiza la conecxión a MQTT
}

void zumbador()
{
    while (!alarma)
    {
        digitalWrite(pinZumb, HIGH);
        delay(1000);
        digitalWrite(pinZumb, LOW);
        delay(1000);
    } // Si el boton de alarma no esta pulsado activa la alarma
    alarma = false; // Vuelve a desactivar el boton de la alarma
}

void envio()
{
    if ((WiFi.status() == WL_CONNECTED) && mqtt.isConnected())
    {
        lcd.noBacklight(); // Apaga la pantalla para ahorrar energia

        Buffer::Message msg;
        msg.topic = "PMM/temp";
        String temp_envio = String(temp, 2);
        msg.payload = String(temp, 2).c_str();
        buffer.setMessageToSend(msg);
        delay(10);

        if (buffer.isSendEmpty() == false)
        {
            Buffer::Message msg = buffer.getMessageToSend();
            Serial.println("Envia mensaje de temperatura");
            mqtt.publish(msg.topic.c_str(), msg.payload.c_str()); // Publicar en el topic tal, el mensaje tal
            delay(100);
        } // Si detecta que hay algo que enviar en la cola. Lo envia.

        msg.topic = "PMM/hum";
        msg.payload = String(hum, 2).c_str();
        buffer.setMessageToSend(msg);
        delay(10);

        if (buffer.isSendEmpty() == false)
        {
            Buffer::Message msg = buffer.getMessageToSend();
            Serial.println("Envia mensaje de humedad");
            mqtt.publish(msg.topic.c_str(), msg.payload.c_str()); // Publicar en el topic tal, el mensaje tal
            delay(100);
        } // Si detecta que hay algo que enviar en la cola. Lo envia.

        msg.topic = "PMM/pres";
        msg.payload = String(pres, 2).c_str();
        buffer.setMessageToSend(msg);
        delay(10);

        if (buffer.isSendEmpty() == false)
        {
            Buffer::Message msg = buffer.getMessageToSend();
            Serial.println("Envia mensaje de presión");
            mqtt.publish(msg.topic.c_str(), msg.payload.c_str()); // Publicar en el topic tal, el mensaje tal
            delay(100);
        } // Si detecta que hay algo que enviar en la cola. Lo envia.

        msg.topic = "PMM/gas1";
        msg.payload = String(gas1, 2).c_str();
        buffer.setMessageToSend(msg);
        delay(10);

        if (buffer.isSendEmpty() == false)
        {
            Buffer::Message msg = buffer.getMessageToSend();
            Serial.println("Envia mensaje de COV");
            mqtt.publish(msg.topic.c_str(), msg.payload.c_str()); // Publicar en el topic tal, el mensaje tal
            delay(100);
        } // Si detecta que hay algo que enviar en la cola. Lo envia.

        msg.topic = "PMM/gas2";
        msg.payload = String(gas2, 2).c_str();
        buffer.setMessageToSend(msg);
        delay(10);

        if (buffer.isSendEmpty() == false)
        {
            Buffer::Message msg = buffer.getMessageToSend();
            Serial.println("Envia mensaje de GLP");
            mqtt.publish(msg.topic.c_str(), msg.payload.c_str()); // Publicar en el topic tal, el mensaje tal
            delay(100);
        } // Si detecta que hay algo que enviar en la cola. Lo envia.
    }
}

void medir_parametros()
{
    temp = bme.temperature - 1;
    hum = bme.humidity;
    pres = bme.pressure / 1000;
    gas1 = bme.gas_resistance / 1000;
    Serial.print("Medido BME680: ");
    Serial.println(gas1);

    if (gas1 >= 200)
    {
        Serial.println("Peligro");

        Buffer::Message msg;
        msg.topic = "PMM/Alarma";
        msg.payload = "Peligro, gases COV demasiado altos";
        buffer.setMessageToSend(msg);
        delay(10);

        if (buffer.isSendEmpty() == false)
        {
            Buffer::Message msg = buffer.getMessageToSend();
            Serial.println("Envia mensaje de peligro");
            mqtt.publish(msg.topic.c_str(), msg.payload.c_str()); // Publicar en el topic tal, el mensaje tal
            delay(100);
        } // Si detecta que hay algo que enviar en la cola. Lo envia.

        zumbador();
    }

    if (!bme.performReading())
    {
        lcd.print("Error BME680");
        Serial.print("Error BME680");
    }

    gas2 = analogRead(MQ5_AO);

    Vout = gas2 * (Vcc / 4095);
    // Serial.print("gas2:");
    // Serial.println(gas2);
    // Serial.print("Vout:");
    // Serial.println(Vout);
    Rs2 = RL2 * ((Vcc - Vout) / Vout);
    // Serial.print("Valor de Rs2: ");
    // Serial.println(Rs2);
    // Serial.println(Ro2);
    ratio2 = Rs2 / Ro2;
    // Serial.print("Valor de ratio: ");
    // Serial.println(ratio2);
    exponente = ((log(ratio2) - GLPejeY) / GLPcurva) + GLPejeX;
    // Serial.print("Valor del exponente: ");
    // Serial.println(exponente);

    ppm2 = pow(10, exponente);

    Serial.print("Medido MQ9: ");
    Serial.println(ppm2);

    if (ppm2 >= 10000)
    {
        Serial.println("Peligro");

        Buffer::Message msg;
        msg.topic = "PMM/Alarma";
        msg.payload = "Peligro, gases GLP demasiado altos";
        buffer.setMessageToSend(msg);
        delay(10);

        if (buffer.isSendEmpty() == false)
        {
            Buffer::Message msg = buffer.getMessageToSend();
            Serial.println("Envia mensaje de peligro");
            mqtt.publish(msg.topic.c_str(), msg.payload.c_str()); // Publicar en el topic tal, el mensaje tal
            delay(100);
        } // Si detecta que hay algo que enviar en la cola. Lo envia.

        zumbador();
    }
    envio();
}

void setup()
{
    Serial.begin(115200);
    delay(5000);

    Wire.begin(8, 9);

    pinMode(MQ5_AO, INPUT);

    pinMode(pinZumb, OUTPUT);

    pinMode(boton_1, INPUT_PULLUP);
    pinMode(boton_2, INPUT_PULLUP);
    pinMode(boton_3, INPUT_PULLUP);

    attachInterrupt(boton_1, ISR1, RISING);
    attachInterrupt(boton_2, ISR2, RISING);
    attachInterrupt(boton_3, ISR3, RISING);

    lcd.init();        // Se inicializa la pantalla LCD
    lcd.noBacklight(); // Se enciende la pantalla

    WiFi.mode(WIFI_STA);                // Obliga a la placa a estar en modo sta
    WiFi.enableSTA(true);               // Habilita el modo STA
    WiFi.setTxPower(WIFI_POWER_8_5dBm); // Limitar rango del WIFI
    WiFi.setSleep(false);               // Evita que se apague el WIFI

    WiFiManager wm;
    wm.setConfigPortalTimeout(180);

    wm.setPreOtaUpdateCallback([&]()
                               { seguridad_WIFI(WiFi.SSID().c_str()); });

    if (!wm.autoConnect("Pinki", "12345678"))
    {
        Serial.println("No conectó, reiniciando...");
        ESP.restart();
    }
    Serial.println("WiFi conectado!");
    Serial.println(WiFi.localIP());

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("");
        Serial.print("WiFi conectado: ");
        Serial.println(WiFi.SSID().c_str());
        Serial.println("Asignación IP: ");
        Serial.println(WiFi.localIP()); // Te dice la ip de la wifi a la que te has conectado
    }

    if (!bme.begin())
    {
        Serial.print("Error con sensor BME680");
        lcd.print("Fallo");
        while (1)
            ;
    }

    // Configuración de oversampling

    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150); // 320°C por 150 ms
    delay(100);

    medir_parametros();
}

void loop()
{
    conectar_WIFI();
    conectar_MQTT();

    if ((millis() - anterior) >= intervalo)
    {
        medir_parametros();
        envio();
        anterior = millis();
    }
    if (encendido)
    {
        lcd.backlight();
        alarma = false;
        encendido = false;
    }

    if ((millis() - luz_anterior) >= luz)
    {
        lcd.noBacklight();
        luz_anterior = millis();
    }

    if ((WiFi.status() == WL_CONNECTED) && mqtt.isConnected())
    {
        if (buffer.isReceptionEmpty() == false)
        {
            Buffer::Message msg = buffer.getReceivedMessage();
            Serial.println(msg.payload.c_str());  // Muestra el mensaje en la pantalla del PC
            String mensaje = msg.payload.c_str(); // Guarda el mensaje en una variable para despues compararla.
            if (mensaje == "todos")
            {
                medir_parametros();
            }
        }
    }

    switch (estado)
    {
    case 0:

        lcd.setCursor(0, 0);
        lcd.print("Temp: ");
        lcd.print(temp);
        lcd.setCursor(13, 0);
        lcd.print(char(223));
        lcd.print("C");

        if (mide)
        {
            medir_parametros();
            lcd.clear();
            mide = false;
        }

        if (cambio)
        {
            estado = 1;
            cambio = false;
            lcd.clear();
            Serial.println("Pasando a siguiente pantalla");
        }

        break;

    case 1:

        lcd.setCursor(0, 0);
        lcd.print("Hum: ");
        lcd.print(hum);
        lcd.setCursor(13, 0);
        lcd.print("%");

        if (mide)
        {
            medir_parametros();
            lcd.clear();
            mide = false;
        }

        if (cambio)
        {
            estado = 2;
            cambio = false;
            lcd.clear();
            Serial.println("Pasando a siguiente pantalla");
        }
        break;

    case 2:

        lcd.setCursor(0, 0);
        lcd.print("Pres: ");
        lcd.print(pres);
        lcd.setCursor(13, 0);
        lcd.print("kPa");

        if (mide)
        {
            medir_parametros();
            lcd.clear();
            mide = false;
        }

        if (cambio)
        {
            estado = 3;
            cambio = false;
            lcd.clear();
            Serial.println("Pasando a siguiente pantalla");
        }

        break;

    case 3:

        lcd.setCursor(0, 0);
        lcd.print("Gas COV: ");
        lcd.print(gas1);
        lcd.setCursor(13, 0);
        lcd.print("Kohm");

        if (mide)
        {
            medir_parametros();
            lcd.clear();
            mide = false;
        }

        if (cambio)
        {
            estado = 4;
            cambio = false;
            lcd.clear();
            Serial.println("Pasando a siguiente pantalla");
        }
        break;

    case 4:

        lcd.setCursor(0, 0);
        lcd.print("Gas: ");
        lcd.print(ppm2);
        lcd.setCursor(13, 0);
        lcd.print("ppm");

        if (mide)
        {
            medir_parametros();
            lcd.clear();
            mide = false;
        }

        if (cambio)
        {
            estado = 0;
            cambio = false;
            lcd.clear();
            Serial.println("Pasando a siguiente pantalla");
        }

        break;
    }
    delay(100);
}
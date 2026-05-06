#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <LittleFS.h>

#define RX_PIN 16
#define TX_PIN 17
#define DE_RE_PIN 4
#define TX_LED 18
#define RX_LED 19

#define BTN_FW 12
#define BTN_DATA 13
#define BTN_DOWN 14
#define BTN_UP 15

#define RS485Serial Serial2
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// I2C 버스 및 디스플레이 객체
TwoWire I2C_One = TwoWire(0);
TwoWire I2C_Two = TwoWire(1);
Adafruit_SSD1306 display1(SCREEN_WIDTH, SCREEN_HEIGHT, &I2C_One, OLED_RESET);
Adafruit_SSD1306 display2(SCREEN_WIDTH, SCREEN_HEIGHT, &I2C_Two, OLED_RESET);

WebServer server(80);
int currentID = 1;

void updateStatusOLED()
{
    display1.clearDisplay();
    display1.setTextColor(SSD1306_WHITE);
    display1.setCursor(0, 0);
    display1.setTextSize(1);
    display1.println("Bridge Tester v1.1");
    display1.println("IP: " + WiFi.softAPIP().toString());
    display1.println("---------------------");
    display1.println("");
    display1.setTextSize(2);
    display1.printf("TARGET ID: %02d", currentID);
    display1.display();
}

void updateChamberOLED(String rawData)
{
    if (rawData.startsWith("$") && rawData.length() >= 27)
    {
        String hexStatus = rawData.substring(3, 7);
        uint16_t statusBits = strtol(hexStatus.c_str(), NULL, 16);

        String temps[4] = {
            rawData.substring(7, 12), rawData.substring(12, 17),
            rawData.substring(17, 22), rawData.substring(22, 27)};

        display2.clearDisplay();
        display2.setTextColor(SSD1306_WHITE);
        display2.setTextSize(1);
        display2.setCursor(0, 0);
        display2.println("[Chamber Status]");
        display2.println("---------------------");

        for (int i = 0; i < 4; i++)
        {
            int shift = (3 - i) * 4;
            uint8_t chStatus = (statusBits >> shift) & 0x0F;
            int waterLvl = (chStatus & 0x08) ? 1 : 0;
            int pumpIn = (chStatus & 0x04) ? 1 : 0;
            int pumpOut = (chStatus & 0x02) ? 1 : 0;

            display2.printf("%d: %s W:%d I:%d O:%d\n", i + 1, temps[i].c_str(), waterLvl, pumpIn, pumpOut);
        }
        display2.display();
    }
}

void displayMessageOLED2(String title, String msg)
{
    display2.clearDisplay();
    display2.setTextColor(SSD1306_WHITE);
    display2.setTextSize(1);
    display2.setCursor(0, 0);
    display2.println(title);
    display2.println("---------------------");
    display2.println("");
    display2.setTextSize(1);
    display2.println(msg); // 긴 문장은 알아서 줄바꿈됨
    display2.display();
}

String handleRS485(String command)
{
    digitalWrite(TX_LED, HIGH);
    while (RS485Serial.available())
        RS485Serial.read();
    digitalWrite(DE_RE_PIN, HIGH);
    RS485Serial.print(command);
    RS485Serial.flush();
    digitalWrite(DE_RE_PIN, LOW);
    digitalWrite(TX_LED, LOW);

    delay(300);

    digitalWrite(RX_LED, HIGH);
    String rawResponse = "";
    unsigned long startMillis = millis();
    while (millis() - startMillis < 1000)
    {
        if (RS485Serial.available())
            rawResponse += (char)RS485Serial.read();
    }
    digitalWrite(RX_LED, LOW);

    String cleanResponse = "";
    for (int i = 0; i < rawResponse.length(); i++)
    {
        if (rawResponse[i] >= 32 && rawResponse[i] <= 126)
            cleanResponse += rawResponse[i];
    }

    if (cleanResponse.length() == 0)
    {
        // Case 1: 응답 없음 (FAILED)
        displayMessageOLED2("[Comm Status]", command + "\n\n-> FAILED");
        Serial.println("Result: FAILED");
    }
    else if (cleanResponse.indexOf("F/W") != -1)
    {
        // Case 2: 펌웨어 정보 (그대로 출력)
        displayMessageOLED2("[Firmware Info]", cleanResponse);
        Serial.println("Result: FW Info Displayed");
    }
    else
    {
        // Case 3: 일반 데이터 (파싱 시도)
        updateChamberOLED(cleanResponse);
        Serial.println("Result: Data Parsed");
    }

    return cleanResponse;
}

void setup()
{
    Serial.begin(115200);
    RS485Serial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

    pinMode(DE_RE_PIN, OUTPUT);
    pinMode(TX_LED, OUTPUT);
    pinMode(RX_LED, OUTPUT);
    digitalWrite(DE_RE_PIN, LOW);

    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_DATA, INPUT_PULLUP);
    pinMode(BTN_FW, INPUT_PULLUP);

    // 2. I2C 및 OLED 초기화 (Panic 방지 핵심)
    I2C_One.begin(21, 22);
    I2C_Two.begin(25, 26);

    if (!display1.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println("OLED 1 Failed");
        while (1)
            ;
    }
    if (!display2.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println("OLED 1 Failed");
        while (1)
            ;
    }

    display1.clearDisplay();
    display1.display();
    display2.clearDisplay();
    display2.display();

    if (!LittleFS.begin(true))
        Serial.println("LittleFS Mount Failed");

    WiFi.softAP("Bridge_Tester", "");
    updateStatusOLED();

    // 4. 웹 서버 라우팅
    server.on("/", HTTP_GET, []()
              {
        if(LittleFS.exists("/index.html")) {
            File file = LittleFS.open("/index.html", "r");
            server.streamFile(file, "text/html");
            file.close();
        } else {
            server.send(404, "text/plain", "File Not Found. Upload index.html!");
        } });

    server.on("/api/send", HTTP_POST, []()
              {
        String command = server.arg("plain");
        String response = handleRS485(command);
        server.send(200, "application/json", "{\"response\":\"" + response + "\"}");
        updateStatusOLED(); });

    server.begin();
    Serial.println("System Ready!");
}

void loop()
{
    server.handleClient();
    
    if (digitalRead(BTN_UP) == LOW)
    {
        currentID = (currentID >= 20) ? 1 : currentID + 1;
        updateStatusOLED();
        delay(200);
    }
    if (digitalRead(BTN_DOWN) == LOW)
    {
        currentID = (currentID <= 1) ? 20 : currentID - 1;
        updateStatusOLED();
        delay(200);
    }
    if (digitalRead(BTN_DATA) == LOW)
    {
        char cmd[16];
        sprintf(cmd, "$$%02d01;", currentID);
        handleRS485(String(cmd));
        delay(300);
    }
    if (digitalRead(BTN_FW) == LOW)
    {
        char cmd[16];
        sprintf(cmd, "$$%02d10;", currentID);
        handleRS485(String(cmd));
        delay(300);
    }
}
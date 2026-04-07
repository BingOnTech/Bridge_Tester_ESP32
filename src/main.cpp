#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------------------------------------------------------
// 1. 설정 영역
// ---------------------------------------------------------
const char *ssid = "Bridge_Tester";
const char *password = "";

// ---------------------------------------------------------
// 1. 32D용 핀 정의 (D16, D17 하드웨어 UART2 사용)
// ---------------------------------------------------------
#define RX_PIN 16   // RX2
#define TX_PIN 17   // TX2
#define DE_RE_PIN 4 // D4
#define TX_LED 18   // D18
#define RX_LED 19   // D19

// RS485 통신에 Serial2 사용 (ESP32-32D 전용)
#define RS485Serial Serial2

// OLED 설정 (I2C 표준 핀 21, 22 사용)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
TwoWire I2C_One = TwoWire(0); // 1번 채널
TwoWire I2C_Two = TwoWire(1); // 2번 채널

Adafruit_SSD1306 display1(SCREEN_WIDTH, SCREEN_HEIGHT, &I2C_One, OLED_RESET);
Adafruit_SSD1306 display2(SCREEN_WIDTH, SCREEN_HEIGHT, &I2C_Two, OLED_RESET);
WebServer server(80);

// ---------------------------------------------------------
// 2. 프론트엔드 HTML (모바일 반응형, 스캔 기능 탑재)
// ---------------------------------------------------------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ko">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>RS485 브릿지 테스트</title>
    <style>
        * { box-sizing: border-box; }
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; padding: 15px; max-width: 800px; margin: auto; background-color: #f0f2f5; color: #333; }
        h2 { font-size: 1.4rem; margin-top: 0; text-align: center; color: #1a1a1a; }
        h3 { font-size: 1.1rem; margin-bottom: 10px; border-bottom: 2px solid #eaeaea; padding-bottom: 5px; }
        .panel { background: white; padding: 15px; border-radius: 10px; box-shadow: 0 4px 6px rgba(0,0,0,0.05); margin-bottom: 15px; }
        .panel-scan { border-left: 5px solid #008CBA; }
        .flex-group { display: flex; flex-wrap: wrap; gap: 8px; margin-bottom: 10px; align-items: center; }
        .flex-group > * { flex: 1 1 120px; }
        input[type="number"], input[type="text"] { padding: 12px; border: 1px solid #ccc; border-radius: 6px; font-size: 1rem; width: 100%; }
        button { padding: 12px 15px; border: none; border-radius: 6px; font-size: 1rem; font-weight: bold; cursor: pointer; background: #e4e6eb; color: #050505; transition: background 0.2s; width: 100%; }
        button:active { transform: scale(0.98); }
        .btn-test { background: #ffffff; border: 1px solid #ced0d4; }
        .btn-test:hover { background: #f2f2f2; }
        .btn-scan { background: #008CBA; color: white; }
        .btn-scan:hover { background: #007399; }
        .btn-stop { background: #ff4d4d; color: white; margin-top: 5px; }
        .btn-stop:hover { background: #cc0000; }
        .log-box { background: #1e1e1e; color: #00ff00; padding: 12px; height: 350px; overflow-y: scroll; font-family: 'Consolas', monospace; border-radius: 8px; font-size: 0.9rem; line-height: 1.4; word-wrap: break-word; }
        .desc-text { font-size: 0.85rem; color: #666; margin-bottom: 10px; line-height: 1.3; }
        @media (max-width: 600px) {
            body { padding: 10px; }
            .panel { padding: 12px; }
            .flex-group > * { flex: 1 1 100%; }
        }
    </style>
</head>
<body>
    <h2>🔌 EdgeNode 테스터기 (ESP32)</h2>
    
    <div class="panel">
        <h3>1. 기본 전송 (수동)</h3>
        <div class="flex-group">
            <button class="btn-test" onclick="sendCommand('$$0101;')">코드 1 ($$0101;)</button>
            <button class="btn-test" onclick="sendCommand('$$0110;')">코드 2 ($$0110;)</button>
        </div>
        <div class="flex-group">
            <input type="text" id="customCmd" placeholder="직접 입력 ($$0110;)">
            <button class="btn-test" style="background: #e4e6eb;" onclick="sendCustom()">수동 전송</button>
        </div>
    </div>

    <div class="panel panel-scan">
        <h3>2. 자동 스캔 모드</h3>
        <p class="desc-text">※ 현장 설치 및 디버깅 시 보드 상태를 연속 확인합니다.</p>
        <div class="flex-group">
            <input type="number" id="singleScanId" placeholder="보드 ID (예: 1)" min="1" max="99">
            <button class="btn-scan" onclick="startSingleScan()">🎯 단일 스캔</button>
        </div>
        <div class="flex-group" style="margin-top: 10px;">
            <button class="btn-scan" style="flex: 2;" onclick="startCyclicScan()">🔄 순회 스캔 (자동 범위감지)</button>
        </div>
        <button class="btn-stop" onclick="stopScan()">🛑 모든 스캔 정지</button>
    </div>

    <h3>수신 로그</h3>
    <div class="log-box" id="logBox"></div>

    <script>
        const logBox = document.getElementById('logBox');
        let isScanning = false;

        function appendLog(text, color = '#00ff00') {
            const time = new Date().toLocaleTimeString();
            logBox.innerHTML += `<div style="color: ${color}">[${time}] ${text}</div>`;
            logBox.scrollTop = logBox.scrollHeight;
        }

        function sleep(ms) { return new Promise(resolve => setTimeout(resolve, ms)); }

        async function sendCommand(cmd, isAutoScan = false) {
            if(!isAutoScan) appendLog(`송신 ➡️ ${cmd}`);
            else appendLog(`[스캔] 송신 ➡️ ${cmd}`, '#aaaaaa');

            try {
                const res = await fetch('/api/send', { method: 'POST', body: cmd });
                const data = await res.json();
                
                if (data.error) {
                    appendLog(`오류 ❌ ${data.error}`, '#ff4d4d');
                    return null;
                } else {
                    const responseText = data.response || '(응답 없음)';
                    const color = responseText === '(응답 없음)' ? '#ffaa00' : '#00ff00';
                    if(!isAutoScan) appendLog(`수신 ⬅️ ${responseText}`, color);
                    else appendLog(`[스캔] 수신 ⬅️ ${responseText}`, color);
                    return data.response;
                }
            } catch (err) {
                appendLog(`서버 통신 오류 ❌ ${err.message}`, '#ff4d4d');
                return null;
            }
        }

        function sendCustom() {
            const cmd = document.getElementById('customCmd').value;
            if(cmd) sendCommand(cmd);
        }

        function stopScan() {
            if(isScanning) {
                isScanning = false;
                appendLog(`[시스템] 🛑 스캔 중지됨`, '#ffaa00');
            }
        }

        async function startSingleScan() {
            if (isScanning) return alert("이미 스캔 진행 중입니다.");
            let idVal = document.getElementById('singleScanId').value;
            if (!idVal) return alert("번호를 입력하세요!");
            
            isScanning = true;
            let idStr = idVal.toString().padStart(2, '0');
            appendLog(`[시스템] 🎯 ${idStr}번 단일 스캔 시작...`, '#00bfff');

            while (isScanning) {
                await sendCommand(`$$${idStr}10;`, true);
                await sleep(500); 
            }
        }

        async function startCyclicScan() {
            if (isScanning) return alert("이미 스캔 진행 중입니다.");
            isScanning = true;
            appendLog(`[시스템] 🔄 순회 스캔 시작...`, '#00bfff');

            let currentId = 1, maxId = 99, failCount = 0;
            let isDetectingPhase = true;

            while (isScanning) {
                let idStr = currentId.toString().padStart(2, '0');
                let response = await sendCommand(`$$${idStr}10;`, true);

                if (!response || response.trim() === '') {
                    failCoSunt++;
                    if (isDetectingPhase && failCount >= 2) {
                        maxId = Math.max(1, currentId - 2); 
                        appendLog(`[시스템] 📌 범위 감지: 01 ~ ${maxId.toString().padStart(2, '0')}번 순회`, '#ff4d4d');
                        isDetectingPhase = false; currentId = 1; failCount = 0;
                        await sleep(1000); continue; 
                    }
                } else { failCount = 0; }

                currentId++; 
                if (!isDetectingPhase && currentId > maxId) {
                    currentId = 1;
                    appendLog(`[시스템] ♻️ 사이클 완료.`, '#00bfff');
                    await sleep(1000); 
                }
                await sleep(200); 
            }
        }
    </script>
</body>
</html>
)rawliteral";

extern const char index_html[] PROGMEM;

void updateOLED(String msg1, String msg2)
{
    display1.clearDisplay(); // 🌟 display1 로 통일!
    display1.setTextColor(SSD1306_WHITE);
    display1.setTextSize(1);
    display1.setCursor(0, 0);
    display1.println("Bridge Tester v1.0");
    display1.println("-----------------");
    display1.println(msg1);
    display1.println("");
    display1.println(msg2);
    display1.display(); // 🌟 display1 로 통일!
}

void updateChamberOLED(String rawData)
{
    if (rawData.startsWith("$") && rawData.length() >= 27)
    {

        // 1. 상태 데이터(16진수 4자리) 추출 및 정수 변환
        String hexStatus = rawData.substring(3, 7); // 예: "0000"
        uint16_t statusBits = strtol(hexStatus.c_str(), NULL, 16);

        // 2. 온도 데이터 추출
        String temps[4] = {
            rawData.substring(7, 12),
            rawData.substring(12, 17),
            rawData.substring(17, 22),
            rawData.substring(22, 27)};

        display2.clearDisplay();
        display2.setTextColor(SSD1306_WHITE);
        display2.setTextSize(1);
        display2.setCursor(0, 0);

        display2.println("[Chamber Status]");
        display2.println("---------------------");

        // 3. 비트 파싱 및 디스플레이 출력 (재호님의 파이썬 로직 이식!)
        for (int i = 0; i < 4; i++)
        {
            // 각 챔버당 4비트(1개의 16진수 문자)씩 차지한다고 가정 (왼쪽부터 CH1)
            int shift = (3 - i) * 4; // 12, 8, 4, 0 비트씩 밀어냄
            uint8_t chStatus = (statusBits >> shift) & 0x0F;

            // 🌟 재호님의 파이썬 비트맵핑 규칙에 맞춰 아래 마스킹(0x08, 0x04..)을 수정하세요!
            int waterLvl = (chStatus & 0x08) ? 1 : 0; // 4번째 비트 (수위)
            int pumpIn = (chStatus & 0x04) ? 1 : 0;   // 3번째 비트 (입력 펌프)
            int pumpOut = (chStatus & 0x02) ? 1 : 0;  // 2번째 비트 (출력 펌프)

            // 출력 (한 줄에 딱 20글자로 맞춰서 화면 밖으로 안 잘림)
            // 출력 예: "1: 99.00 W:1 I:1 O:0"
            display2.print(i + 1);
            display2.print(": ");
            display2.print(temps[i]);
            display2.print(" W:");
            display2.print(waterLvl);
            display2.print(" I:");
            display2.print(pumpIn);
            display2.print(" O:");
            display2.println(pumpOut);
        }

        display2.display();
    }
}

// ---------------------------------------------------------
// 3. 백엔드 로직
// ---------------------------------------------------------
void setup()
{
    Serial.begin(115200); // 디버깅용 시리얼

    // RS485용 하드웨어 시리얼2 초기화 (핀 번호 명시)
    RS485Serial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

    pinMode(DE_RE_PIN, OUTPUT);
    pinMode(TX_LED, OUTPUT);
    pinMode(RX_LED, OUTPUT);
    digitalWrite(DE_RE_PIN, LOW);
    digitalWrite(TX_LED, LOW);
    digitalWrite(RX_LED, LOW);

    // OLED 초기화
    // I2C 2개 독립 초기화
    I2C_One.begin(21, 22); // 기존 OLED 핀
    I2C_Two.begin(25, 26); // 신규 OLED 핀 (25번, 26번 빵판에 꽂으세요)

    // 디스플레이 1 초기화
    if (!display1.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println(F("OLED 1 실패"));
    }
    // 디스플레이 2 초기화
    if (!display2.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println(F("OLED 2 실패"));
    }

    display1.clearDisplay();
    display1.display();
    display2.clearDisplay();
    display2.display();

    // WiFi AP 설정
    WiFi.softAP(ssid, password);
    IPAddress IP = WiFi.softAPIP();

    // OLED에 IP 주소 표시
    updateOLED("WiFi: Bridge Tester", "IP: " + IP.toString());

    server.on("/", HTTP_GET, []()
              { server.send(200, "text/html", index_html); });

    server.on("/api/send", HTTP_POST, []()
              {
    if (!server.hasArg("plain")) {
      server.send(500, "application/json", "{\"error\":\"No Data\"}");
      return;
    }
    
    String command = server.arg("plain");

    // --- 송신 시작 (TX LED ON) ---
    digitalWrite(TX_LED, HIGH);
    
    // 🌟 여기부터 전부 RS485Serial 로 변경!
    while(RS485Serial.available()) RS485Serial.read(); // 수신 버퍼 비우기
    
    digitalWrite(DE_RE_PIN, HIGH);
    RS485Serial.print(command);
    RS485Serial.flush();
    digitalWrite(DE_RE_PIN, LOW);
    digitalWrite(TX_LED, LOW);
    // ----------------------------

    delay(500); // 응답 대기 시간

   // --- 수신 시작 (RX LED ON) ---
    String rawResponse = "";
    unsigned long startMillis = millis();
    digitalWrite(RX_LED, HIGH);
    
    while(millis() - startMillis < 2000) { // 최대 2초 대기
      if(RS485Serial.available()) {
        rawResponse += (char)RS485Serial.read();
      }
    }
    digitalWrite(RX_LED, LOW);
    // ----------------------------

    // 🌟 실전용 데이터 정제 (제어 문자 완전 삭제)
    String cleanResponse = "";
    for(int i=0; i < rawResponse.length(); i++) {
      char c = rawResponse[i];
      
      // 화면에 표시할 수 있는 일반 텍스트(ASCII 32~126)만 통과시킵니다.
      // <00>이나 <0d> 같은 찌꺼기는 여기서 전부 걸러집니다.
      if(c >= 32 && c <= 126) { 
        if(c == '"' || c == '\\') cleanResponse += '\\'; // JSON 오류 방지
        cleanResponse += c;
      }
    }
    // 🌟🌟 핵심 추가: OLED 2번 화면 업데이트 호출! 🌟🌟
    if(cleanResponse.length() > 0) {
        updateChamberOLED(cleanResponse); 
    }
    // 정제된 데이터 전송
    String jsonResponse = "{\"request\":\"" + command + "\", \"response\":\"" + cleanResponse + "\"}";
    server.send(200, "application/json", jsonResponse);
    
    // OLED에 마지막 통신 상태 표시
    updateOLED("IP: " + WiFi.softAPIP().toString(), "Last: " + command + " -> " + (cleanResponse.length() > 0 ? "OK" : "FAIL")); });

    server.begin();
}

void loop()
{
    server.handleClient();
}

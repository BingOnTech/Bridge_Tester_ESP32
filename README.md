# ESP32 RS485 Bridge Tester

ESP32 기반 RS485 브릿지 통신 테스트 및 챔버 상태(온도, 수위, 펌프) 모니터링 펌웨어.

## 1. 하드웨어 요구사항 (Hardware Requirements)

- NodeMCU ESP32-32D
- MAX485 트랜시버 모듈 (5V)
- 0.96 inch I2C SSD1306 OLED (x2)
- 저항: 1kΩ (x1), 2.2kΩ (x1), 10kΩ (x2)

## 2. 배선 및 회로도 (Pinout & Wiring)

### Block Diagram

```text
[ESP32-32D]                    [MAX485]                   [Bridge Board]
 VIN (5V)   -----------------> VCC
 GND        -----------------> GND ---------------------> GND
 GPIO 17    -----------------> DI
 GPIO 16    <--[1k]--+--<----- RO
                     |
                  [2.2k]
                     |
                    GND

 GPIO 4     -----------------> DE/RE (Short)

                               A <----[10k Pull-up]-----> A (or TX)
                               B <----[10k Pull-down]---> B (or RX)

[OLED 1 - Status]
 SDA: GPIO 21
 SCL: GPIO 22

[OLED 2 - Chamber Data]
 SDA: GPIO 25
 SCL: GPIO 26
```

### 상세 핀 맵

1. UART2 (MAX485 통신)
    - RO 핀은 5V 로직이므로 ESP32(3.3V) 보호를 위해 반드시 1kΩ / 2.2kΩ 저항 분압을 거쳐 GPIO 16에 연결.
2. RS485 Bus (A/B 선)
    - 유휴 상태(Idle) 플로팅 방지를 위해 A핀은 5V로 풀업(10kΩ), B핀은 GND로 풀다운(10kΩ) 바이어스 저항 구성 필수.
3. I2C (OLED)
    - 하드웨어 I2C 주소 충돌 방지를 위해 채널 0, 채널 1로 분리 배선.

## 3. 소프트웨어 및 라이브러리

- 개발 환경: VS Code + PlatformIO
- 프레임워크: Arduino
- 필요 라이브러리 (platformio.ini 종속성):
  - adafruit/Adafruit GFX Library
  - adafruit/Adafruit SSD1306

### 4. 가동 방법 (How to Run)

1. 소스 코드를 빌드하여 ESP32에 업로드.

2. 기기 전원 인가 후, 호스트 기기(스마트폰/PC)에서 WiFi AP Bridge_Tester 에 연결 (비밀번호 없음).

3. 웹 브라우저를 통해 <http://192.168.4.1> (혹은 OLED에 주소가 출력됨) 로 접속.
4. 웹 대시보드를 통해 송신 명령 하달 및 OLED 디스플레이 파싱 데이터 확인.

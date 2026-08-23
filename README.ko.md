[English](README.md) | 한국어

# Wireless Music Page Turner Pedal

악보 앱 페이지를 넘겨주는 BLE 풋페달입니다. 밟으면 페이지가 넘어가고, 태블릿이나 폰에 케이블을 연결할 필요가 없습니다.

Waveshare ESP32-S3-Zero 데브킷을 커스텀 캐리어 보드에 얹은 구성입니다. 표준 BLE HID 키보드로 인식되기 때문에 forScore, piaScore를 비롯해 키보드 단축키에 반응하는 앱이면 다 사용할 수 있습니다.

## 진행 상황

하드웨어 설계는 완료됐고 첫 PCB/PCBA 제작이 진행 중입니다. 펌웨어는 디바운싱과 키 매핑 프로파일을 포함한 풋스위치 입력, NimBLE 기반 BLE HID, 저전압 컷오프가 있는 배터리 모니터링, 커넥트 스위치, 상태 LED까지 구현했지만, 아직 실제 하드웨어로는 검증하지 않았습니다.

## 동작 방식

- 래칭(latching) 타입 풋스위치 두 개가 GPIO4(왼쪽, "이전")와 GPIO5(오른쪽, "다음")에 연결됩니다. 모멘터리가 아니라 래칭 스위치라서, 스텝당 한 번만 입력되도록 레벨이 아니라 GPIO 엣지를 추적합니다.
- 4비트 DIP 스위치로 부팅 시 키 매핑 프로파일을 선택합니다. 페어링된 앱에 따라 같은 페달로 화살표 키, Page Up/Down, 프레젠테이션용 Backspace/Space를 보낼 수 있습니다.
- 전용 스위치로 BLE 페어링 모드를 켭니다. 데브킷의 BOOT/RESET 버튼은 그대로 플래싱과 리셋 용도로 남아 있습니다.
- 18650 셀 하나를 TP4056 모듈로 충전해서 구동하고, ADC로 배터리 전압을 측정해 저전압 컷오프를 처리합니다.

### GPIO 맵

| GPIO | 기능 |
|------|------|
| GP4  | 왼쪽 풋스위치 ("이전") |
| GP5  | 오른쪽 풋스위치 ("다음") |
| GP6–GP9 | DIP 스위치 (4비트 키 매핑 프로파일 선택) |
| GP10 | BLE 페어링 스위치 |
| GP11 | 배터리 전압 감지 (ADC) |
| GP12 | 상태 LED |
| GP13 | 디버그 헤더 전용 |

GPIO21, GPIO33–37은 데브킷 자체가 쓰는 핀(온보드 RGB LED, PSRAM)이라 캐리어 보드에서는 사용하지 않습니다.

### 키 매핑 프로파일

| # | DIP (GP9 GP8 GP7 GP6) | 이전 | 다음 | 대상 |
|---|---|---|---|---|
| 0 | `0000` (기본값) | Left Arrow | Right Arrow | 대부분의 악보 앱 (forScore 등) |
| 1 | `0001` | Up Arrow | Down Arrow | 세로 스크롤 뷰어 |
| 2 | `0010` | Page Up | Page Down | 데스크톱 PDF 리더 |
| 3 | `0011` | Backspace | Space | 프레젠테이션 "클리커" 방식 (PowerPoint/Keynote) |
| 4 | `0100` | Backspace | Enter | Enter로 다음 페이지 넘기는 뷰어 |

정의되지 않은 조합은 프로파일 0으로 대체됩니다.

## 펌웨어

ESP-IDF로 작성했고(Arduino/PlatformIO 아님), 타겟은 `esp32s3`입니다. BLE HID는 NimBLE 백엔드 위에서 `esp_hid` 컴포넌트를 사용하며, BLE 전용입니다(Bluetooth Classic은 쓰지 않습니다).

```
idf.py set-target esp32s3   # sdkconfig.defaults에 이미 기본값으로 설정되어 있습니다
idf.py build
idf.py -p <PORT> flash monitor
```

## 하드웨어 소스

회로도, 거버 파일, 3D 모델은 [`hardware/`](hardware/)에 있습니다. BOM은 제작과 검증이 끝난 뒤 추가할 예정입니다. 이 프로젝트의 PCB 제작과 조립은 [PCBWay](https://www.pcbway.com/)의 스폰서십으로 진행됩니다.

## 라이선스

아직 정하지 않았습니다. 릴리즈 시 하드웨어 파일과 함께 추가할 예정입니다.

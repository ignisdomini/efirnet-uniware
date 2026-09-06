#pragma once
// ============================================================================
//  ЭФИРНЕТ — профили плат. Прошивка одна на все поддерживаемые модули LoRa.
//
//  Плата выбирается флагом сборки: -D BOARD_HELTEC_V3 (см. platformio.ini).
//  Каждый профиль обязан определить:
//     RADIO_*        — какой чип (SX1262/SX1268/LLCC68/SX1276/SX1278/RFM95)
//     LORA_*         — пины радио
//     HAS_DISPLAY    — есть ли OLED (+ тип и пины)
//     HAS_BATTERY    — можно ли мерить батарею (+ пин и делитель)
//     HAS_BUTTON / HAS_LED
//  Всё, чего у платы нет, просто не определяется — код обходит это #ifdef'ами.
//
//  Область применимости: ESP32 / ESP32-S3 / ESP32-C3. BLE, Wi-Fi и OTA завязаны
//  на ESP32, поэтому платы на nRF52 (RAK4631) и RP2040 сюда не входят —
//  на них поедет только LoRa-часть, но не приложение целиком.
// ============================================================================

// ---------------------------------------------------------------- Heltec V3
#if defined(BOARD_HELTEC_V3)
  #define BOARD_NAME     "Heltec V3"
  #define RADIO_SX1262
  #define LORA_SCK       9
  #define LORA_MISO      11
  #define LORA_MOSI      10
  #define LORA_NSS       8
  #define LORA_RST       12
  #define LORA_BUSY      13
  #define LORA_DIO1      14
  #define RADIO_HAS_TCXO
  #define RADIO_TCXO_V   1.8f
  #define RADIO_DIO2_RF_SWITCH
  #define HAS_DISPLAY
  #define DISPLAY_SSD1306
  #define OLED_SDA       17
  #define OLED_SCL       18
  #define OLED_RST       21
  #define PIN_VEXT       36           // питание OLED, активный уровень НИЗКИЙ
  #define VEXT_ON        LOW
  #define HAS_BATTERY
  #define PIN_VBAT_ADC   1
  #define PIN_ADC_CTRL   37
  // Проверено замером на живой плате (MAC ac:a7:04:09:ce:50):
  // при HIGH на АЦП приходит 773 мВ (3.79 В после делителя), при LOW — ноль.
  #define ADC_CTRL_ENABLE HIGH
  #define VBAT_DIVIDER   4.9f
  #define HAS_BUTTON
  #define PIN_BUTTON     0
  #define HAS_LED
  #define PIN_LED        35

// ------------------------------------------------- Heltec V4 (ревизия R8, с экраном)
// Распиновка взята из варианта heltec_v4_r8 проекта Meshtastic — это плата,
// которую там поддерживают и проверяют на живом железе, а не догадка.
//
// Радиочасть совпадает с V3 пин в пин. Отличий три, и все существенные:
//   1. Vext сидит на 40, а не на 36.
//   2. Разрешающего пина делителя батареи (ADC_CTRL) у R8 нет — делитель
//      подключён всегда. У младшей V4 он есть, на 37; это разные платы.
//   3. Появился усилитель на выходе (KCT8103L): без питания FEM сигнал
//      до антенны просто не доходит.
#elif defined(BOARD_HELTEC_V4_R8)
  #define BOARD_NAME     "Heltec V4 R8"
  #define RADIO_SX1262
  #define LORA_SCK       9
  #define LORA_MISO      11
  #define LORA_MOSI      10
  #define LORA_NSS       8
  #define LORA_RST       12
  #define LORA_BUSY      13
  #define LORA_DIO1      14
  #define RADIO_HAS_TCXO
  #define RADIO_TCXO_V   1.8f
  #define RADIO_DIO2_RF_SWITCH
  #define HAS_DISPLAY
  #define DISPLAY_SSD1306             // на плате SSD1315, драйвер SSD1306 с ним совместим
  #define OLED_SDA       17
  #define OLED_SCL       18
  #define OLED_RST       21
  #define PIN_VEXT       40           // питание экрана и тракта антенны, активный уровень НИЗКИЙ
  #define VEXT_ON        LOW
  #define HAS_BATTERY
  #define PIN_VBAT_ADC   1
  #define VBAT_DIVIDER   5.07f        // 4.9 по номиналу делителя и поправка 1.035 на его же утечку
  #define HAS_BUTTON
  #define PIN_BUTTON     0
  #define HAS_LED
  #define PIN_LED        46

  // ---- выходной усилитель ----
  // KCT8103L между SX1262 и антенной, ставится только он — определять нечего.
  #define HAS_LORA_FEM
  #define FEM_FIXED_KCT8103L
  #define PIN_FEM_POWER  7            // питание LDO усилителя, включаем HIGH
  #define PIN_FEM_CSD    2            // разрешение микросхемы, HIGH = включена
  #define PIN_FEM_CTX    5            // выбор тракта, см. lora_fem.cpp

  // Мощность на выводе чипа занижена НАМЕРЕННО, см. femLegalInputDbm().
  #define RF_POWER_DBM   0

// ---------------------------------------------- Heltec WiFi LoRa 32 V4
//
// Младшая V4: тот же ESP32-S3, но без PSRAM. От R8 отличается почти всей
// обвязкой, поэтому это отдельный профиль, а не флаг внутри общего:
//   Vext 36 (у R8 — 40), делитель батареи через ADC_CTRL 37 (у R8 его нет),
//   светодиод 35 (у R8 — 46, и там этот же номер занят усилителем).
//
// Главное: усилитель бывает ДВУХ разных микросхем — GC1109 на ревизии 4.2
// и KCT8103L на 4.3. Управляются они по-разному и разными выводами, а внешне
// платы неотличимы, поэтому чип определяется при включении (см. lora_fem.cpp).
#elif defined(BOARD_HELTEC_V4)
  #define BOARD_NAME     "Heltec V4"
  #define RADIO_SX1262
  #define LORA_SCK       9
  #define LORA_MISO      11
  #define LORA_MOSI      10
  #define LORA_NSS       8
  #define LORA_RST       12
  #define LORA_BUSY      13
  #define LORA_DIO1      14
  #define RADIO_HAS_TCXO
  #define RADIO_TCXO_V   1.8f
  #define RADIO_DIO2_RF_SWITCH
  #define HAS_DISPLAY
  #define DISPLAY_SSD1306             // SSD1315, драйвер SSD1306 с ним совместим
  #define OLED_SDA       17
  #define OLED_SCL       18
  #define OLED_RST       21
  #define PIN_VEXT       36
  #define VEXT_ON        LOW
  #define HAS_BATTERY
  #define PIN_VBAT_ADC   1
  #define PIN_ADC_CTRL   37
  #define ADC_CTRL_ENABLE HIGH
  #define VBAT_DIVIDER   5.12f        // 4.9 по номиналу делителя и поправка 1.045
  #define HAS_BUTTON
  #define PIN_BUTTON     0
  #define HAS_LED
  #define PIN_LED        35

  // ---- выходной усилитель ----
  #define HAS_LORA_FEM
  #define FEM_AUTODETECT              // GC1109 или KCT8103L, решается при включении
  #define PIN_FEM_POWER  7            // общий LDO обеих микросхем
  #define PIN_FEM_CSD    2            // разрешение: у GC1109 это CSD, у KCT8103L тоже
  #define PIN_FEM_CTX    5            // только KCT8103L
  #define PIN_FEM_CPS    46           // только GC1109
  #define RF_POWER_DBM   0            // поднимается при старте, см. femLegalInputDbm()

// ------------------------------------------- Heltec Wireless Stick Lite V3
#elif defined(BOARD_HELTEC_STICK_LITE_V3)
  #define BOARD_NAME     "Stick Lite V3"
  #define RADIO_SX1262
  #define LORA_SCK       9
  #define LORA_MISO      11
  #define LORA_MOSI      10
  #define LORA_NSS       8
  #define LORA_RST       12
  #define LORA_BUSY      13
  #define LORA_DIO1      14
  #define RADIO_HAS_TCXO
  #define RADIO_TCXO_V   1.8f
  #define RADIO_DIO2_RF_SWITCH
  // Экрана нет — весь UI уходит в приложение по BLE.
  #define HAS_BATTERY
  #define PIN_VBAT_ADC   1
  #define PIN_ADC_CTRL   37
  // Та же схема питания, что у V3, где HIGH проверен замером. На живом
  // Stick Lite не проверялось — если заряд читается как 0%, смотрите строку
  // «[BATT] замер» в мониторе при старте: там видно оба варианта сразу.
  #define ADC_CTRL_ENABLE HIGH
  #define VBAT_DIVIDER   4.9f
  #define HAS_BUTTON
  #define PIN_BUTTON     0
  #define HAS_LED
  #define PIN_LED        35

// ---------------------------------------------------------------- Heltec V2
#elif defined(BOARD_HELTEC_V2)
  #define BOARD_NAME     "Heltec V2"
  #define RADIO_SX1276
  #define LORA_SCK       5
  #define LORA_MISO      19
  #define LORA_MOSI      27
  #define LORA_NSS       18
  #define LORA_RST       14
  #define LORA_DIO0      26
  #define LORA_DIO1      35
  #define HAS_DISPLAY
  #define DISPLAY_SSD1306
  #define OLED_SDA       4
  #define OLED_SCL       15
  #define OLED_RST       16
  #define PIN_VEXT       21
  #define VEXT_ON        LOW
  #define HAS_BATTERY
  #define PIN_VBAT_ADC   37
  #define VBAT_DIVIDER   4.9f
  #define HAS_BUTTON
  #define PIN_BUTTON     0
  #define HAS_LED
  #define PIN_LED        25

// -------------------------------------------------- LilyGO TTGO LoRa32 V1
#elif defined(BOARD_TTGO_LORA32_V1)
  #define BOARD_NAME     "TTGO LoRa32 V1"
  #define RADIO_SX1276
  #define LORA_SCK       5
  #define LORA_MISO      19
  #define LORA_MOSI      27
  #define LORA_NSS       18
  #define LORA_RST       14
  #define LORA_DIO0      26
  #define LORA_DIO1      33
  #define HAS_DISPLAY
  #define DISPLAY_SSD1306
  #define OLED_SDA       4
  #define OLED_SCL       15
  #define OLED_RST       16
  #define HAS_BUTTON
  #define PIN_BUTTON     0
  #define HAS_LED
  #define PIN_LED        2

// ------------------------------------------------ LilyGO TTGO LoRa32 V2.1
#elif defined(BOARD_TTGO_LORA32_V21)
  #define BOARD_NAME     "TTGO LoRa32 V2.1"
  #define RADIO_SX1276
  #define LORA_SCK       5
  #define LORA_MISO      19
  #define LORA_MOSI      27
  #define LORA_NSS       18
  #define LORA_RST       23
  #define LORA_DIO0      26
  #define LORA_DIO1      33
  #define HAS_DISPLAY
  #define DISPLAY_SSD1306
  #define OLED_SDA       21
  #define OLED_SCL       22
  #define OLED_RST       -1            // сброса нет, экран стартует сам
  #define HAS_BATTERY
  #define PIN_VBAT_ADC   35
  #define VBAT_DIVIDER   2.0f
  #define HAS_BUTTON
  #define PIN_BUTTON     0
  #define HAS_LED
  #define PIN_LED        25

// -------------------------------------------------------- LilyGO T3-S3 (S3)
#elif defined(BOARD_LILYGO_T3S3)
  #define BOARD_NAME     "LilyGO T3-S3"
  #define RADIO_SX1262
  #define LORA_SCK       5
  #define LORA_MISO      3
  #define LORA_MOSI      6
  #define LORA_NSS       7
  #define LORA_RST       8
  #define LORA_BUSY      34
  #define LORA_DIO1      33
  #define RADIO_HAS_TCXO
  #define RADIO_TCXO_V   1.8f
  #define RADIO_DIO2_RF_SWITCH
  #define HAS_DISPLAY
  #define DISPLAY_SSD1306
  #define OLED_SDA       18
  #define OLED_SCL       17
  #define OLED_RST       -1
  #define HAS_BATTERY
  #define PIN_VBAT_ADC   1
  #define VBAT_DIVIDER   2.0f
  #define HAS_BUTTON
  #define PIN_BUTTON     0
  #define HAS_LED
  #define PIN_LED        37

// ------------------------------------------------------ LilyGO T-Beam v1.1
#elif defined(BOARD_TBEAM_V11)
  #define BOARD_NAME     "T-Beam v1.1"
  #define RADIO_SX1276
  #define LORA_SCK       5
  #define LORA_MISO      19
  #define LORA_MOSI      27
  #define LORA_NSS       18
  #define LORA_RST       23
  #define LORA_DIO0      26
  #define LORA_DIO1      33
  #define HAS_DISPLAY
  #define DISPLAY_SSD1306
  #define OLED_SDA       21
  #define OLED_SCL       22
  #define OLED_RST       -1
  // Питанием экрана и радио заведует контроллер AXP192 по той же шине I2C.
  // Без его инициализации плата выглядит мёртвой — см. boardPowerOn() в main.cpp.
  #define HAS_AXP192
  #define AXP192_ADDR    0x34
  #define HAS_BUTTON
  #define PIN_BUTTON     38
  #define HAS_LED
  #define PIN_LED        4

// ------------------------- Обобщённая ESP32 + RFM95/SX1276 (самосбор, шилды)
#elif defined(BOARD_GENERIC_SX1276)
  #define BOARD_NAME     "ESP32 + SX1276"
  #define RADIO_SX1276
  #ifndef LORA_SCK
    #define LORA_SCK     18
    #define LORA_MISO    19
    #define LORA_MOSI    23
    #define LORA_NSS     5
    #define LORA_RST     14
    #define LORA_DIO0    26
    #define LORA_DIO1    33
  #endif
  #define HAS_BUTTON
  #define PIN_BUTTON     0

// ------------------- Обобщённая ESP32-S3 + SX1262/SX1268/LLCC68 (E22, Ra-01)
#elif defined(BOARD_GENERIC_SX126X)
  #ifndef BOARD_NAME
    #define BOARD_NAME   "ESP32 + SX126x"
  #endif
  #if !defined(RADIO_SX1262) && !defined(RADIO_SX1268) && !defined(RADIO_LLCC68)
    #define RADIO_SX1262
  #endif
  #ifndef LORA_SCK
    #define LORA_SCK     12
    #define LORA_MISO    13
    #define LORA_MOSI    11
    #define LORA_NSS     10
    #define LORA_RST     5
    #define LORA_BUSY    4
    #define LORA_DIO1    6
  #endif
  #define HAS_BUTTON
  #define PIN_BUTTON     0
  // У модулей E22 антенный тракт переключается внешними ножками RXEN/TXEN.
  // Если они есть — задайте -D PIN_RF_RXEN=.. -D PIN_RF_TXEN=.., HAL их подхватит.

#else
  #error "ЭФИРНЕТ: не выбрана плата. Укажите -D BOARD_HELTEC_V3 (или другой профиль из board_pins.h)"
#endif

// ---- значения по умолчанию для необязательных вещей ----
#ifndef VEXT_ON
  #define VEXT_ON LOW
#endif
#ifndef OLED_RST
  #define OLED_RST -1
#endif
#ifndef RADIO_TCXO_V
  #define RADIO_TCXO_V 0.0f     // 0 = кварц, а не TCXO
#endif

// Семейство чипа — чтобы дальше по коду не перечислять все модели.
#if defined(RADIO_SX1262) || defined(RADIO_SX1268) || defined(RADIO_LLCC68)
  #define RADIO_IS_SX126X
#elif defined(RADIO_SX1276) || defined(RADIO_SX1278) || defined(RADIO_RFM95)
  #define RADIO_IS_SX127X
#else
  #error "ЭФИРНЕТ: не задан тип радиочипа"
#endif

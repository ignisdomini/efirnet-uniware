// ============================================================================
//  EFIRNET UNIWARE V.1 — менеджер загрузки  ·  EFIRNET.RU
//
//  Живёт в разделе factory. Показывает бренд-заставку, затем список прошивок,
//  найденных в разделах app_*, и переводит плату в выбранную через otadata.
//
//  Пины НЕ прописаны здесь: берутся из board_pins.h проекта ЭФИРНЕТ, ровно те
//  же, что у самой прошивки. Плата задаётся флагом -D BOARD_* в platformio.ini,
//  так что новая плата добавляется одним окружением, без правок этого файла.
//
//  Управление кнопкой PRG:
//      короткое нажатие   — следующий пункт меню (автостарт отменяется);
//      удержание > 700 мс — загрузить выбранную прошивку;
//      бездействие 6 с    — грузится прошивка, выбранная в прошлый раз.
//
//  На платах без экрана (Stick Lite V3) меню озвучивается светодиодом: номер
//  выбранной прошивки = столько же вспышек. Полный список идёт в UART 115200.
//
//  ВНИМАНИЕ: кнопка сидит на strapping-пине GPIO 0. Держать её в момент сброса
//  нельзя — чип уйдёт в UART-загрузчик, а не в меню. Кнопку нажимают ПОСЛЕ
//  того, как появилась заставка.
// ============================================================================
#include <Arduino.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_app_format.h>

#include "board_pins.h"          // из firmware/include проекта ЭФИРНЕТ

#if defined(HAS_DISPLAY)
  #include <U8g2lib.h>
  #include <Wire.h>
#endif

#ifndef PIN_BUTTON
  #define PIN_BUTTON 0
#endif

#define AUTOBOOT_MS   6000       // таймаут автостарта
#define LONGPRESS_MS  700        // порог «длинного» нажатия
#define DEBOUNCE_MS   40
#define SPLASH_MS     1600       // сколько держим заставку

// ------------------------------------------------------------------- бренд
#define BRAND_NAME    "EFIRNET"
#define BRAND_PRODUCT "UNIWARE  V.1"
#define BRAND_SITE    "EFIRNET.RU"

// -------------------------------------------------------------------- экран
#if defined(HAS_DISPLAY)
  // У части плат линии RST у экрана нет вовсе, и в board_pins.h она помечена
  // как -1. U8g2 для этого случая ждёт U8X8_PIN_NONE, а не отрицательное число.
  #if OLED_RST < 0
    #define OLED_RST_ARG U8X8_PIN_NONE
  #else
    #define OLED_RST_ARG OLED_RST
  #endif

  #if defined(DISPLAY_SH1106)
    static U8G2_SH1106_128X64_NONAME_F_HW_I2C  u8g2(U8G2_R0, OLED_RST_ARG, OLED_SCL, OLED_SDA);
  #else
    static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_RST_ARG, OLED_SCL, OLED_SDA);
  #endif

  #define F_SMALL u8g2_font_5x7_t_cyrillic
  #define F_MAIN  u8g2_font_6x13_t_cyrillic
  #define F_BIG   u8g2_font_10x20_t_cyrillic
  #define F_NAME  u8g2_font_8x13_t_cyrillic
#endif

// -------------------------------------------------------------- список слотов
// Порядок здесь = порядок в меню. Метки обязаны совпадать с partitions.csv.
struct Slot {
    const char *label;           // метка раздела
    const char *title;           // как показать на экране
};

static const Slot kSlots[] = {
    { "app_efr", "ЭФИРНЕТ"    },
    { "app_mt",  "MESHTASTIC" },
    { "app_mc",  "MESHCORE"   },
};
static const int kSlotCount = sizeof(kSlots) / sizeof(kSlots[0]);

// Что реально нашлось во флеше.
struct Found {
    const esp_partition_t *part;
    bool                   valid;
};
static Found found[kSlotCount];
static int   present[kSlotCount];         // индексы непустых слотов
static int   presentCount = 0;
static int   defaultIdx   = -1;           // что выбирали в прошлый раз

static Preferences prefs;

// ---------------------------------------------------------------------------
// Слот считаем живым, если в нём лежит образ с корректным заголовком. У
// esp_ota_get_partition_description() ровно эта семантика: она разбирает
// esp_app_desc_t сразу за заголовком образа и возвращает ошибку на мусоре.
// Проверка нужна не для красоты: esp_ota_set_boot_partition() на пустом
// разделе вернёт ошибку, и без неё меню молча «не сработало бы».
// ---------------------------------------------------------------------------
static void scanSlots()
{
    for (int i = 0; i < kSlotCount; ++i) {
        found[i].valid = false;
        found[i].part = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                                 ESP_PARTITION_SUBTYPE_ANY,
                                                 kSlots[i].label);
        if (!found[i].part) continue;

        esp_app_desc_t desc;
        if (esp_ota_get_partition_description(found[i].part, &desc) == ESP_OK) {
            found[i].valid = true;
        } else {
            // Дескриптора может не быть, а образ быть. Смотрим магию заголовка.
            uint8_t magic = 0;
            if (esp_partition_read(found[i].part, 0, &magic, 1) == ESP_OK &&
                magic == ESP_IMAGE_HEADER_MAGIC)
                found[i].valid = true;
        }
        if (found[i].valid) present[presentCount++] = i;
    }
}

// ------------------------------------------------------------------- рисование
// Все функции ниже на плате без экрана вырождаются в пустышки, чтобы основная
// логика не обрастала #if на каждом шагу.

static void drawSplash(uint32_t elapsed)
{
#if defined(HAS_DISPLAY)
    u8g2.clearBuffer();
    u8g2.drawFrame(0, 0, 128, 64);
    u8g2.drawHLine(0, 2, 128);

    u8g2.setFont(F_BIG);
    u8g2.drawUTF8((128 - u8g2.getUTF8Width(BRAND_NAME)) / 2, 26, BRAND_NAME);

    u8g2.setFont(F_MAIN);
    u8g2.drawUTF8((128 - u8g2.getUTF8Width(BRAND_PRODUCT)) / 2, 41, BRAND_PRODUCT);

    u8g2.drawHLine(14, 46, 100);

    u8g2.setFont(F_SMALL);
    u8g2.drawUTF8((128 - u8g2.getUTF8Width(BRAND_SITE)) / 2, 56, BRAND_SITE);

    const int w = (int)(126L * (elapsed > SPLASH_MS ? SPLASH_MS : elapsed) / SPLASH_MS);
    u8g2.drawHLine(1, 61, w);
    u8g2.sendBuffer();
#else
    (void)elapsed;
#endif
}

static void drawMenu(int cursor, uint32_t msLeft, bool autoboot)
{
#if defined(HAS_DISPLAY)
    u8g2.clearBuffer();

    u8g2.setFont(F_SMALL);
    u8g2.drawUTF8(0, 7, BRAND_NAME " " BRAND_PRODUCT);
    u8g2.drawHLine(0, 10, 128);

    // В строке только название сети. Версию из esp_app_desc_t отсюда убрали:
    // у Meshtastic она вида «2.7.26.54e0d8d» и своим правым краем наезжала на
    // название — на экране это выглядело как обрубленные буквы.
    int row = 0;
    for (int i = 0; i < kSlotCount; ++i) {
        if (!found[i].valid) continue;          // пустые слоты не показываем вовсе

        const int  y   = 24 + row * 14;
        const bool sel = (i == cursor);
        if (sel) {
            u8g2.drawBox(0, y - 11, 128, 14);
            u8g2.setDrawColor(0);
        }
        // Точка у левого края — прошивка, выбранная в прошлый раз. Курсор
        // стартует на ней, но человек может его увести, и тогда без метки
        // непонятно, что именно уйдёт в автостарт.
        if (i == defaultIdx) u8g2.drawBox(1, y - 7, 3, 3);

        u8g2.setFont(F_NAME);
        u8g2.drawUTF8(7, y, kSlots[i].title);

        if (sel) u8g2.setDrawColor(1);
        ++row;
    }

    // Обратный отсчёт — полосой внизу, без текста.
    if (autoboot) u8g2.drawBox(0, 61, (int)(128L * msLeft / AUTOBOOT_MS), 3);
    u8g2.sendBuffer();
#else
    (void)cursor; (void)msLeft; (void)autoboot;
#endif
}

static void drawBooting(const char *title)
{
#if defined(HAS_DISPLAY)
    u8g2.clearBuffer();
    u8g2.setFont(F_SMALL);
    u8g2.drawUTF8(0, 7, BRAND_NAME " " BRAND_PRODUCT);
    u8g2.drawHLine(0, 10, 128);
    u8g2.drawUTF8(0, 26, "ЗАГРУЗКА");
    u8g2.setFont(F_BIG);
    u8g2.drawUTF8((128 - u8g2.getUTF8Width(title)) / 2, 48, title);
    u8g2.sendBuffer();
#else
    (void)title;
#endif
}

static void drawFatal(const char *msg)
{
#if defined(HAS_DISPLAY)
    u8g2.clearBuffer();
    u8g2.setFont(F_SMALL);
    u8g2.drawUTF8(0, 7, BRAND_NAME " " BRAND_PRODUCT);
    u8g2.drawHLine(0, 10, 128);
    u8g2.setFont(F_MAIN);
    u8g2.drawUTF8(0, 28, "ОШИБКА");
    u8g2.setFont(F_SMALL);
    u8g2.drawUTF8(0, 43, msg);
    u8g2.drawUTF8(0, 55, "залейте образ в слот");
    u8g2.sendBuffer();
#endif
    Serial.printf("[UNIWARE] ошибка: %s\n", msg);
}

// ------------------------------------------------------------- светодиод
// Единственный способ что-то сообщить на плате без экрана: номер выбранной
// прошивки = столько же вспышек. Вызывается только там, где экрана нет, —
// иначе мигание отвлекало бы от меню.
static void blinkSlot(int idx)
{
#if defined(HAS_LED) && !defined(HAS_DISPLAY)
    const int n = idx + 1;
    for (int i = 0; i < n; ++i) {
        digitalWrite(PIN_LED, HIGH); delay(120);
        digitalWrite(PIN_LED, LOW);  delay(180);
    }
#else
    (void)idx;
#endif
}

static void printMenu(int cursor)
{
    Serial.printf("\n=== %s %s · %s ===\n", BRAND_NAME, BRAND_PRODUCT, BRAND_SITE);
    for (int i = 0; i < kSlotCount; ++i) {
        if (!found[i].valid) continue;
        Serial.printf(" %s %d) %-12s%s\n",
                      i == cursor ? ">" : " ", i + 1, kSlots[i].title,
                      i == defaultIdx ? "  (по умолчанию)" : "");
    }
    Serial.println(" PRG: коротко — следующая, удержать — запустить");
}

// ---------------------------------------------------------------------------
static void bootSlot(int idx)
{
    if (idx < 0 || idx >= kSlotCount || !found[idx].valid) return;

    // Запоминаем выбор ДО перезагрузки: следующий автостарт пойдёт сюда же.
    prefs.begin("bootmgr", false);
    prefs.putString("slot", kSlots[idx].label);
    prefs.end();

    Serial.printf("[UNIWARE] запускаю %s\n", kSlots[idx].title);
    drawBooting(kSlots[idx].title);

    const esp_err_t err = esp_ota_set_boot_partition(found[idx].part);
    if (err != ESP_OK) {
        drawFatal(esp_err_to_name(err));
        delay(4000);
        return;
    }
    delay(400);
#if defined(HAS_DISPLAY)
    u8g2.setPowerSave(1);        // гасим экран, чтобы не моргал на рестарте
#endif
    esp_restart();
}

// ---------------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);

#if defined(HAS_LED)
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);
#endif

#if defined(PIN_VEXT)
    // Vext кормит OLED. Без него экран не отзовётся по I2C, и меню окажется
    // невидимым — плата ушла бы в автостарт вслепую.
    pinMode(PIN_VEXT, OUTPUT);
    digitalWrite(PIN_VEXT, VEXT_ON);
    delay(60);
#endif

    pinMode(PIN_BUTTON, INPUT_PULLUP);

#if defined(HAS_DISPLAY)
    u8g2.begin();
    u8g2.setBusClock(400000);
    u8g2.enableUTF8Print();
#endif

    // Заставка. Нажатие PRG во время неё пропускает её и сразу отменяет
    // автостарт: человек тянулся к кнопке — значит, хочет выбрать сам.
    bool skipAuto = false;
    for (uint32_t s0 = millis();;) {
        const uint32_t el = millis() - s0;
        if (digitalRead(PIN_BUTTON) == LOW) { skipAuto = true; break; }
        if (el >= SPLASH_MS) break;
        drawSplash(el);
        delay(25);
    }
    // Ждём отпускания, иначе меню примет это же нажатие за «длинное».
    for (uint32_t r0 = millis();
         digitalRead(PIN_BUTTON) == LOW && millis() - r0 < 3000; )
        delay(20);

    scanSlots();

    if (presentCount == 0) {
        drawFatal("нет ни одного образа");
        while (true) {
#if defined(HAS_LED)
            digitalWrite(PIN_LED, !digitalRead(PIN_LED));
#endif
            delay(300);
        }
    }

    // Курсор ставим на прошивку из прошлого сеанса.
    int cursor = present[0];
    prefs.begin("bootmgr", true);
    const String last = prefs.getString("slot", "");
    prefs.end();
    if (last.length()) {
        for (int i = 0; i < kSlotCount; ++i)
            if (found[i].valid && last == kSlots[i].label) {
                cursor = defaultIdx = i;
                break;
            }
    }

    // Единственный живой слот — меню показывать не за чем.
    if (presentCount == 1) { bootSlot(present[0]); return; }

    printMenu(cursor);
    blinkSlot(cursor);

    bool           autoboot  = !skipAuto;
    const uint32_t t0        = millis();
    uint32_t       pressedAt = 0;
    bool           wasDown   = false;
    bool           fired     = false;   // длинное нажатие уже отработано

    while (true) {
        const uint32_t now  = millis();
        const bool     down = (digitalRead(PIN_BUTTON) == LOW);

        if (down && !wasDown) {                       // фронт нажатия
            pressedAt = now;
            wasDown   = true;
            fired     = false;
            autoboot  = false;                        // человек за пультом
        } else if (down && wasDown && !fired && (now - pressedAt) >= LONGPRESS_MS) {
            fired = true;                             // длинное — пуск
#if defined(HAS_LED)
            digitalWrite(PIN_LED, HIGH);
#endif
            bootSlot(cursor);
            return;
        } else if (!down && wasDown) {                // отпустили
            wasDown = false;
            if (!fired && (now - pressedAt) >= DEBOUNCE_MS) {
                // короткое — следующий ЖИВОЙ слот, пустые пропускаем
                for (int step = 1; step <= kSlotCount; ++step) {
                    const int cand = (cursor + step) % kSlotCount;
                    if (found[cand].valid) { cursor = cand; break; }
                }
                printMenu(cursor);
                blinkSlot(cursor);
            }
        }

        uint32_t left = 0;
        if (autoboot) {
            const uint32_t el = now - t0;
            if (el >= AUTOBOOT_MS) { bootSlot(cursor); return; }
            left = AUTOBOOT_MS - el;
        }
        drawMenu(cursor, left, autoboot);
        delay(20);
    }
}

void loop() {}

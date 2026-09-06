// ============================================================================
//  ВОЗВРАТ В МЕНЮ EFIRNET UNIWARE — вставка для целевых прошивок
//
//  КУДА КЛАСТЬ. В корень проекта Meshtastic / MeshCore / ЭФИРНЕТ, в папку
//  `dualboot/`, и добавить её в сборку:
//
//      build_src_filter = ${env:<исходный_env>.build_src_filter} +<../dualboot/>
//
//  Класть в `lib/` НЕЛЬЗЯ. Проверено на сборке: PlatformIO кладёт библиотеку
//  в архив .a, а на этот файл никто не ссылается — компоновщик выбрасывает
//  объектный файл целиком вместе с конструктором, и возврат молча не работает.
//  Из `src/` (или из папки, добавленной через build_src_filter) объектный файл
//  попадает в компоновку напрямую и конструктор остаётся.
//
//  ЗАЧЕМ. После esp_ota_set_boot_partition() запись в otadata постоянная:
//  плата будет грузить выбранную прошивку и никогда сама не вернётся в
//  factory. Единственный способ попасть обратно в меню без компьютера —
//  чтобы сама прошивка переписала otadata на factory. Этим и занят код ниже.
//
//  КАК ПОЛЬЗОВАТЬСЯ. В первые 12 секунд после включения зажать PRG и держать
//  2 секунды — плата уйдёт в меню. Держать PRG в момент сброса НЕЛЬЗЯ: GPIO 0
//  это strapping-пин, чип уйдёт в UART-загрузчик.
//
//  Сторожевой таймер живёт в отдельной задаче и загрузку не задерживает —
//  прошивка стартует ровно так же быстро, как без этой вставки.
// ============================================================================
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <esp_log.h>

// Режим возврата в меню:
//   0 (по умолчанию) — ПРЯМАЯ ЗАГРУЗКА. Выбранная в меню прошивка стартует при
//     каждом включении мгновенно, меню больше не появляется. В меню возвращает
//     только удержание PRG (см. ниже).
//   1 — МЕНЮ ВСЕГДА. Прошивка сразу на старте возвращает otadata на factory,
//     поэтому каждое следующее включение открывает меню EFIRNET UNIWARE, в нём
//     подсвечена последняя выбранная прошивка, и она же уходит в автостарт по
//     таймауту. Цена — одна перезапись сектора otadata за загрузку (это порядка
//     десятилетий при десятке включений в день) и лишние 6 секунд до старта.
#ifndef DUALBOOT_ALWAYS_MENU
#define DUALBOOT_ALWAYS_MENU 0
#endif

#ifndef DUALBOOT_BUTTON_GPIO
#define DUALBOOT_BUTTON_GPIO 0        // PRG на Heltec V3
#endif
#ifndef DUALBOOT_WINDOW_MS
#define DUALBOOT_WINDOW_MS   12000    // сколько секунд после старта слушаем кнопку
#endif
#ifndef DUALBOOT_HOLD_MS
#define DUALBOOT_HOLD_MS     2000     // сколько держать, чтобы сработало
#endif

static const char *TAG = "dualboot";

// ---------------------------------------------------------------------------
// Возврат управления загрузчику = стирание otadata. Пустой otadata загрузчик
// ESP32 трактует однозначно: грузить factory.
//
// Через esp_ota_set_boot_partition(factory) идти не стали намеренно. В IDF 4.4
// поведение этой функции на разделе типа factory неочевидно и от версии к версии
// менялось, а стирание сектора делает ровно то же, что проверенная команда
// `esptool erase_region 0xe000 0x2000`, и не зависит ни от чего.
// ---------------------------------------------------------------------------
static bool dualboot_release_to_menu(void)
{
    const esp_partition_t *otadata = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
    if (!otadata) {
        ESP_LOGE(TAG, "раздел otadata не найден, возврат в меню невозможен");
        return false;
    }
    const esp_err_t err = esp_partition_erase_range(otadata, 0, otadata->size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "не удалось стереть otadata: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

// Уже ли otadata пуст? Если да, второй раз стирать сектор незачем — это лишний
// цикл записи флеша на каждой загрузке.
static bool dualboot_otadata_is_blank(void)
{
    const esp_partition_t *otadata = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
    if (!otadata) return true;

    uint32_t seq[2] = {0, 0};
    if (esp_partition_read(otadata, 0,             &seq[0], 4) != ESP_OK) return false;
    if (esp_partition_read(otadata, otadata->size / 2, &seq[1], 4) != ESP_OK) return false;
    return seq[0] == 0xFFFFFFFFu && seq[1] == 0xFFFFFFFFu;
}

static void dualboot_task(void *)
{
    const gpio_num_t pin = (gpio_num_t)DUALBOOT_BUTTON_GPIO;

    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << pin;
    cfg.mode         = GPIO_MODE_INPUT;
    cfg.pull_up_en   = GPIO_PULLUP_ENABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type    = GPIO_INTR_DISABLE;
    gpio_config(&cfg);

    const TickType_t step = pdMS_TO_TICKS(50);
    uint32_t elapsed = 0, held = 0;

    while (elapsed < DUALBOOT_WINDOW_MS) {
        vTaskDelay(step);
        elapsed += 50;

        // Кнопка замыкает GPIO 0 на землю, поэтому нажатие — это ноль.
        held = (gpio_get_level(pin) == 0) ? held + 50 : 0;

        if (held >= DUALBOOT_HOLD_MS) {
            ESP_LOGW(TAG, "возврат в меню EFIRNET UNIWARE");
            if (dualboot_release_to_menu()) {
                vTaskDelay(pdMS_TO_TICKS(100));
                esp_restart();
            }
            break;
        }
    }
    vTaskDelete(NULL);
}

// Конструктор отрабатывает до setup() и до app_main(), но уже под управлением
// планировщика: xTaskCreate здесь легален, а сама задача не трогает ни NVS,
// ни Arduino-обвязку — только GPIO и OTA API, которые доступны сразу.
__attribute__((constructor)) static void dualboot_return_install()
{
#if DUALBOOT_ALWAYS_MENU
    // Отдаём управление загрузчику прямо сейчас, не дожидаясь кнопки.
    // Перезагрузки здесь нет: текущая прошивка спокойно доработает до конца,
    // а в меню плата попадёт при следующем включении.
    if (!dualboot_otadata_is_blank() && dualboot_release_to_menu())
        ESP_LOGI(TAG, "otadata очищен, следующий старт - меню EFIRNET UNIWARE");
#endif
    xTaskCreate(dualboot_task, "dualboot", 3072, NULL, 1, NULL);
}

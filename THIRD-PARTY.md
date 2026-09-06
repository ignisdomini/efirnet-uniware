# Чужой код в составе EFIRNET UNIWARE

Комплект UNIWARE — это не одна программа, а четыре независимых образа,
записанных в разные разделы флеша одной платы. Лицензия у каждого своя и
не меняется от того, что они лежат рядом.

| Образ | Раздел | Лицензия | Откуда |
|---|---|---|---|
| EFIRNET UNIWARE — меню загрузки | `factory` | Apache 2.0 | этот репозиторий |
| ЭФИРНЕТ | `app_efr` | Apache 2.0 | https://github.com/ignisdomini/Efirnet |
| Meshtastic | `app_mt` | **GPL-3.0** | https://github.com/meshtastic/firmware |
| MeshCore | `app_mc` | MIT | https://github.com/meshcore-dev/MeshCore |

Точные версии записаны в `manifest.json` рядом с образами в релизе.

---

## Обе чужие прошивки — изменённые

Изменения минимальны и одинаковы по смыслу. Без них мультизагрузчик
работать не может: прошивка, однажды получившая управление, никогда не
вернула бы его меню.

**Что добавлено в обе:**

1. Файл `patch_dualboot_return/dualboot_return.cpp` из этого репозитория.
   Он поднимает сторожевую задачу, которая при старте очищает `otadata`,
   и тем самым возвращает управление меню при следующем включении.
2. Заменена таблица разделов: во флеше теперь три прошивки вместо одной.
3. Образ собирается по другому смещению — в свой слот, а не в `app0`.

**Дополнительно в MeshCore** — одна строка монтирования файловой системы
в `examples/companion_radio/main.cpp`:

```cpp
SPIFFS.begin(true, "/spiffs", 10, "mc_fs");   // было: SPIFFS.begin(true);
```

Без неё MeshCore заняла бы раздел соседа. Логика самой прошивки не тронута.

**Meshtastic не правится вовсе**: в раскладке UNIWARE канонические имена
`nvs` и `spiffs` отданы именно ей, поэтому её код монтирования подходит
как есть. Всё изменение сводится к добавленному файлу и конфигурации сборки.

---

## Исходный код Meshtastic (требование GPL-3.0)

Мы распространяем изменённый двоичный образ Meshtastic. GPL-3.0 требует
предоставить соответствующий исходный код — то есть апстрим ровно той
версии плюс наши изменения. Всё это доступно и воспроизводится:

**Апстрим.** Тег указан в `manifest.json` каждого комплекта. Для выпуска
1.0 это `v2.7.26.54e0d8d` в https://github.com/meshtastic/firmware

**Наши изменения.** Лежат в этом репозитории:

* `patch_dualboot_return/dualboot_return.cpp` — добавляемый файл
* `integration/meshtastic/uniware.ini` — окружения сборки
* `integration/meshtastic/partition-uniware-8mb.csv` и `-16mb.csv` — таблицы разделов

**Как собрать тот же образ:**

```bash
git clone --depth 1 --branch v2.7.26.54e0d8d https://github.com/meshtastic/firmware
cd firmware
cp <uniware>/patch_dualboot_return/dualboot_return.cpp src/
cp <uniware>/integration/meshtastic/partition-uniware-*.csv .
cp <uniware>/integration/meshtastic/uniware.ini variants/esp32s3/
pio run -e heltec-v3-uniware
```

Если этого недостаточно, автор обязуется в течение трёх лет с момента
получения вами копии выдать полный исходный код соответствующей версии
на носителе по себестоимости пересылки. Запрос: apostolgroup@gmail.com

## Исходный код MeshCore

MIT не требует письменного предложения, но условия те же и всё
воспроизводится так же:

```bash
git clone --depth 1 --branch companion-v1.17.0 https://github.com/meshcore-dev/MeshCore
cd MeshCore
cp <uniware>/patch_dualboot_return/dualboot_return.cpp dualboot/
cp <uniware>/integration/meshcore/platformio.local.ini .
cp <uniware>/integration/meshcore/partition-uniware-*.csv .
# правка одной строки в examples/companion_radio/main.cpp — см. выше
pio run -e Heltec_v3_uniware
```

---

## Почему этот репозиторий под Apache 2.0, а не под GPL

Код здесь — самостоятельное произведение: меню загрузки, таблицы разделов
и вспомогательные скрипты. Оно не включает и не производно от кода
Meshtastic.

Единственный файл, который попадает внутрь чужой прошивки, —
`dualboot_return.cpp`. Собранный вместе с Meshtastic, он становится частью
произведения под GPL-3.0, и на тот двоичный образ распространяется GPL-3.0
целиком. Совместимость здесь односторонняя и работает в нужную сторону:
код под Apache 2.0 можно включить в произведение под GPLv3.

Поэтому: репозиторий — Apache 2.0, собранный образ Meshtastic — GPL-3.0,
и исходники к нему предоставлены выше.

## Оговорка

Это добросовестное исполнение лицензионных требований, а не юридическая
консультация. Если комплект будет распространяться широко, условия стоит
показать юристу — особенно раздел про письменное предложение исходного кода.

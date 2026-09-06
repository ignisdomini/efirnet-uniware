# -*- coding: utf-8 -*-
"""
EFIRNET UNIWARE — сборка комплекта для публикации в разделе Releases на GitHub.

Собирает из готовых сборок трёх проектов каталог dist/ и один архив к нему.
Образы в git НЕ кладутся: они пересобираются, и каждая пересборка добавляла бы
к истории двадцать мегабайт. Место двоичных файлов — в релизе.

    python tools/make_github_release.py            — собрать
    python tools/make_github_release.py --check    — только проверить наличие и размеры

Что получается:

    dist/efirnet-uniware-<версия>/
        <плата>/bootloader.bin, partitions.bin, otadata_blank.bin,
                uniware.bin, efirnet.bin, meshtastic.bin, meshcore.bin,
                manifest.json, ПРОШИТЬ.txt
        SHA256SUMS.txt
        README.txt
    dist/efirnet-uniware-<версия>.zip

Манифест каждого комплекта несёт версии всех четырёх образов и теги апстримов —
это же и есть исполнение GPL-3.0: по тегу восстанавливается исходный код той
самой Meshtastic, что лежит рядом.
"""
import hashlib
import json
import os
import shutil
import sys
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_web_bundles import (BOARDS, LAYOUT, SLOT_SIZE, OTADATA_OFF, OTADATA_LEN,
                               app_bin, mgr_dir, efr_dir, base_manifest,
                               MESHTAST, MESHCORE)  # noqa: E402

VERSION = '1.0'
UPSTREAM = {
    'efirnet':    ('ЭФИРНЕТ',    '1.2',              'Apache-2.0', 'https://github.com/ignisdomini/Efirnet'),
    'meshtastic': ('Meshtastic', 'v2.7.26.54e0d8d',  'GPL-3.0',    'https://github.com/meshtastic/firmware'),
    'meshcore':   ('MeshCore',   'companion-v1.17.0', 'MIT',       'https://github.com/meshcore-dev/MeshCore'),
}

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIST = os.path.join(HERE, 'dist')

HOWTO = """EFIRNET UNIWARE {ver} — {board}

Три меш-сети в одной плате: ЭФИРНЕТ, Meshtastic, MeshCore.
Выбор кнопкой в меню на экране при каждом включении.

КАК ПРОШИТЬ

Нужен esptool. Ставится одной командой:  pip install esptool

Полностью стереть плату (обязательно при первой установке — комплект
переразмечает флеш целиком, и остатки прежней разметки помешают):

    esptool --chip {chip} -p COM6 erase_flash

Записать все образы:

    esptool --chip {chip} -p COM6 -b 921600 write_flash -z \\
        --flash_mode keep --flash_freq keep --flash_size keep \\
{lines}

Порт COM6 замените на свой. На Linux и macOS это /dev/ttyUSB0 или
/dev/cu.usbserial-*.

Проще — скриптом из репозитория, он сам подставит смещения:

    python tools/flash_release.py {board} COM6

ЧТО ДАЛЬШЕ

Включите плату. Появится заставка, затем меню из трёх сетей.
Короткое нажатие кнопки — следующая сеть, удержание — запуск.
Не трогать кнопку — через 6 секунд стартует та, что выбирали в прошлый раз.

ЛИЦЕНЗИИ

Внутри четыре независимых образа, у каждого своя лицензия. Meshtastic —
GPL-3.0 и в изменённом виде; соответствующий исходный код и способ
воспроизвести сборку описаны в THIRD-PARTY.md репозитория.
"""


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def collect(board, cfg):
    """Пути к образам одного комплекта. Порядок — по смещению во флеше."""
    mb, md = cfg['mb'], mgr_dir(cfg['mgr'])
    offs, size = LAYOUT[mb], SLOT_SIZE[mb]
    parts = [
        ('bootloader.bin', os.path.join(md, 'bootloader.bin'), 0x0 if cfg['s3'] else 0x1000, None),
        ('partitions.bin', os.path.join(md, 'partitions.bin'), 0x8000, None),
        ('uniware.bin',    app_bin(md),                        offs['factory'], size['factory']),
        ('efirnet.bin',    app_bin(efr_dir(cfg['efr'])),       offs['app_efr'], size['app_efr']),
        ('meshtastic.bin', app_bin(os.path.join(MESHTAST, '.pio', 'build', cfg['mt'])),
         offs['app_mt'], size['app_mt']),
        ('meshcore.bin',   app_bin(os.path.join(MESHCORE, '.pio', 'build', cfg['mc'])),
         offs['app_mc'], size['app_mc']),
    ]
    return sorted(parts, key=lambda p: p[2])


def main(check_only=False):
    problems, made = [], []
    root = os.path.join(DIST, 'efirnet-uniware-' + VERSION)
    if not check_only:
        shutil.rmtree(DIST, ignore_errors=True)
        os.makedirs(root)

    sums = []
    for board, cfg in BOARDS.items():
        try:
            parts = collect(board, cfg)
        except (FileNotFoundError, OSError) as ex:
            problems.append('%-22s %s' % (board, ex))
            continue

        line = []
        for name, path, _off, limit in parts:
            if not os.path.exists(path):
                problems.append('%-22s нет файла %s' % (board, path))
                continue
            n = os.path.getsize(path)
            if limit and n > limit:
                problems.append('%-22s %s не влезает: %d > %d' % (board, name, n, limit))
            if limit:
                line.append('%s %.0f%%' % (name.split('.')[0], 100.0 * n / limit))
        if check_only:
            print('%-22s %s' % (board, '  '.join(line)))
            continue

        out = os.path.join(root, board)
        os.makedirs(out)
        for name, path, _off, _l in parts:
            shutil.copyfile(path, os.path.join(out, name))
        with open(os.path.join(out, 'otadata_blank.bin'), 'wb') as f:
            f.write(b'\xff' * OTADATA_LEN)

        entries = [{'path': n, 'offset': o} for n, _p, o, _l in parts]
        entries.append({'path': 'otadata_blank.bin', 'offset': OTADATA_OFF})
        entries.sort(key=lambda e: e['offset'])

        b = base_manifest(board)
        man = {
            'name': 'EFIRNET UNIWARE · ' + (b.get('name', board).split('·', 1)[-1].strip()),
            'kind': 'uniware',
            'version': VERSION,
            'board': board,
            'chip': b.get('chip', ''),
            'flash_mb': cfg['mb'],
            'band': b.get('band', ''),
            'display': b.get('display', ''),
            # Версии и теги апстримов — это и есть исполнение GPL-3.0: по тегу
            # восстанавливается исходный код именно того образа, что лежит рядом.
            'firmwares': [
                {'slot': 'factory', 'file': 'uniware.bin', 'name': 'EFIRNET UNIWARE',
                 'version': VERSION, 'license': 'Apache-2.0'},
                {'slot': 'ota_0', 'file': 'efirnet.bin',    'name': UPSTREAM['efirnet'][0],
                 'version': UPSTREAM['efirnet'][1], 'license': UPSTREAM['efirnet'][2],
                 'upstream': UPSTREAM['efirnet'][3]},
                {'slot': 'ota_1', 'file': 'meshtastic.bin', 'name': UPSTREAM['meshtastic'][0],
                 'version': UPSTREAM['meshtastic'][1], 'license': UPSTREAM['meshtastic'][2],
                 'upstream': UPSTREAM['meshtastic'][3], 'modified': True},
                {'slot': 'ota_2', 'file': 'meshcore.bin',   'name': UPSTREAM['meshcore'][0],
                 'version': UPSTREAM['meshcore'][1], 'license': UPSTREAM['meshcore'][2],
                 'upstream': UPSTREAM['meshcore'][3], 'modified': True},
            ],
            'parts': entries,
        }
        with open(os.path.join(out, 'manifest.json'), 'w', encoding='utf-8') as f:
            json.dump(man, f, ensure_ascii=False, indent=2)

        lines = '\n'.join('        0x%06x %s \\' % (e['offset'], e['path']) for e in entries)
        with open(os.path.join(out, 'ПРОШИТЬ.txt'), 'w', encoding='utf-8', newline='\r\n') as f:
            f.write(HOWTO.format(ver=VERSION, board=board,
                                 chip='esp32s3' if cfg['s3'] else 'esp32',
                                 lines=lines.rstrip(' \\')))

        total = 0
        for name in sorted(os.listdir(out)):
            p = os.path.join(out, name)
            total += os.path.getsize(p)
            sums.append('%s  %s/%s' % (sha256(p), board, name))
        made.append('%-22s %d файлов, %.2f МБ' % (board, len(os.listdir(out)), total / 1048576.0))

    if check_only:
        if problems:
            print('\nПРОБЛЕМЫ:')
            for p in problems:
                print(' ', p)
        return 1 if problems else 0
    if problems:
        print('ПРОБЛЕМЫ:')
        for p in problems:
            print(' ', p)
        return 1

    with open(os.path.join(root, 'SHA256SUMS.txt'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(sorted(sums)) + '\n')
    for src, dst in (('THIRD-PARTY.md', 'THIRD-PARTY.md'), ('NOTICE', 'NOTICE'),
                     ('LICENSE', 'LICENSE')):
        shutil.copyfile(os.path.join(HERE, src), os.path.join(root, dst))

    zpath = os.path.join(DIST, 'efirnet-uniware-%s.zip' % VERSION)
    with zipfile.ZipFile(zpath, 'w', zipfile.ZIP_DEFLATED) as z:
        for base, _dirs, files in os.walk(root):
            for name in files:
                p = os.path.join(base, name)
                # Имена записей только через прямой слэш: с обратными unzip на
                # Linux создаёт один файл со слэшем в имени вместо каталога.
                z.write(p, os.path.relpath(p, DIST).replace(os.sep, '/'))

    for m in made:
        print(m)
    print('\nархив: %s  %.2f МБ' % (zpath, os.path.getsize(zpath) / 1048576.0))
    print('хеши: %s' % os.path.join(root, 'SHA256SUMS.txt'))
    return 0


if __name__ == '__main__':
    sys.exit(main(check_only='--check' in sys.argv))

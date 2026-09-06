# -*- coding: utf-8 -*-
"""
EFIRNET UNIWARE V.1 — сборка комплектов для онлайн-прошивки.

Складывает комплекты в два места сразу:
    efirnet/web/fw/<плата>_uniware/     — для страницы /flash.php на сайте
    efirnet/desktop/fw-uniware/<плата>/ — для программы под Windows
В обоих лежит manifest.json того же вида, что у остальных сборок. И сайт, и
программа читают каталоги сами, поэтому списки плат править руками не нужно.

Запуск:  python tools/build_web_bundles.py [--check]
         --check только проверяет, что все образы на месте и влезают в слоты.

Образы файловых систем намеренно НЕ кладём. У Meshtastic образ littlefs пустой
(1.5 МБ, сжимается до двух килобайт), MeshCore свой раздел форматирует сама при
первом запуске. Класть их — значит гонять мегабайты по сети без всякой пользы.
"""
import json
import os
import shutil
import sys

ROOT     = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLAUDE   = os.path.dirname(ROOT)
EFIRNET  = os.path.join(CLAUDE, 'efirnet')
MESHTAST = os.path.join(CLAUDE, 'meshtastic-src')
MESHCORE = os.path.join(CLAUDE, 'meshcore-src')
WEBFW    = os.path.join(EFIRNET, 'web', 'fw')
# Программа для Windows читает образы из своих каталогов: свои в desktop/fw,
# чужие сети — отдельными папками рядом. UNIWARE кладём так же, отдельно:
# иначе зеркало из collect_firmware.ps1 утащит комплекты в desktop/fw и
# программа покажет их как обычные прошивки ЭФИРНЕТа.
DESKFW   = os.path.join(EFIRNET, 'desktop', 'fw-uniware')

# Смещения слотов. Совпадают с partitions_triboot_{8,16}mb.csv.
LAYOUT = {
    8:  {'factory': 0x10000, 'app_efr': 0x80000, 'app_mt': 0x160000, 'app_mc': 0x400000},
    16: {'factory': 0x10000, 'app_efr': 0x80000, 'app_mt': 0x200000, 'app_mc': 0x600000},
}
SLOT_SIZE = {
    8:  {'factory': 0x70000, 'app_efr': 0xE0000,  'app_mt': 0x2A0000, 'app_mc': 0x1A0000},
    16: {'factory': 0x70000, 'app_efr': 0x180000, 'app_mt': 0x400000, 'app_mc': 0x300000},
}
OTADATA_OFF, OTADATA_LEN = 0xe000, 0x2000

# Плата -> где чей образ лежит. Ключи те же, что у каталогов web/fw.
BOARDS = {
    'heltec_v3': dict(
        mb=8, s3=True,
        mgr='heltec_v3', efr='heltec_v3_uniware',
        mt='heltec-v3-uniware', mc='Heltec_v3_uniware'),
    'heltec_stick_lite_v3': dict(
        mb=8, s3=True,
        mgr='heltec_stick_lite_v3', efr='heltec_stick_lite_v3_uniware',
        mt='heltec-wsl-v3-uniware', mc='Heltec_WSL3_uniware'),
    'lilygo_t3s3': dict(
        mb=8, s3=True,
        mgr='lilygo_t3s3', efr='lilygo_t3s3_uniware',
        mt='tlora-t3s3-v1-uniware', mc='LilyGo_T3S3_uniware'),
    'heltec_v2': dict(
        mb=8, s3=False,
        mgr='heltec_v2', efr='heltec_v2_uniware',
        mt='heltec-v2_1-uniware', mc='Heltec_v2_uniware'),
    'heltec_v4': dict(
        mb=16, s3=True,
        mgr='heltec_v4', efr='heltec_v4_uniware',
        mt='heltec-v4-uniware', mc='heltec_v4_uniware'),
    'heltec_v4_r8': dict(
        mb=16, s3=True,
        mgr='heltec_v4_r8', efr='heltec_v4_r8_uniware',
        mt='hv4r8-uniware', mc='heltec_v4_r8_uniware'),
}

VERSION = '1.0'


def mgr_dir(env):
    return os.path.join(ROOT, 'bootmanager', '.pio', 'build', env)


def efr_dir(env):
    return os.path.join(EFIRNET, 'firmware', '.pio', 'build', env)


def app_bin(build_dir):
    """Образ приложения. Meshtastic переименовывает свой в firmware-<вариант>.bin,
    остальные оставляют firmware.bin — берём то, что есть, но никогда не .factory."""
    direct = os.path.join(build_dir, 'firmware.bin')
    if os.path.exists(direct):
        return direct
    cands = [f for f in os.listdir(build_dir)
             if f.startswith('firmware') and f.endswith('.bin') and '.factory' not in f]
    if not cands:
        raise FileNotFoundError('нет образа приложения в ' + build_dir)
    return os.path.join(build_dir, sorted(cands)[0])


def base_manifest(board):
    p = os.path.join(WEBFW, board, 'manifest.json')
    if os.path.exists(p):
        with open(p, encoding='utf-8') as f:
            return json.load(f)
    return {}


def main(check_only=False):
    problems, done = [], []

    for board, cfg in BOARDS.items():
        mb   = cfg['mb']
        offs = LAYOUT[mb]
        size = SLOT_SIZE[mb]

        try:
            md = mgr_dir(cfg['mgr'])
            src = {
                'bootloader.bin': (os.path.join(md, 'bootloader.bin'),
                                   0x0 if cfg['s3'] else 0x1000, None),
                'partitions.bin': (os.path.join(md, 'partitions.bin'), 0x8000, None),
                'uniware.bin':    (app_bin(md),                  offs['factory'], size['factory']),
                'efirnet.bin':    (app_bin(efr_dir(cfg['efr'])), offs['app_efr'], size['app_efr']),
                'meshtastic.bin': (app_bin(os.path.join(MESHTAST, '.pio', 'build', cfg['mt'])),
                                   offs['app_mt'], size['app_mt']),
                'meshcore.bin':   (app_bin(os.path.join(MESHCORE, '.pio', 'build', cfg['mc'])),
                                   offs['app_mc'], size['app_mc']),
            }
        except (FileNotFoundError, OSError) as ex:
            problems.append('%-22s %s' % (board, ex))
            continue

        line = []
        for name, (path, off, limit) in src.items():
            if not os.path.exists(path):
                problems.append('%-22s нет файла %s' % (board, path))
                continue
            n = os.path.getsize(path)
            if limit and n > limit:
                problems.append('%-22s %s не влезает: %d > %d' % (board, name, n, limit))
            if limit:
                line.append('%s %d/%d (%.0f%%)' % (name.split('.')[0], n, limit, 100.0 * n / limit))

        if check_only:
            print('%-22s %s' % (board, '  '.join(line)))
            continue

        out = os.path.join(WEBFW, board + '_uniware')
        os.makedirs(out, exist_ok=True)
        parts = []
        for name, (path, off, _limit) in src.items():
            shutil.copyfile(path, os.path.join(out, name))
            parts.append({'path': name, 'offset': off})

        # Пустой otadata пишется последним: он же и есть команда «грузись в меню».
        # Отдельного шага «стереть» у esptool-js нет, но запись 0xFF равносильна
        # стиранию — сектор всё равно предварительно чистится.
        with open(os.path.join(out, 'otadata_blank.bin'), 'wb') as f:
            f.write(b'\xff' * OTADATA_LEN)
        parts.append({'path': 'otadata_blank.bin', 'offset': OTADATA_OFF})

        parts.sort(key=lambda p: p['offset'])

        b = base_manifest(board)
        name = b.get('name', board)
        if '·' in name:
            name = name.split('·', 1)[1].strip()
        man = {
            'name':    'EFIRNET UNIWARE · ' + name,
            # По этому полю страница заливки раскладывает сборки по группам:
            # обычная прошивка отдельно, мультизагрузчик отдельно. У старых
            # манифестов поля нет, и они молча считаются обычными.
            'kind':    'uniware',
            'chip':    b.get('chip', ''),
            'version': VERSION,
            'band':    b.get('band', ''),
            'display': b.get('display', ''),
            # Страница склеивает chip · band · display · notes в одну строку под
            # выпадающим списком, поэтому здесь коротко.
            'notes':   '3 меш-сети в одной прошивке: ЭФИРНЕТ, Meshtastic, MeshCore',
            'parts':   parts,
        }
        with open(os.path.join(out, 'manifest.json'), 'w', encoding='utf-8') as f:
            json.dump(man, f, ensure_ascii=False, indent=4)

        # Зеркало для программы под Windows. Имя папки без суффикса _uniware:
        # внутри fw-uniware он был бы повтором, а у чужих сетей папки названы
        # просто по плате.
        dsk = os.path.join(DESKFW, board)
        os.makedirs(dsk, exist_ok=True)
        for part in parts:
            shutil.copyfile(os.path.join(out, part['path']),
                            os.path.join(dsk, part['path']))
        dman = dict(man, network='uniware')
        with open(os.path.join(dsk, 'manifest.json'), 'w', encoding='utf-8') as f:
            json.dump(dman, f, ensure_ascii=False, indent=4)

        total = sum(os.path.getsize(os.path.join(out, p['path'])) for p in parts)
        done.append('%-22s %d частей, %.2f МБ' % (board, len(parts), total / 1048576.0))

    for d in done:
        print(d)
    if problems:
        print('\nПРОБЛЕМЫ:')
        for p in problems:
            print(' ', p)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(check_only='--check' in sys.argv))

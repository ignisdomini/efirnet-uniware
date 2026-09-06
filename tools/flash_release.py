# -*- coding: utf-8 -*-
"""
EFIRNET UNIWARE — заливка готового комплекта из релиза.

Работает на Windows, Linux и macOS: нужен только Python и esptool
(pip install esptool). Смещения берутся из manifest.json рядом с образами,
а не задаются руками — ошибиться адресом невозможно.

    python tools/flash_release.py                       — показать платы
    python tools/flash_release.py heltec_v3 COM6
    python tools/flash_release.py heltec_v3 /dev/ttyUSB0 --erase

--erase стирает флеш целиком перед записью. При ПЕРВОЙ установке это нужно:
комплект переразмечает память, и остатки прежней разметки ложатся посреди
чужих слотов. При повторной заливке того же комплекта не обязательно.
"""
import argparse
import glob
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def find_bundles():
    """Комплекты ищем и в распакованном релизе, и в собранном локально dist/."""
    roots = []
    for pat in ('dist/efirnet-uniware-*', '.', '..'):
        for d in glob.glob(os.path.join(HERE, pat)):
            if os.path.isdir(d):
                roots.append(d)
    found = {}
    for r in roots:
        for m in glob.glob(os.path.join(r, '*', 'manifest.json')):
            board = os.path.basename(os.path.dirname(m))
            found.setdefault(board, os.path.dirname(m))
    return found


def esptool_cmd():
    """esptool ставится и как модуль, и как отдельная команда — берём что есть."""
    try:
        subprocess.run([sys.executable, '-m', 'esptool', 'version'],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
        return [sys.executable, '-m', 'esptool']
    except Exception:
        return ['esptool']


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('board', nargs='?', help='папка комплекта, например heltec_v3')
    ap.add_argument('port', nargs='?', help='COM6, /dev/ttyUSB0, /dev/cu.usbserial-…')
    ap.add_argument('--erase', action='store_true', help='стереть флеш перед записью')
    ap.add_argument('--baud', default='921600')
    a = ap.parse_args()

    bundles = find_bundles()
    if not a.board or not a.port or a.board not in bundles:
        print('Использование: python tools/flash_release.py <плата> <порт> [--erase]')
        print('Найденные комплекты:')
        for b in sorted(bundles):
            print('  ', b)
        if not bundles:
            print('   (ни одного — распакуйте архив релиза или соберите dist/)')
        return 1

    d = bundles[a.board]
    with open(os.path.join(d, 'manifest.json'), encoding='utf-8') as f:
        man = json.load(f)
    chip = 'esp32s3' if 'S3' in man.get('chip', '') else 'esp32'

    print('%s — %s' % (man.get('name', a.board), man.get('chip', '')))
    for fw in man.get('firmwares', []):
        print('   %-9s %-16s %s' % (fw['slot'], fw['name'], fw.get('version', '')))
    print()

    esp = esptool_cmd()
    if a.erase:
        print('Стираю флеш…')
        subprocess.run(esp + ['--chip', chip, '-p', a.port, 'erase_flash'], check=True)

    args = []
    for p in man['parts']:
        path = os.path.join(d, p['path'])
        if not os.path.exists(path):
            print('НЕТ ФАЙЛА:', path)
            return 1
        args += [hex(p['offset']), path]
        print('  0x%06x  %-18s %8d Б' % (p['offset'], p['path'], os.path.getsize(path)))
    print()

    # keep для режима, частоты и объёма: у каждого образа в заголовке уже стоят
    # значения, с которыми его собирали. Переписывать их своими — верный способ
    # получить плату, которая не выходит из ребута.
    subprocess.run(esp + ['--chip', chip, '-p', a.port, '-b', a.baud,
                          'write_flash', '-z',
                          '--flash_mode', 'keep', '--flash_freq', 'keep',
                          '--flash_size', 'keep'] + args, check=True)

    print('\nГотово. Включите плату — появится меню выбора сети.')
    return 0


if __name__ == '__main__':
    sys.exit(main())

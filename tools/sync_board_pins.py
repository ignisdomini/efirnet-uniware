# -*- coding: utf-8 -*-
"""
Обновляет копию board_pins.h из проекта ЭФИРНЕТ.

Менеджер загрузки берёт распиновку из того же файла, что и сама прошивка, —
иначе платы разъедутся: у прошивки один пин Vext, у меню другой, и экран
не включится. Но тянуть путь во второй репозиторий нельзя: у того, кто
склонировал только этот проект, такого каталога нет.

Поэтому в bootmanager/include/ лежит копия, а этот скрипт её обновляет.

    python tools/sync_board_pins.py           — показать, различаются ли файлы
    python tools/sync_board_pins.py --apply   — обновить копию

Путь к ЭФИРНЕТу берётся из --src, переменной EFIRNET_DIR или подбирается
рядом с этим репозиторием.
"""
import argparse
import hashlib
import os
import shutil
import sys

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
COPY = os.path.join(HERE, 'bootmanager', 'include', 'board_pins.h')
REL  = os.path.join('firmware', 'include', 'board_pins.h')


def digest(path):
    with open(path, 'rb') as f:
        return hashlib.sha256(f.read()).hexdigest()


def find_source(explicit):
    for cand in (explicit, os.environ.get('EFIRNET_DIR'),
                 os.path.join(os.path.dirname(HERE), 'efirnet')):
        if cand and os.path.isfile(os.path.join(cand, REL)):
            return os.path.join(cand, REL)
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--src', help='каталог проекта ЭФИРНЕТ')
    ap.add_argument('--apply', action='store_true', help='обновить копию')
    a = ap.parse_args()

    src = find_source(a.src)
    if not src:
        print('Проект ЭФИРНЕТ не найден. Укажите --src или EFIRNET_DIR.')
        print('Это не ошибка сборки: копия в bootmanager/include/ самодостаточна.')
        return 0

    if not os.path.exists(COPY):
        print('Копии нет вовсе.')
    elif digest(src) == digest(COPY):
        print('Копия совпадает с оригиналом:', src)
        return 0
    else:
        print('РАЗОШЛИСЬ: копия в репозитории отличается от', src)

    if not a.apply:
        print('Запустите с --apply, чтобы обновить.')
        return 1

    shutil.copyfile(src, COPY)
    print('Обновлено:', COPY)
    return 0


if __name__ == '__main__':
    sys.exit(main())

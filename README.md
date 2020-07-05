# bkbtl - BKBTL emulator, Win32 version

[![License: LGPL v3](https://img.shields.io/badge/License-LGPL%20v3-blue.svg)](https://www.gnu.org/licenses/lgpl-3.0)
[![Build status](https://ci.appveyor.com/api/projects/status/a1p2sovj2ew7iime?svg=true)](https://ci.appveyor.com/project/nzeemin/bkbtl)
[![CodeFactor](https://www.codefactor.io/repository/github/nzeemin/bkbtl/badge)](https://www.codefactor.io/repository/github/nzeemin/bkbtl)

**BKBTL** — **BK Back to Life!** — is [BK0010/BK0011](http://en.wikipedia.org/wiki/Elektronika_BK) emulator.
The emulation project started on Nov. 14, 2009 and based on [UKNCBTL](https://github.com/nzeemin/ukncbtl) code.
BK is soviet home computer based on 16-bit PDP-11 compatible processor K1801VM1.

The BKBTL project consists of:
* [**bkbtl**](https://github.com/nzeemin/bkbtl) — Win32 version, for Windows.
* [**bkbtl-qt**](https://github.com/nzeemin/bkbtl-qt) is Qt based BKBTL branch, works under Windows, Linux and Mac OS X.
* [**bkbtl-testbench**](https://github.com/nzeemin/bkbtl-testbench) — test bench for regression testing.
* [**bkbtl-doc**](https://github.com/nzeemin/bkbtl-doc) — documentation and screenshots.
* Project wiki: https://github.com/nzeemin/bkbtl-doc/wiki

Current status: Beta, under development.

Emulated:
* BK-0010.01 and BK-0011M
* CPU
* Motherboard
* Screen — black and white mode, color mode, short mode, BK0011 color mode palettes
* Keyboard (but mapped not all BK keys)
* Reading from tape (WAV file), writing to tape (WAV file)
* Sound
* Joystick (numpad keys, external joystick)
* Covox
* Floppy drive (at least in BK11M configuration)
* Programmable timer (partially)

**BKBTL** — **BK Back to Life!** — это проект эмуляции советского бытового компьютера [БК-0010/БК-0011](http://ru.wikipedia.org/wiki/БК), построенного на 16-разрядном процессоре К1801ВМ1, совместимом по системе команд с семейством PDP-11. Проект начат 14 ноября 2009 года. Основан на коде проекта [UKNCBTL](https://github.com/nzeemin/ukncbtl).

В проект BKBTL входят репозитории:
* **bkbtl** — Windows-версия. Написана под Win32 и требует поддержки Юникода, поэтому набор версий Windows — 2000/2003/2008/XP/Vista/7.
* [**bkbtl-qt**](https://github.com/nzeemin/bkbtl-qt) — Qt-версия. Работает под Windows, Linux и Mac OS X. В Qt-версии нет поддержки звука, нет окна карты памяти, нет поддержки внешнего джойстика; в остальном возможности те же.
* [**bkbtl-testbench**](https://github.com/nzeemin/bkbtl-testbench) — тестовый стенд для регрессионного тестирования.
* [**bkbtl-doc**](https://github.com/nzeemin/bkbtl-doc) — документация и скриншоты.

## Состояние эмулятора

Бета-версия. Многие игры пока не работают. Дисковод более-менее работает в конфигурации БК-0011М.

Поддерживаются конфигурации:
БК-0010.01+Бейсик, БК-0010.01+Фокал+тесты, БК-0010.01+дисковод, БК-0011М+тесты, БК-0011М+дисковод.

Эмулируется:
* БК-0010.01 и БК-0011М
* процессор (тест 791401 проходит, тест 791404 НЕ проходит)
* материнская плата (частично, тест памяти 791323 НЕ проходит)
* экран — черно-белый, цветной, усеченный режим, палитры цветного режима БК-0011
* клавиатура — маппинг PC-клавиатуры на БК-клавиатуру зависит от переключателя РУС/ЛАТ в БК (но размаплены не все клавиши)
* чтение с магнитофона (из файла формата WAV), запись на магнитофон (в WAV-файл)
* звук пьезодинамика БК
* Covox
* джойстик (клавиши NumPad, внешний джойстик)
* ИРПС на регистрах 177560..0177566 (пока только передача данных в отладочное окно) — используется для прогона тестов
* дисковод (более-менее в конфигурации БК-0011М)
* AY-3-8910

Планируется сделать:

* сделать правильную систему прерываний процессора (пока сделано ближе к ВМ2)
* прогон тестов 791404 и 791323, отладка работы машины на них
* доделать маппинг клавиатуры
* программируемый таймер (нужно доделать)
* мышь

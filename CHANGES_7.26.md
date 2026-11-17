# 7.26 — zeppelins and ships: what the server sends, and what the client does when it is silent

## Диагностика (perf_events.txt)

- `transport <guid> entry <e> display <d> at x y z, pose map <m> (we are <M>), phase <p> of <period>, N yd, moving 0/1, model loaded|NONE`
  — раз в секунду **каждый** транспорт, а не первый попавшийся (в 7.25 был один общий таймер на всех, из-за чего в лог
  попадал только один объект из пятнадцати).
- `transport create|out of range|destroyed|replaced: guid ... entry ... display ... progress ... at ... N yd from us`
  — когда объект появился или пропал.
- `update packet: <blocks> blocks, <bytes> bytes` и `update create: guid ... type ... entry ... display ...`
  — что именно лежало в каждом `SMSG_UPDATE_OBJECT` (проверка парсинга на уровне блоков).

## Что показал лог (стоя у башни Оргриммара)

```
[ 32.9 s] online (in the world)
[ 32.9 s] transport create: ... entry 176244 / 177233 / 20808 / 176231 (display 3015, корабли) + 11 лифтов
[ 61.3 s] transport create: ... entry 164871 display 3031 progress 374454380      <- зеппелин
[ 61.5 s] ride: ... entry 164871 ... (pose 1317.9 -4646.3 71.8, 5 yd)
[ 62.5 s] transport ... entry 164871 display 3031 at 1317.6 -4641.0 71.7, pose map 1, phase 114455 of 356175, 16 yd, moving 1
```

`Map::SendInitTransports` при входе отдаёт все транспорты, которые **уже в карте**; зеппелина среди них не было. Он вошёл
в карту телепортом (`ShipTransport::TeleportTransport`) и был объявлен только на update-кадре — то есть на отходе.
Поза и путь при этом правильные (`phase 114455 of 356175`, стоянка `1318.11 -4658.05` из `transports.txt`), конвертер и
пак ни при чём: модель `OWMO` на месте, маршрут на месте.

Серверный фикс (одна строка после `TeleportTransport`) — в `docs/SERVER_TRANSPORT_STOP.md`. Чужой сервер не трогаем.

## Клиентский фикс: помнить, где транспорт был

- В конце сессии (и раз в 30 с) клиент пишет `sdmc:/3ds/w3dproto/transport_cache.txt`:
  `S <unix-time>`, затем `T <entry> <display> <progress>` для каждого пассажирского транспорта (type 15).
- При входе, если сервер ещё не прислал транспорт с таким entry, клиент ставит его на путь сам: `progress + прошедшее
  время`, шаблон запрашивается обычным `CMSG_GAMEOBJECT_QUERY`, поза считается тем же `TransportPath::at()`.
- Как только приходит настоящий `create`, «догадка» удаляется (`dropPredictedTransports`).
- Если прогресс сервера не сходится с памятью больше чем на 5 минут (перезапуск реалма), все догадки сбрасываются.
- Догадка рисуется только в той карте, куда её ставит фаза, и по ней нельзя ходить (`floorQ`, посадка на транспорт её
  игнорируют).
- Память старше 6 часов не используется.

Итог: зеппелин виден на подлёте и на стоянке сразу после входа, а не «появляется из воздуха» на отходе, как только
клиент хоть раз его видел.

## Тесты

`net/tests/predicted_transport_test.cpp` (в `run_transport_tests.sh`): догадка получает верную позу, стоит на стоянке,
едет по пути, не попадает в память, удаляется при настоящем `create`, не дублируется. Все 4 транспортных теста — PASS.

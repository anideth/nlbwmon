# Метрики nlbwmon в Prometheus и шаблон Grafana

Эта интеграция **не является частью форка**: используется сторонний Lua-коллектор автора **giuliomagnifico**, [исходный репозиторий](https://github.com/giuliomagnifico/openwrt-nlbwmon-prometheus-collector). Форк nlbwmon сохраняет обычный вывод `nlbw -c csv`, а отдельно и одновременно отправляет события NEW/DESTROY в Python-агрегатор для OpenSearch.

## Схема

- **OpenSearch:** `nlbwmon → TCP JSONL → nlbwmon-aggregator → OpenSearch` (экспериментальный экспорт потоков).
- **Метрики:** `nlbwmon → nlbw -c csv → nlbwmon.lua → prometheus-node-exporter-lua → Prometheus/VictoriaMetrics → Grafana` (независимый сборщик).

## Установка Lua-коллектора на OpenWrt

Убедитесь, что установлен `prometheus-node-exporter-lua`, работает `nlbw -c csv`, а нужные зависимости соответствуют версии прошивки. Для OpenWrt 25.12 с `apk`:

```sh
apk update && apk add prometheus-node-exporter-lua
mkdir -p /usr/lib/lua/prometheus-collectors && wget -O /usr/lib/lua/prometheus-collectors/nlbwmon.lua https://raw.githubusercontent.com/giuliomagnifico/openwrt-nlbwmon-prometheus-collector/main/nlbwmon.lua && /etc/init.d/prometheus-node-exporter-lua restart
wget -qO- 'http://127.0.0.1:9100/metrics?collect[]=nlbwmon' | grep '^nlbwmon_' | head
```

Это загрузка кода третьей стороны непосредственно с её ветки `main`; для воспроизводимой установки зафиксируйте проверенный коммит/контрольную сумму. При уже настроенном Prometheus scrape для OpenWrt отдельный endpoint обычно не требуется.

Метрики: `nlbwmon_rx_bytes`, `nlbwmon_tx_bytes`, `nlbwmon_connections`; метки: `family`, `proto`, `port`, `mac`, `ip`, `hostname`, `layer7`. Счётчики сбрасываются вместе с периодом учёта nlbwmon; графики зависят от частоты опроса и сохранности исторических данных.

## Шаблон Grafana

**[Наш адаптированный JSON-шаблон Grafana](../../grafana/openwrt-nlbwmon-dashboard.json)** — экспорт пользовательской панели OpenWrt с переменной **WAN interface**. Он не является оригинальным шаблоном Lua-коллектора; см. также [исходный пример автора коллектора](https://github.com/giuliomagnifico/openwrt-nlbwmon-prometheus-collector/blob/main/Grafana%20dashboard%20example.json).

**Перед импортом:** скачайте JSON и замените во всём файле `PROMETHEUS_DATASOURCE_UID` на UID своего источника Prometheus/VictoriaMetrics (Grafana → Connections → Data sources → нужный источник → UID). Это не имя источника, а именно UID. Затем в Grafana выберите **Dashboards → New → Import** и загрузите файл. Формат файла — Grafana Dashboard API v2; проверьте поддержку в вашей версии Grafana. В экспортированном шаблоне исходный UID заменён для защиты сведений о локальной установке.

Вверху панели выберите **WAN interface**: по умолчанию `pppoe-wan`, но доступны интерфейсы из метрики `node_network_receive_bytes_total` для выбранных `job`, `node`, `port`. WAN RX/TX используют `$wan_interface`; остальные панели не менялись. **Пользователь подтвердил работу переключателя в своей Grafana**, но переносимый JSON с заменённым UID ещё не проверялся на чистой установке.

Пример запроса для трафика по устройствам за выбранный интервал:

```promql
topk(20, sum by (hostname, ip) (increase(nlbwmon_rx_bytes[$__range]) + increase(nlbwmon_tx_bytes[$__range])))
```

Для суммарного трафика выберите `bytes (IEC)`, для скорости `rate(...[$__rate_interval]) * 8` — `bits/sec`. Для панелей с одним итоговым значением используйте Instant-запрос, если это соответствует типу панели.

**Авторство:** Lua-коллектор и его оригинальный пример Grafana — сторонний проект giuliomagnifico. Опубликованный здесь адаптированный dashboard предоставлен пользователем и переработан для выбора WAN-интерфейса; он не заявляется как оригинальная работа автора Lua. При дальнейшем распространении сохраняйте ссылки на исходный проект. Проверка авторства всех исходных панелей и совместимости Grafana на других установках не завершена.

## Обновление OpenWrt и резервные копии

Lua-файл вручную установлен в `/usr/lib` и может исчезнуть после обновления прошивки. Добавьте его в перечень сохраняемых файлов:

```sh
grep -qxF '/usr/lib/lua/prometheus-collectors/nlbwmon.lua' /etc/sysupgrade.conf || echo '/usr/lib/lua/prometheus-collectors/nlbwmon.lua' >> /etc/sysupgrade.conf
```

Перед обновлением проверьте `sysupgrade -l`, сделайте внешнюю резервную копию и сохраните список пакетов. Подробная англоязычная инструкция: [PROMETHEUS_AND_BACKUP.md](../PROMETHEUS_AND_BACKUP.md).

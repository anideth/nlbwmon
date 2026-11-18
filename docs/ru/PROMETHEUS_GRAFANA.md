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

**[Скачать исходный JSON-шаблон Grafana автора](https://github.com/giuliomagnifico/openwrt-nlbwmon-prometheus-collector/blob/main/Grafana%20dashboard%20example.json)**. В Grafana откройте **Dashboards → New → Import**, загрузите JSON и выберите источник Prometheus/VictoriaMetrics, поддерживающий PromQL.

Пример запроса для трафика по устройствам за выбранный интервал:

```promql
topk(20, sum by (hostname, ip) (increase(nlbwmon_rx_bytes[$__range]) + increase(nlbwmon_tx_bytes[$__range])))
```

Для суммарного трафика выберите `bytes (IEC)`, для скорости `rate(...[$__rate_interval]) * 8` — `bits/sec`. Для панелей с одним итоговым значением используйте Instant-запрос, если это соответствует типу панели.

**Важно об авторстве:** оригинальный Lua-файл и JSON-шаблон принадлежат стороннему проекту. В этом репозитории мы даём ссылки, но не включаем копии этих файлов. Ранее изменённый пользователем dashboard **не найден в текущих ветках исходников**; его JSON нужно восстановить из Grafana/экспорта, проверить на внутренние имена, адреса, UID источников и лицензию, затем опубликовать отдельно с указанием исходного автора.

## Обновление OpenWrt и резервные копии

Lua-файл вручную установлен в `/usr/lib` и может исчезнуть после обновления прошивки. Добавьте его в перечень сохраняемых файлов:

```sh
grep -qxF '/usr/lib/lua/prometheus-collectors/nlbwmon.lua' /etc/sysupgrade.conf || echo '/usr/lib/lua/prometheus-collectors/nlbwmon.lua' >> /etc/sysupgrade.conf
```

Перед обновлением проверьте `sysupgrade -l`, сделайте внешнюю резервную копию и сохраните список пакетов. Подробная англоязычная инструкция: [PROMETHEUS_AND_BACKUP.md](../PROMETHEUS_AND_BACKUP.md).

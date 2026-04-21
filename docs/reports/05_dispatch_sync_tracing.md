# Отчет 5. Диспетчеризация, синхронизация и трассировка

## Покрываемые стадии

- Стадия 7: Dispatch + Synchronization
- Стадия 11: Host Multithreading Runtime
- Стадия 12: Tracing

## Цель отчета

Показать, как система оркестрирует жизненный цикл процессов в одно- и многопоточном режиме, как предотвращаются гонки и deadlock, и как трассировка обеспечивает наблюдаемость.

## Что обязательно описать

1. Роли `Dispatcher`, `MPDispatcher`, `DispatcherPool`.
2. Разделение kernel-internal sync и simulated sync (`Mutex`, `Semaphore`).
3. Модель deadlock detection: simulated wait-for + internal lock-order.
4. Deterministic mode для `N > 1`.
5. `Tracer`, `NullTracer`, `TraceScope`, sink-архитектура.

## Код и артефакты (вставить фрагменты)

- `src/include/contur/dispatch/i_dispatcher.h`
- `src/contur/dispatch/dispatcher.cpp`
- `src/contur/dispatch/mp_dispatcher.cpp`
- `src/contur/dispatch/dispatcher_pool.cpp`
- `src/contur/sync/deadlock_detector.cpp`
- `src/include/contur/tracing/i_tracer.h`
- `src/contur/tracing/tracer.cpp`
- `src/include/contur/tracing/trace_scope.h`

## Место под рисунки

- Рисунок 1. Схема worker lanes и work stealing.
- Рисунок 2. Пример trace-лога с глубиной вызовов.

## Диаграмма 1 (Lifecycle + dispatcher sequence, mxGraphModel)

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="100" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="Kernel" vertex="1"><mxGeometry x="40" y="60" width="100" height="50" as="geometry" /></mxCell>
    <mxCell id="101" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="Dispatcher" vertex="1"><mxGeometry x="200" y="60" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="102" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="Scheduler" vertex="1"><mxGeometry x="380" y="60" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="103" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="Engine" vertex="1"><mxGeometry x="550" y="60" width="100" height="50" as="geometry" /></mxCell>
    <mxCell id="104" edge="1" parent="1" source="100" target="101" style="endArrow=classic;html=1;" value="create/tick"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="105" edge="1" parent="1" source="101" target="102" style="endArrow=classic;html=1;" value="select"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="106" edge="1" parent="1" source="101" target="103" style="endArrow=classic;html=1;" value="execute"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Диаграмма 2 (Deadlock dual-graph, mxGraphModel)

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="110" parent="1" style="rounded=1;whiteSpace=wrap;html=1;dashed=1;" value="Simulated Wait-For Graph" vertex="1"><mxGeometry x="40" y="40" width="300" height="220" as="geometry" /></mxCell>
    <mxCell id="111" parent="1" style="rounded=1;whiteSpace=wrap;html=1;dashed=1;" value="Internal Lock-Order Graph" vertex="1"><mxGeometry x="380" y="40" width="300" height="220" as="geometry" /></mxCell>
    <mxCell id="112" parent="110" style="ellipse;whiteSpace=wrap;html=1;" value="P1" vertex="1"><mxGeometry x="30" y="70" width="50" height="40" as="geometry" /></mxCell>
    <mxCell id="113" parent="110" style="ellipse;whiteSpace=wrap;html=1;" value="R1" vertex="1"><mxGeometry x="130" y="70" width="50" height="40" as="geometry" /></mxCell>
    <mxCell id="114" edge="1" parent="110" source="112" target="113" style="endArrow=classic;html=1;" value="wait"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="115" parent="111" style="ellipse;whiteSpace=wrap;html=1;" value="L1" vertex="1"><mxGeometry x="30" y="70" width="50" height="40" as="geometry" /></mxCell>
    <mxCell id="116" parent="111" style="ellipse;whiteSpace=wrap;html=1;" value="L2" vertex="1"><mxGeometry x="130" y="70" width="50" height="40" as="geometry" /></mxCell>
    <mxCell id="117" edge="1" parent="111" source="115" target="116" style="endArrow=classic;html=1;" value="before"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

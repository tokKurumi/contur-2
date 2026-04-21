# Отчет 4. Планирование процессов

## Покрываемые стадии

- Стадия 6: Scheduling

## Цель отчета

Сравнить и обосновать стратегии планирования, показать, как единый контракт `ISchedulingPolicy` позволяет менять алгоритм без изменений ядра.

## Что обязательно описать

1. Контракт `ISchedulingPolicy` и его методы (`selectNext`, `shouldPreempt`).
2. Краткая теория всех 7 алгоритмов.
3. Как `Scheduler` управляет очередями состояний.
4. Как рассчитываются статистики и предсказания burst-time.

## Код и артефакты (вставить фрагменты)

- `src/include/contur/scheduling/i_scheduling_policy.h`
- `src/contur/scheduling/scheduler.cpp`
- `src/contur/scheduling/fcfs_policy.cpp`
- `src/contur/scheduling/round_robin_policy.cpp`
- `src/contur/scheduling/mlfq_policy.cpp`
- `src/contur/scheduling/statistics.cpp`

## Место под рисунки

- Рисунок 1. Сравнительный график ожидания/отклика для 7 алгоритмов.
- Рисунок 2. Пример Gantt-диаграммы для одного набора процессов.

## Диаграмма 1 (Strategy pattern, mxGraphModel)

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="80" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="Scheduler" vertex="1"><mxGeometry x="70" y="90" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="81" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="ISchedulingPolicy" vertex="1"><mxGeometry x="270" y="90" width="150" height="50" as="geometry" /></mxCell>
    <mxCell id="82" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="FCFS" vertex="1"><mxGeometry x="470" y="40" width="100" height="40" as="geometry" /></mxCell>
    <mxCell id="83" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="RoundRobin" vertex="1"><mxGeometry x="470" y="90" width="100" height="40" as="geometry" /></mxCell>
    <mxCell id="84" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="MLFQ" vertex="1"><mxGeometry x="470" y="140" width="100" height="40" as="geometry" /></mxCell>
    <mxCell id="85" edge="1" parent="1" source="80" target="81" style="endArrow=classic;html=1;" value="has"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="86" edge="1" parent="1" source="82" target="81" style="endArrow=block;html=1;dashed=1;" value="implements"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="87" edge="1" parent="1" source="83" target="81" style="endArrow=block;html=1;dashed=1;" value="implements"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="88" edge="1" parent="1" source="84" target="81" style="endArrow=block;html=1;dashed=1;" value="implements"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Диаграмма 2 (Gantt-шаблон, mxGraphModel)

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="90" parent="1" style="shape=swimlane;whiteSpace=wrap;html=1;" value="CPU Timeline" vertex="1"><mxGeometry x="40" y="70" width="640" height="120" as="geometry" /></mxCell>
    <mxCell id="91" parent="90" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;" value="P1" vertex="1"><mxGeometry x="40" y="40" width="100" height="40" as="geometry" /></mxCell>
    <mxCell id="92" parent="90" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;" value="P2" vertex="1"><mxGeometry x="150" y="40" width="80" height="40" as="geometry" /></mxCell>
    <mxCell id="93" parent="90" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;" value="P3" vertex="1"><mxGeometry x="240" y="40" width="120" height="40" as="geometry" /></mxCell>
    <mxCell id="94" parent="90" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#f8cecc;" value="P1" vertex="1"><mxGeometry x="370" y="40" width="90" height="40" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

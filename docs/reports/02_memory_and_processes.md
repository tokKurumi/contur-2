# Отчет 2. Память и процессы

## Покрываемые стадии

- Стадия 2: Memory Subsystem
- Стадия 3: Process Model

## Цель отчета

Показать модель памяти и процесса в симуляторе: трансляция адресов, таблицы страниц, стратегии замещения, жизненный цикл процесса, приоритеты.

## Что обязательно описать

1. Роль `IMemory`, `IMMU`, `IVirtualMemory`, `PageTable`.
2. Отличия FIFO/LRU/Clock/Optimal и где какая стратегия полезна.
3. Структуру `PCB` и `ProcessImage`.
4. Валидацию переходов `ProcessState`.

## Код и артефакты (вставить фрагменты)

- `src/include/contur/memory/i_memory.h`
- `src/include/contur/memory/i_mmu.h`
- `src/contur/memory/mmu.cpp`
- `src/include/contur/memory/page_table.h`
- `src/include/contur/process/state.h`
- `src/include/contur/process/pcb.h`
- `src/include/contur/process/process_image.h`

## Место под рисунки

- Рисунок 1. Иллюстрация виртуальной и физической памяти.
- Рисунок 2. Пример состояния очередей процессов до/после переключения.

## Диаграмма 1 (Class diagram memory/process, mxGraphModel)

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" />
    <mxCell id="1" parent="0" />
    <mxCell id="40" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="IMemory" vertex="1"><mxGeometry x="50" y="60" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="41" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="Mmu" vertex="1"><mxGeometry x="220" y="60" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="42" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="PageTable" vertex="1"><mxGeometry x="390" y="60" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="43" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="PCB" vertex="1"><mxGeometry x="220" y="170" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="44" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="ProcessImage" vertex="1"><mxGeometry x="390" y="170" width="140" height="50" as="geometry" /></mxCell>
    <mxCell id="45" edge="1" parent="1" source="41" target="40" style="endArrow=classic;html=1;" value="uses"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="46" edge="1" parent="1" source="41" target="42" style="endArrow=classic;html=1;" value="uses"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="47" edge="1" parent="1" source="44" target="43" style="endArrow=diamond;html=1;" value="has"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Диаграмма 2 (State machine процесса, mxGraphModel)

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" />
    <mxCell id="1" parent="0" />
    <mxCell id="50" parent="1" style="ellipse;whiteSpace=wrap;html=1;" value="New" vertex="1"><mxGeometry x="60" y="100" width="90" height="50" as="geometry" /></mxCell>
    <mxCell id="51" parent="1" style="ellipse;whiteSpace=wrap;html=1;" value="Ready" vertex="1"><mxGeometry x="210" y="100" width="90" height="50" as="geometry" /></mxCell>
    <mxCell id="52" parent="1" style="ellipse;whiteSpace=wrap;html=1;" value="Running" vertex="1"><mxGeometry x="360" y="100" width="90" height="50" as="geometry" /></mxCell>
    <mxCell id="53" parent="1" style="ellipse;whiteSpace=wrap;html=1;" value="Blocked" vertex="1"><mxGeometry x="360" y="210" width="90" height="50" as="geometry" /></mxCell>
    <mxCell id="54" parent="1" style="ellipse;whiteSpace=wrap;html=1;" value="Terminated" vertex="1"><mxGeometry x="510" y="100" width="110" height="50" as="geometry" /></mxCell>
    <mxCell id="55" edge="1" parent="1" source="50" target="51" style="endArrow=classic;html=1;" value="admit"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="56" edge="1" parent="1" source="51" target="52" style="endArrow=classic;html=1;" value="dispatch"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="57" edge="1" parent="1" source="52" target="53" style="endArrow=classic;html=1;" value="wait I/O"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="58" edge="1" parent="1" source="53" target="51" style="endArrow=classic;html=1;" value="event"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="59" edge="1" parent="1" source="52" target="54" style="endArrow=classic;html=1;" value="exit"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

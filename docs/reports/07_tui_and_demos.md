# Отчет 7. TUI и демонстрационные сценарии

## Покрываемые стадии

- Стадия 13: Terminal UI
- Стадия 14: Demos + CLI

## Цель отчета

Показать внешний UI-слой (без зависимости ядра от UI), MVC-контракты, режимы playback/history и образовательные сценарии демо.

## Что обязательно описать

1. Почему `contur2_tui` отделен от `contur2_lib`.
2. `IKernelDiagnostics` и `IKernelReadModel` как read-only мост.
3. Контракты `TuiSnapshot`, `TuiCommand`, `TuiPlaybackConfig`.
4. Поведение `tick(n)`, autoplay, pause, seek.
5. Структуру CLI-демонстраций и `Stepper`.

## Код и артефакты (вставить фрагменты)

- `src/include/contur/tui/tui_models.h`
- `src/include/contur/tui/tui_commands.h`
- `src/include/contur/kernel/i_kernel_diagnostics.h`
- `src/contur/tui/kernel_read_model.cpp`
- `src/contur/tui/history_buffer.cpp`
- `src/contur/tui/tui_controller.cpp`
- `src/demos/include/demos/stepper.h`
- `src/app/main.cpp`

## Место под рисунки

- Рисунок 1. Wireframe dashboard (Process/Scheduler/Memory панели).
- Рисунок 2. Скриншот работы пошагового демо в Debug.

## Диаграмма 1 (MVC + boundary, mxGraphModel)

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="140" parent="1" style="rounded=1;whiteSpace=wrap;html=1;dashed=1;" value="contur2_lib (Kernel)" vertex="1"><mxGeometry x="40" y="50" width="250" height="220" as="geometry" /></mxCell>
    <mxCell id="141" parent="1" style="rounded=1;whiteSpace=wrap;html=1;dashed=1;" value="contur2_tui (External UI)" vertex="1"><mxGeometry x="340" y="50" width="300" height="220" as="geometry" /></mxCell>
    <mxCell id="142" parent="140" style="rounded=1;whiteSpace=wrap;html=1;" value="IKernel" vertex="1"><mxGeometry x="20" y="40" width="90" height="40" as="geometry" /></mxCell>
    <mxCell id="143" parent="140" style="rounded=1;whiteSpace=wrap;html=1;" value="IKernelDiagnostics" vertex="1"><mxGeometry x="130" y="40" width="100" height="40" as="geometry" /></mxCell>
    <mxCell id="144" parent="141" style="rounded=1;whiteSpace=wrap;html=1;" value="ReadModel" vertex="1"><mxGeometry x="20" y="40" width="90" height="40" as="geometry" /></mxCell>
    <mxCell id="145" parent="141" style="rounded=1;whiteSpace=wrap;html=1;" value="Controller" vertex="1"><mxGeometry x="120" y="40" width="90" height="40" as="geometry" /></mxCell>
    <mxCell id="146" parent="141" style="rounded=1;whiteSpace=wrap;html=1;" value="Renderer" vertex="1"><mxGeometry x="220" y="40" width="70" height="40" as="geometry" /></mxCell>
    <mxCell id="147" edge="1" parent="1" source="143" target="144" style="endArrow=classic;html=1;" value="snapshot()"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Диаграмма 2 (Playback state machine, mxGraphModel)

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="150" parent="1" style="ellipse;whiteSpace=wrap;html=1;" value="Idle" vertex="1"><mxGeometry x="90" y="120" width="90" height="50" as="geometry" /></mxCell>
    <mxCell id="151" parent="1" style="ellipse;whiteSpace=wrap;html=1;" value="Playing" vertex="1"><mxGeometry x="260" y="120" width="100" height="50" as="geometry" /></mxCell>
    <mxCell id="152" parent="1" style="ellipse;whiteSpace=wrap;html=1;" value="Paused" vertex="1"><mxGeometry x="450" y="120" width="100" height="50" as="geometry" /></mxCell>
    <mxCell id="153" edge="1" parent="1" source="150" target="151" style="endArrow=classic;html=1;" value="autoplay start"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="154" edge="1" parent="1" source="151" target="152" style="endArrow=classic;html=1;" value="pause"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="155" edge="1" parent="1" source="152" target="151" style="endArrow=classic;html=1;" value="resume"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="156" edge="1" parent="1" source="152" target="150" style="endArrow=classic;html=1;" value="stop"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

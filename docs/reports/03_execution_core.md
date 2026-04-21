# Отчет 3. Ядро исполнения

## Покрываемые стадии

- Стадия 4: CPU + I/O
- Стадия 5: Interpreter Engine

## Цель отчета

Показать вычислительный контур симулятора: ALU, CPU, устройства ввода-вывода и интерпретатор, исполняющий инструкции по тик-бюджету.

## Что обязательно описать

1. Fetch-decode-execute цикл.
2. Контракты `ICPU` и `IExecutionEngine`.
3. Как обрабатываются прерывания и ошибки (`DivisionByZero`, `Exit`).
4. Связь с устройствами через `IDevice`/`DeviceManager`.

## Код и артефакты (вставить фрагменты)

- `src/include/contur/cpu/alu.h`
- `src/contur/cpu/cpu.cpp`
- `src/include/contur/io/i_device.h`
- `src/contur/io/device_manager.cpp`
- `src/include/contur/execution/i_execution_engine.h`
- `src/contur/execution/interpreter_engine.cpp`

## Место под рисунки

- Рисунок 1. Поток исполнения одной инструкции.
- Рисунок 2. Связи CPU и I/O через DeviceManager.

## Диаграмма 1 (Sequence step(), mxGraphModel)

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="60" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="InterpreterEngine" vertex="1"><mxGeometry x="40" y="60" width="130" height="50" as="geometry" /></mxCell>
    <mxCell id="61" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="CPU" vertex="1"><mxGeometry x="230" y="60" width="100" height="50" as="geometry" /></mxCell>
    <mxCell id="62" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="IMemory" vertex="1"><mxGeometry x="380" y="60" width="110" height="50" as="geometry" /></mxCell>
    <mxCell id="63" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="ALU" vertex="1"><mxGeometry x="540" y="60" width="90" height="50" as="geometry" /></mxCell>
    <mxCell id="64" edge="1" parent="1" source="60" target="61" style="endArrow=classic;html=1;" value="step()"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="65" edge="1" parent="1" source="61" target="62" style="endArrow=classic;html=1;" value="fetch"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="66" edge="1" parent="1" source="61" target="63" style="endArrow=classic;html=1;" value="execute op"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Диаграмма 2 (Component diagram CPU/IO, mxGraphModel)

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="70" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="CPU" vertex="1"><mxGeometry x="70" y="100" width="100" height="50" as="geometry" /></mxCell>
    <mxCell id="71" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="DeviceManager" vertex="1"><mxGeometry x="250" y="100" width="140" height="50" as="geometry" /></mxCell>
    <mxCell id="72" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="ConsoleDevice" vertex="1"><mxGeometry x="450" y="60" width="130" height="50" as="geometry" /></mxCell>
    <mxCell id="73" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="NetworkDevice" vertex="1"><mxGeometry x="450" y="140" width="130" height="50" as="geometry" /></mxCell>
    <mxCell id="74" edge="1" parent="1" source="70" target="71" style="endArrow=classic;html=1;" value="I/O req"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="75" edge="1" parent="1" source="71" target="72" style="endArrow=classic;html=1;" value="dispatch"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="76" edge="1" parent="1" source="71" target="73" style="endArrow=classic;html=1;" value="dispatch"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

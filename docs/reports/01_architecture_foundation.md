# Отчет 1. Архитектурный фундамент

## Покрываемые стадии

- Стадия 0: Project Scaffolding
- Стадия 1: Foundation (`core/` + `arch/`)

## Цель отчета

Показать, как был собран минимально рабочий каркас системы: структура директорий, CMake-конфигурация, базовые типы, механизмы ошибок и архитектурные контракты.

## Что обязательно описать в тексте

1. Назначение проекта и образовательные цели.
2. Почему выбраны DIP, PIMPL, `Result<T>`, smart pointers.
3. Как устроена сборка (`CMakePresets.json`, `build.sh`, target-структура).
4. Что дают `core/types.h`, `core/error.h`, `core/clock.h`, `core/event.h`, `arch/*`.

## Код и артефакты (вставить фрагменты)

- `src/CMakeLists.txt`
- `src/CMakePresets.json`
- `src/build.sh`
- `src/include/contur/core/types.h`
- `src/include/contur/core/error.h`
- `src/include/contur/core/clock.h`
- `src/include/contur/core/event.h`
- `src/include/contur/arch/instruction.h`
- `src/include/contur/arch/register_file.h`

## Место под рисунки

- Рисунок 1. Общая структура репозитория (дерево модулей).
- Рисунок 2. Цепочка сборки: preset -> configure -> build -> run tests.

## Диаграмма 1 (C4 Container, шаблон mxGraphModel)

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" />
    <mxCell id="1" parent="0" />
    <mxCell id="10" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="contur2 (app)" vertex="1">
      <mxGeometry x="40" y="70" width="150" height="60" as="geometry" />
    </mxCell>
    <mxCell id="11" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="contur2_demos" vertex="1">
      <mxGeometry x="240" y="70" width="150" height="60" as="geometry" />
    </mxCell>
    <mxCell id="12" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="contur2_lib" vertex="1">
      <mxGeometry x="140" y="180" width="150" height="60" as="geometry" />
    </mxCell>
    <mxCell id="13" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#f8cecc;strokeColor=#b85450;" value="tests (GTest)" vertex="1">
      <mxGeometry x="430" y="180" width="150" height="60" as="geometry" />
    </mxCell>
    <mxCell id="20" edge="1" parent="1" source="10" target="12" style="endArrow=classic;html=1;" value="link" >
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
    <mxCell id="21" edge="1" parent="1" source="11" target="12" style="endArrow=classic;html=1;" value="link" >
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
    <mxCell id="22" edge="1" parent="1" source="13" target="12" style="endArrow=classic;html=1;" value="test" >
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
  </root>
</mxGraphModel>
```

## Диаграмма 2 (Пайплайн сборки, шаблон mxGraphModel)

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" />
    <mxCell id="1" parent="0" />
    <mxCell id="30" parent="1" style="shape=process;whiteSpace=wrap;html=1;" value="conan install" vertex="1">
      <mxGeometry x="40" y="80" width="120" height="50" as="geometry" />
    </mxCell>
    <mxCell id="31" parent="1" style="shape=process;whiteSpace=wrap;html=1;" value="cmake --preset" vertex="1">
      <mxGeometry x="200" y="80" width="140" height="50" as="geometry" />
    </mxCell>
    <mxCell id="32" parent="1" style="shape=process;whiteSpace=wrap;html=1;" value="cmake --build" vertex="1">
      <mxGeometry x="380" y="80" width="130" height="50" as="geometry" />
    </mxCell>
    <mxCell id="33" parent="1" style="shape=process;whiteSpace=wrap;html=1;" value="ctest --preset" vertex="1">
      <mxGeometry x="550" y="80" width="130" height="50" as="geometry" />
    </mxCell>
    <mxCell id="34" edge="1" parent="1" source="30" target="31" style="endArrow=classic;html=1;" value="" ><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="35" edge="1" parent="1" source="31" target="32" style="endArrow=classic;html=1;" value="" ><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="36" edge="1" parent="1" source="32" target="33" style="endArrow=classic;html=1;" value="" ><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Критерии готовности отчета

- Есть связка «решение -> код -> проверка».
- Приведены минимум 2 диаграммы и 2 рисунка.
- Зафиксированы выводы по стадиям 0-1.

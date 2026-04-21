# Отчет 8. Нативное исполнение, тестирование и CI

## Покрываемые стадии

- Стадия 15: Native Execution Engine
- Стадия 16: Full Test Suite
- Стадия 17: Documentation & CI

## Цель отчета

Зафиксировать завершение проекта на уровне production-ready практик: native execution, полнота тестов, документация и автоматический контроль качества в CI.

## Что обязательно описать

1. Архитектуру `NativeEngine` и platform-specific ветвление.
2. Матрицу тестов: unit, integration, stress, sanitizers, deterministic tests.
3. Покрытие и критерии качества.
4. CI pipeline и проверяемые условия merge-ready.

## Код и артефакты (вставить фрагменты)

- `src/include/contur/execution/native_engine.h`
- `src/contur/execution/native_engine.cpp`
- `src/tests/CMakeLists.txt`
- `src/CMakePresets.json`
- `.github/workflows/*` (если уже есть)
- `src/docs/doxygen/Doxyfile.in`

## Место под рисунки

- Рисунок 1. Сравнение native/interpret режимов по шагам запуска.
- Рисунок 2. Сводная таблица прохождения тестов и санитайзеров.

## Диаграмма 1 (NativeEngine flow, mxGraphModel)

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="160" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="Kernel/Dispatcher" vertex="1"><mxGeometry x="40" y="90" width="140" height="50" as="geometry" /></mxCell>
    <mxCell id="161" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="NativeEngine" vertex="1"><mxGeometry x="240" y="90" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="162" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="POSIX fork/exec or WinAPI" vertex="1"><mxGeometry x="420" y="90" width="180" height="50" as="geometry" /></mxCell>
    <mxCell id="163" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="Child Process" vertex="1"><mxGeometry x="640" y="90" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="164" edge="1" parent="1" source="160" target="161" style="endArrow=classic;html=1;" value="execute/halt"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="165" edge="1" parent="1" source="161" target="162" style="endArrow=classic;html=1;" value="spawn/control"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="166" edge="1" parent="1" source="162" target="163" style="endArrow=classic;html=1;" value="create"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Диаграмма 2 (CI pipeline, mxGraphModel)

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="170" parent="1" style="shape=process;whiteSpace=wrap;html=1;" value="Build (Clang/GCC)" vertex="1"><mxGeometry x="50" y="80" width="140" height="50" as="geometry" /></mxCell>
    <mxCell id="171" parent="1" style="shape=process;whiteSpace=wrap;html=1;" value="Unit Tests" vertex="1"><mxGeometry x="220" y="80" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="172" parent="1" style="shape=process;whiteSpace=wrap;html=1;" value="Integration Tests" vertex="1"><mxGeometry x="370" y="80" width="140" height="50" as="geometry" /></mxCell>
    <mxCell id="173" parent="1" style="shape=process;whiteSpace=wrap;html=1;" value="ASAN/UBSAN/TSAN" vertex="1"><mxGeometry x="540" y="80" width="150" height="50" as="geometry" /></mxCell>
    <mxCell id="174" parent="1" style="shape=process;whiteSpace=wrap;html=1;" value="Docs + Coverage" vertex="1"><mxGeometry x="720" y="80" width="140" height="50" as="geometry" /></mxCell>
    <mxCell id="175" edge="1" parent="1" source="170" target="171" style="endArrow=classic;html=1;" value="" ><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="176" edge="1" parent="1" source="171" target="172" style="endArrow=classic;html=1;" value="" ><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="177" edge="1" parent="1" source="172" target="173" style="endArrow=classic;html=1;" value="" ><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="178" edge="1" parent="1" source="173" target="174" style="endArrow=classic;html=1;" value="" ><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Чек-лист финализации отчета

- Есть таблица тестов с фактическим статусом и датой прогона.
- Есть минимум 2 примера падений/дефектов и как они были исправлены.
- Есть скрин CI и ссылка на pipeline run.

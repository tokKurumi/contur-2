# Отчет 8. Нативное исполнение, тестирование и CI

## Охват этапов

- Стадия 15: Native Execution Engine
- Стадия 16: Full Test Suite
- Стадия 17: Documentation & CI

## Контекст и цель

Заключительный этап Contur 2 фиксирует переход проекта к завершенной инженерной конфигурации. На этом уровне важно не только наличие функциональности, но и то, что она работает в двух режимах исполнения, покрыта тестами разных уровней и сопровождается воспроизводимой сборкой, документацией и автоматической проверкой в CI.

В данном отчете подводится итог следующим направлениям:

- нативное исполнение процессов через `NativeEngine` и платформенные backend-ветки;
- расширение тестовой матрицы до unit, integration, deterministic и sanitizer-прогонов;
- закрепление критериев качества, по которым можно судить о готовности к merge;
- настройка CI и публикации документации как обязательной части рабочего цикла.

Иллюстрация для вставки в Word:

- Рисунок 1. Сравнение native и interpret режимов по шагам запуска.
- Рисунок 2. Сводная таблица прохождения тестов и санитайзеров.

## Нативное исполнение: роль `NativeEngine`

Переход к нативному режиму нужен для того, чтобы Contur 2 мог запускать не только интерпретируемые учебные программы, но и реальные процессы хостовой операционной системы. В этой модели ядро по-прежнему остается координатором жизненного цикла, а `NativeEngine` выступает как исполнительный адаптер между абстрактным интерфейсом проекта и системными API платформы.

Архитектурно это означает следующее:

- ядро и диспетчер не знают о конкретных системных вызовах напрямую;
- платформа выбирается через отдельную ветку реализации, а не через условные конструкции в клиентском коде;
- запуск, остановка и контроль дочернего процесса инкапсулируются внутри execution layer;
- интерфейсный контракт остается единым, поэтому код верхнего уровня не различает interpret и native режимы.

На практике `NativeEngine` берет на себя создание и сопровождение внешнего процесса, а также завершение его работы по команде ядра. Для POSIX-платформ используется ветка с `fork/exec`, а для Windows предусмотрен собственный путь через WinAPI. Такой подход сохраняет переносимость проекта и позволяет не смешивать учебную модель исполнения с host-specific деталями.

### Диаграмма 1: поток `NativeEngine`

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

Смысл этой схемы в том, что инициатором работы остается kernel-side логика, а `NativeEngine` выполняет только ту часть, которая зависит от платформы. Благодаря этому interpret и native режимы сохраняют общую внешнюю модель, но различаются уровнем фактического исполнения.

## Матрица тестирования

На финальной стадии проекта тестирование строится не как единый набор, а как набор взаимодополняющих слоев. Это важно для Contur 2, потому что часть дефектов проявляется только в малых модульных сценариях, часть — при полном прохождении ядра, а часть — только под нагрузкой или под санитайзерами.

В `src/tests/CMakeLists.txt` тесты разведены по нескольким группам:

- unit-тесты ядра без зависимости от TUI;
- integration-тесты, проверяющие сквозные сценарии взаимодействия подсистем;
- отдельные TUI-специфичные тесты, если собран `contur2_tui`;
- deterministic сценарии, где важна повторяемость результата;
- sanitizer-прогоны, которые ловят ошибки памяти и конкурентного доступа на этапе CI.

Такое разделение позволяет не смешивать быстрые проверочные наборы и тяжелые конфигурации, а также делает сборку удобной как для локального запуска, так и для автоматизации.

### Что считается покрытым

- базовая логика подсистемы должна иметь unit-проверку;
- пользовательские и межмодульные сценарии должны быть подтверждены integration-тестами;
- повторяемые сценарии должны давать одинаковый результат на разных запусках;
- sanitizer-сборки должны проходить без диагностик;
- результаты тестов должны быть воспроизводимы в пресетах Debug, Release и TSAN-ветках.

### Диаграмма 2: матрица CI и тестов

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

Эта схема показывает, что контроль качества в проекте строится как последовательный конвейер: сначала сборка, затем юнит-проверки, после этого интеграционные сценарии, затем sanitizer-режимы и только потом документация и артефакты публикации.

## Сборочные пресеты и контроль качества

Сборочная матрица закреплена в `src/CMakePresets.json`. В ней разведены конфигурации Debug и Release, отдельные варианты для Clang и GCC, а также TSAN-ветки. Для Windows предусмотрены собственные пресеты, чтобы не смешивать кроссплатформенные условия в одной конфигурации.

Это дает два практических результата:

- локально можно быстро воспроизвести нужный режим без ручной настройки CMake;
- в CI можно явно зафиксировать, что именно считается проверяемой конфигурацией для merge-ready состояния.

Именно поэтому в отчете по финальной стадии важно не просто перечислить тесты, а показать, что они привязаны к реальным build/test presets и к одной воспроизводимой схеме запуска.

## Документация и CI

Финальное качество проекта определяется не только тестами, но и тем, насколько автоматически поддерживается документация. В текущей конфигурации это решается двумя workflow-файлами:

- `ci.yml` отвечает за сборку и тестирование по матрице компиляторов и sanitizer-режимов;
- `docs.yml` отвечает за генерацию Doxygen-документации и публикацию на Pages.

CI-пайплайн строится как последовательность обязательных стадий:

1. checkout и подготовка зависимостей;
2. установка Conan-пакетов и конфигурация проекта через preset;
3. сборка таргета;
4. запуск тестов с выводом ошибок при падении;
5. публикация документации как отдельного артефакта.

Тем самым merge-ready состояние определяется не субъективно, а по проверяемым условиям: проект должен собираться, тесты должны проходить, sanitizer-сборки должны быть чистыми, а документация должна генерироваться без ошибок.

## Критерии готовности к слиянию

Финальная проверка проекта должна опираться на конкретные критерии. Для Contur 2 это означает следующее:

- все обязательные тестовые наборы проходят в актуальной матрице сборки;
- нет необработанных ошибок в sanitizer-режимах;
- документация собирается в автоматическом workflow;
- артефакты, использованные для проверки, доступны и воспроизводимы;
- native и interpret режимы дают согласованное поведение в заявленных сценариях.

## Код и артефакты для вставки в отчет

При финальной подготовке текста в Word в этот раздел следует вставить:

- фрагмент из `src/include/contur/execution/native_engine.h`;
- фрагмент из `src/contur/execution/native_engine.cpp`;
- фрагмент из `src/tests/CMakeLists.txt`;
- фрагмент из `src/CMakePresets.json`;
- фрагмент из `.github/workflows/ci.yml`;
- фрагмент из `.github/workflows/docs.yml`;
- фрагмент из `src/docs/doxygen/Doxyfile.in`;

## Чек-лист финализации отчета

- Есть таблица тестов с фактическим статусом и датой прогона.
- Есть минимум 2 примера падений/дефектов и как они были исправлены.
- Есть скрин CI и ссылка на pipeline run.

## Итог

На завершающем этапе Contur 2 получает не только нативное исполнение процессов, но и полный контур промышленной проверки: от сборочных пресетов и тестовой матрицы до документации и CI-публикации. Это делает проект завершенным не в смысле набора функций, а в смысле инженерной воспроизводимости и проверяемости.

#import "@preview/smk-sto:0.3.1": *
#import "@preview/fletcher:0.5.8" as fletcher: diagram, edge, node
#import "_meta.typ": *

#show: lab-report.with(
  institute: institute,
  department: department,
  work-number: 8,
  discipline: discipline,
  title: "Нативный движок исполнения, тестовый пакет и непрерывная интеграция",
  author: author,
  supervisor: supervisor,
  designation: designation,
)

= Цель работы

Замкнуть исполнительный контур Contur 2 второй реализацией `IExecutionEngine` — нативным
движком, запускающим настоящий host-процесс под управлением диспетчера, а также покрыть
проект массовым тестовым пакетом и автоматизированным контролем качества: матрица CI,
ThreadSanitizer, отдельный workflow публикации API-документации.

Задачи работы:

+ реализовать `NativeEngine` с симметричными Windows- и POSIX-бекендами;
+ описать минимальные точки соприкосновения с ядром (`ProcessImage`, `ProcessConfig`);
+ зафиксировать карту тестового пакета (unit, integration, extended);
+ описать CMake-пресеты и матрицу CI;
+ описать публикацию Doxygen-документации на GitHub Pages.

= Реализация

== Нативный движок исполнения

`NativeEngine` реализует `IExecutionEngine` без новых интерфейсов и без новых директорий.
Точки соприкосновения с ядром минимальны:

- `ProcessImage` содержит опциональное поле `nativePath_` и геттер `nativePath()`;
- `ProcessConfig` в `IKernel` содержит поле `std::string nativePath`, прокидываемое в
  `ProcessImage` методом `Kernel::createProcess`;
- метод `KernelBuilder::withExecutionEngine(...)` принимает `NativeEngine` так же, как
  `InterpreterEngine` — это единственная точка переключения backend-а:

```cpp
KernelBuilder::withExecutionEngine(std::make_unique<NativeEngine>(*tracer))
```

Реализация раздвоена внутри одного `.cpp` через `#if defined(_WIN32)` /
`#elif defined(__unix__) || defined(__APPLE__)`. Сопоставление шагов жизненного цикла
host-процесса по двум платформам приведено в таблице @tab:native-lifecycle.

#figure(
  text(
    size: 10pt,
    table(
      columns: (auto, 1fr, 1fr),
      align: (left + top, left + top, left + top),
      inset: 6pt,
      [Событие], [Windows], [POSIX],

      [Первый `execute`],
      [`CreateProcessW` + `CREATE_SUSPENDED` + redirected stdout pipe],
      [`pipe()` + `fork()` → `dup2` / `execv`; родитель шлёт `SIGSTOP`],

      [Очередной `execute`],
      [`ResumeThread` → `WaitForSingleObject(sliceMs)` → `SuspendThread`],
      [`kill(SIGCONT)` → `nanosleep(sliceMs)` → `kill(SIGSTOP)`],

      [`halt(pid)`], [`TerminateProcess` + `CloseHandle`], [`kill(SIGKILL)` + блокирующий `waitpid`],

      [Естественный выход],
      [`WAIT_OBJECT_0` + `GetExitCodeProcess` → `R0`],
      [`waitpid(WNOHANG)` + `WIFEXITED` / `WIFSIGNALED` → `R0`],

      [stdout доступен], [`PeekNamedPipe` + `ReadFile`], [non-blocking `read()` через `O_NONBLOCK`],
    ),
  ),
  caption: [Жизненный цикл host-процесса под управлением `NativeEngine`],
) <tab:native-lifecycle>

stdout дочернего процесса дренируется в трассировщик через события
`spawn/resume/suspend/exit/stdout`. Контур управления нативным процессом изображён на
рисунке @fig:native-engine.

#figure(
  scale(
    80%,
    reflow: true,
    diagram(
      spacing: (2.2cm, 1.5cm),
      node-inset: 7pt,
      node((0, 0), [Kernel / Dispatcher], fill: rgb("#dae8fc")),
      node((1, 0), [NativeEngine], fill: rgb("#d5e8d4")),
      node((3, 0), [Host child process], fill: rgb("#fff2cc")),
      node((0, 1), [ITracer], fill: rgb("#f8cecc")),
      node((1, 1), [Win32 backend], fill: rgb("#ffe6cc")),
      node((2, 1), [POSIX backend], fill: rgb("#ffe6cc")),
      edge((0, 0), (1, 0), "->", [execute / halt], label-side: left, label-sep: 0.8em),
      edge((1, 0), (3, 0), "->", [spawn / resume / suspend], label-side: left, label-sep: 0.8em),
      edge((1, 0), (0, 1), "-->", [trace events], label-side: right, label-sep: 0.5em),
      edge((1, 0), (1, 1), "-->", [#raw("#if _WIN32")], label-side: right, label-sep: 0.4em),
      edge((1, 0), (2, 1), "-->", [#raw("#elif __unix__")], label-side: left, label-sep: 0.4em),
    ),
  ),
  caption: [Жизненный цикл нативного процесса под управлением ядра],
) <fig:native-engine>

== Тестовый пакет

Полный тестовый пакет содержит 862 теста (агрегированный счётчик `TEST` + `TEST_F` по всем
исходникам `src/tests/**`). Распределение по этапам приведено в таблице @tab:tests.

#figure(
  table(
    columns: 2,
    align: (left, right),
    [Этап], [Тестов],
    [Foundation (`core` + `arch`)], [43],
    [Подсистема памяти], [78],
    [Модель процесса], [99],
    [CPU + I/O], [94],
    [Движок интерпретации], [26],
    [Планирование (7 политик + статистика + scheduler)], [23],
    [Диспетчеризация и синхронизация], [64],
    [IPC и системные вызовы], [33],
    [Файловая система], [14],
    [Фасад ядра и `KernelBuilder`], [20],
    [Многопоточный рантайм], [39],
    [Трассировка], [6],
    [TUI и FTXUI-рендерер], [69],
    [Native engine + native kernel flow], [29],
    [Расширенные / интеграционные сьюты], [225],
    [*Итого*], [*862*],
  ),
  caption: [Распределение тестов по подсистемам],
) <tab:tests>

Тесты разделены по уровням: unit (`src/tests/unit/test_*.cpp`) — модульные проверки
контрактов и реализаций; integration (`src/tests/integration/test_*.cpp`) — сквозные
сценарии через `IKernel`; extended (`src/tests/unit/test_*_extended.cpp`) — целевые
подсьюты с большой плотностью кейсов.

== Пресеты CMake и матрица CI

Файл `src/CMakePresets.json` определяет три семейства пресетов:

- локальные сборки с ASAN/UBSAN: `debug`, `gcc-debug`, `release`, `gcc-release`,
  `win-debug`, `win-release` (debug-варианты для POSIX подключают
  `-fsanitize=address,undefined`);
- ThreadSanitizer: `tsan-debug`, `tsan-release`, `tsan-gcc-debug`, `tsan-gcc-release`,
  `tsan-win-debug`, `tsan-win-release`;
- общая опция `CONTUR2_ENABLE_TRACING`, автоматически включающаяся в Debug-сборках.

Матрица в `.github/workflows/ci.yml` покрывает четыре конфигурации Release-уровня:

+ Clang × Release;
+ GCC × Release;
+ Clang × TSAN-Release;
+ GCC × TSAN-Release.

Каждый job устанавливает Ninja и Conan, конфигурирует выбранный пресет, собирает проект
и прогоняет `ctest --preset ... --output-on-failure`. Последовательность шагов pipeline
показана на рисунке @fig:ci.

#figure(
  diagram(
    spacing: (1.6cm, 0.9cm),
    node-inset: 7pt,
    node((0, 0), [Trigger: push / PR], fill: rgb("#dae8fc")),
    node((0, 1), [Install ninja, llvm/gcc, conan], fill: rgb("#d5e8d4")),
    node((0, 2), [conan install src/tests], fill: rgb("#ffe6cc")),
    node((0, 3), [cmake --preset ...], fill: rgb("#fff2cc")),
    node((0, 4), [cmake --build --preset ...], fill: rgb("#e1d5e7")),
    node((0, 5), [ctest --preset ...], fill: rgb("#f8cecc")),
    edge((0, 0), (0, 1), "->"),
    edge((0, 1), (0, 2), "->"),
    edge((0, 2), (0, 3), "->"),
    edge((0, 3), (0, 4), "->"),
    edge((0, 4), (0, 5), "->"),
  ),
  caption: [Шаги pipeline непрерывной интеграции],
) <fig:ci>

== Документация и GitHub Pages

В `src/CMakeLists.txt` через `add_custom_target` определена цель `docs`, которая
запускает Doxygen с темой `doxygen-awesome-css`. Отдельный workflow
`.github/workflows/docs.yml` устанавливает Doxygen 1.16.1, конфигурирует release-сборку
с `-DCONTUR2_BUILD_TUI=OFF -DCONTUR2_BUILD_APP=OFF -DCONTUR2_BUILD_TESTS=OFF`, собирает
цель `docs` и публикует артефакт через `actions/upload-pages-artifact` +
`actions/deploy-pages`.

= Заключение <s>

Исполнительный контур симулятора замкнут: ядро одинаково честно работает и со своим
байт-кодом, и с настоящим host-процессом. Тесты покрывают unit-сценарии, сквозные сценарии
через `IKernel` и многопоточные/детерминированные режимы; CI прогоняет Release-сборки на
Clang и GCC, в том числе под ThreadSanitizer. Doxygen-документация автоматически
публикуется на GitHub Pages, что делает API-доку доступной без локальной сборки.

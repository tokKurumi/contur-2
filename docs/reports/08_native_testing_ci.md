# Отчет 8. Нативное исполнение, тестирование и CI

## Охват этапов

- Этап 15: Native Execution Engine
- Этап 16: Full Test Suite (unit + integration + extended + sanitizers)
- Этап 17: Documentation & CI

## Контекст и цель

Этот блок этапов закрывает три направления: альтернативный движок исполнения
(`NativeEngine`, который запускает настоящий host-процесс), массовый тестовый пакет
(unit + integration + extended + sanitizer-варианты) и автоматизированный контроль качества
(CI на двух компиляторах, ThreadSanitizer, отдельный workflow для публикации Doxygen-сайта).

Цели отчёта:

- описать архитектуру `NativeEngine` и host-зависимое ветвление (Windows + POSIX);
- зафиксировать карту тестового пакета (unit / integration / extended) с реальными счётчиками;
- описать матрицу CMake-preset-ов и реальную матрицу CI;
- описать публикацию Doxygen-сайта через отдельный workflow на GitHub Pages.

## NativeEngine: вторая стратегия исполнения

`NativeEngine` реализует `IExecutionEngine` точно так же, как `InterpreterEngine` — без новых
интерфейсов и без новых директорий. Точки соприкосновения с ядром минимальны:

- `ProcessImage` содержит опциональное поле `nativePath_` и геттер `nativePath()`;
- `ProcessConfig` в `IKernel` содержит поле `std::string nativePath`, которое
    `Kernel::createProcess` прокидывает в `ProcessImage`;
- `KernelBuilder::withExecutionEngine(std::make_unique<NativeEngine>(*tracer))` — единственная
    точка переключения backend-а.

Реализация раздвоена внутри одного `.cpp` через `#if defined(_WIN32) / #elif defined(__unix__) || defined(__APPLE__)`:

| Шаг | Windows | POSIX |
|---|---|---|
| Первый `execute(...)` | `CreateProcessW` с `CREATE_SUSPENDED` + redirected stdout pipe | `pipe()` + `fork()` → `dup2`/`execv`; родитель шлёт `SIGSTOP` |
| Очередной `execute(...)` | `ResumeThread` → `WaitForSingleObject(sliceMs)` → `SuspendThread` | `kill(SIGCONT)` → `nanosleep(sliceMs)` → `kill(SIGSTOP)` |
| `halt(pid)` | `TerminateProcess` + `CloseHandle` | `kill(SIGKILL)` + блокирующий `waitpid` |
| Естественный выход | `WAIT_OBJECT_0` + `GetExitCodeProcess` → `R0` | `waitpid(WNOHANG)` + `WIFEXITED/WIFSIGNALED` → `R0` |
| stdout доступен | `PeekNamedPipe` + `ReadFile` | non-blocking `read()` через `O_NONBLOCK` |

stdout ребёнка дренируется в `ITracer` через события `spawn/resume/suspend/exit/stdout`.

### Схема: жизненный цикл нативного процесса под управлением ядра

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="160" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="Kernel / Dispatcher" vertex="1"><mxGeometry x="40" y="100" width="160" height="50" as="geometry" /></mxCell>
    <mxCell id="161" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="NativeEngine" vertex="1"><mxGeometry x="240" y="100" width="140" height="50" as="geometry" /></mxCell>
    <mxCell id="162" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="Win32 backend (CreateProcessW + Resume/SuspendThread)" vertex="1"><mxGeometry x="420" y="40" width="360" height="50" as="geometry" /></mxCell>
    <mxCell id="163" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="POSIX backend (fork/execv + SIGSTOP/SIGCONT/waitpid)" vertex="1"><mxGeometry x="420" y="100" width="360" height="50" as="geometry" /></mxCell>
    <mxCell id="164" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#fff2cc;strokeColor=#d6b656;" value="Host child process (x86 binary)" vertex="1"><mxGeometry x="500" y="180" width="240" height="50" as="geometry" /></mxCell>
    <mxCell id="165" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#f8cecc;strokeColor=#b85450;" value="ITracer (spawn/resume/suspend/exit/stdout)" vertex="1"><mxGeometry x="40" y="180" width="320" height="50" as="geometry" /></mxCell>
    <mxCell id="170" edge="1" parent="1" source="160" target="161" style="endArrow=classic;html=1;" value="execute(...)/halt(pid)"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="171" edge="1" parent="1" source="161" target="162" style="endArrow=classic;html=1;dashed=1;" value="#if _WIN32"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="172" edge="1" parent="1" source="161" target="163" style="endArrow=classic;html=1;dashed=1;" value="#elif __unix__ / __APPLE__"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="173" edge="1" parent="1" source="162" target="164" style="endArrow=classic;html=1;" value="spawn/resume/suspend"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="174" edge="1" parent="1" source="163" target="164" style="endArrow=classic;html=1;" value="spawn/resume/suspend"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="175" edge="1" parent="1" source="161" target="165" style="endArrow=classic;html=1;" value="trace events"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Тестовый пакет: реальные счётчики

Полная подсистема тестов в Contur 2 — 862 теста (агрегированный счёт `TEST` + `TEST_F` по
исходникам в `src/tests/**`). Деление по этапам:

| Этап | Тесты |
|---|---:|
| 1. Foundation (`core` + `arch`) | 43 |
| 2. Memory subsystem | 78 |
| 3. Process model | 99 |
| 4. CPU + I/O | 94 |
| 5. Interpreter engine | 26 |
| 6. Scheduling (7 политик + статистика + scheduler) | 23 |
| 7. Dispatch + sync | 64 |
| 8. IPC + syscalls | 33 |
| 9. File system | 14 |
| 10. Kernel facade + builder | 20 |
| 11. Host MT runtime (`threading_config`, `dispatcher_pool`, `*concurrent`, deterministic) | 39 |
| 12. Tracing (sink + tracer) | 6 |
| **Подытог 0–12** | **539** |
| 13. TUI (DTO/контроллер/история/FTXUI renderer + 2 integration-сьюта) | 69 |
| 15. Native engine + native kernel flow | 29 |
| 16. Расширенные / интеграционные сьюты (extended + end-to-end + scheduling integration) | 225 |
| **Итого** | **862** |

Полная пофайловая раскладка приведена в `.github/IMPLEMENTATION_PLAN.md`, разделы
«Test Statistics».

### Карта тестовых классов

- **unit** — `src/tests/unit/test_*.cpp` — модульные проверки контрактов и реализаций;
- **integration** — `src/tests/integration/test_*.cpp` — сквозные сценарии через `IKernel`
    (`kernel_end_to_end`, `kernel_api`, `tui_tick_navigation`, `tui_ftxui_integration`,
    `native_kernel_flow`, `process_scheduling_integration`, `deterministic_multithread`);
- **extended** — `src/tests/unit/test_*_extended.cpp` — целевые подсьюты с тяжёлой плотностью
    кейсов (например, 47 кейсов в `test_scheduling_policies_extended.cpp`).

## CMake preset-ы и матрица CI

`src/CMakePresets.json` определяет три семейства preset-ов:

- локальные сборки с ASAN/UBSAN: `debug`, `gcc-debug`, `release`, `gcc-release`,
    `win-debug`, `win-release` (debug-варианты для POSIX/Linux подключают
    `-fsanitize=address,undefined`);
- ThreadSanitizer: `tsan-debug`, `tsan-release`, `tsan-gcc-debug`, `tsan-gcc-release`,
    `tsan-win-debug`, `tsan-win-release`;
- общая опция `CONTUR2_ENABLE_TRACING`, автоматически включающаяся в Debug-сборках.

Матрица в `.github/workflows/ci.yml` покрывает четыре конфигурации Release-уровня:

- Clang × Release;
- GCC × Release;
- Clang × TSAN-Release;
- GCC × TSAN-Release.

Каждый job устанавливает Ninja и Conan, конфигурирует выбранный preset, собирает и прогоняет
`ctest --preset ... --output-on-failure`.

### Схема: фактический CI pipeline

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="180" parent="1" style="shape=process;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="Trigger: push / PR (paths: src/, .clang-*, ci.yml)" vertex="1"><mxGeometry x="40" y="40" width="320" height="50" as="geometry" /></mxCell>
    <mxCell id="181" parent="1" style="shape=process;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="apt install ninja-build, llvm/gcc; pip install conan" vertex="1"><mxGeometry x="40" y="110" width="320" height="50" as="geometry" /></mxCell>
    <mxCell id="182" parent="1" style="shape=process;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="conan install src/tests --build=missing" vertex="1"><mxGeometry x="40" y="180" width="320" height="50" as="geometry" /></mxCell>
    <mxCell id="183" parent="1" style="shape=process;whiteSpace=wrap;html=1;fillColor=#fff2cc;strokeColor=#d6b656;" value="cmake --preset {clang|gcc}-{release|tsan-release}" vertex="1"><mxGeometry x="40" y="250" width="380" height="50" as="geometry" /></mxCell>
    <mxCell id="184" parent="1" style="shape=process;whiteSpace=wrap;html=1;fillColor=#e1d5e7;strokeColor=#9673a6;" value="cmake --build --preset ..." vertex="1"><mxGeometry x="40" y="320" width="320" height="50" as="geometry" /></mxCell>
    <mxCell id="185" parent="1" style="shape=process;whiteSpace=wrap;html=1;fillColor=#f8cecc;strokeColor=#b85450;" value="ctest --preset ... --output-on-failure" vertex="1"><mxGeometry x="40" y="390" width="320" height="50" as="geometry" /></mxCell>
    <mxCell id="190" edge="1" parent="1" source="180" target="181" style="endArrow=classic;html=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="191" edge="1" parent="1" source="181" target="182" style="endArrow=classic;html=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="192" edge="1" parent="1" source="182" target="183" style="endArrow=classic;html=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="193" edge="1" parent="1" source="183" target="184" style="endArrow=classic;html=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="194" edge="1" parent="1" source="184" target="185" style="endArrow=classic;html=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Doxygen и GitHub Pages

`src/CMakeLists.txt` создаёт target `docs` (через `add_custom_target` + `DOXYGEN_EXECUTABLE`),
который генерирует HTML-доки с темой `doxygen-awesome-css`. Параллельный workflow
`.github/workflows/docs.yml` ставит Doxygen 1.16.1, конфигурирует release-сборку с
`-DCONTUR2_BUILD_TUI=OFF -DCONTUR2_BUILD_APP=OFF -DCONTUR2_BUILD_TESTS=OFF`, собирает target
`docs` и публикует артефакт через `actions/upload-pages-artifact` + `actions/deploy-pages`.

Документация публикуется на <https://contur.yudashkin-dev.ru/> (это рабочая страница,
ссылку приводит и `README.md`, и сам workflow в финальном шаге).

## Источники кода, использованные в отчёте

- `src/include/contur/execution/native_engine.h`
- `src/contur/execution/native_engine.cpp`
- `src/include/contur/kernel/i_kernel.h` (поле `ProcessConfig::nativePath`)
- `src/include/contur/process/process_image.h`
- `src/contur/process/process_image.cpp`
- `src/tests/unit/test_native_engine.cpp`
- `src/tests/integration/test_native_kernel_flow.cpp`
- `src/CMakePresets.json`
- `.github/workflows/ci.yml`
- `.github/workflows/docs.yml`
- `src/docs/doxygen/Doxyfile.in`
- `src/CMakeLists.txt` (target `docs`, флаг `CONTUR2_ENABLE_TRACING`)
- `.github/IMPLEMENTATION_PLAN.md` (детальный тест-инвентарь)

## Критерии готовности

- Описана архитектура `NativeEngine` и его симметричные Win32/POSIX-пути.
- Приведены реальные счётчики тестов (всего 862) и карта unit/integration/extended.
- Описаны все preset-ы CMake и матрица CI.
- Описан Doxygen-target и отдельный workflow публикации на GitHub Pages.

## Краткие выводы

Контур исполнения замкнут: ядро одинаково честно работает и со своим байт-кодом, и с
настоящим host-процессом. Тесты покрывают unit-сценарии, сквозные сценарии через `IKernel` и
многопоточные/детерминированные режимы; CI прогоняет Release-сборки на Clang и GCC, в том
числе под ThreadSanitizer. Doxygen-документация автоматически публикуется на GitHub Pages,
что делает отчёты, заголовки и архитектурные правила доступными без локальной сборки.

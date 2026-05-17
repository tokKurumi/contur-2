# Отчет 7. TUI и демонстрационные сценарии

## Покрываемые стадии

- Стадия 13: Terminal UI
- Стадия 14: Demos + CLI

## Контекст и цель

После реализации ядра, сервисов, синхронизации и трассировки проект получает внешний слой взаимодействия с пользователем. Этот слой не должен протекать внутрь ядра: UI обязан только читать состояние системы и отправлять команды управления, не нарушая архитектурных границ.

В Contur 2 этот принцип реализован через разделение на две сборки:

- `contur2_lib` содержит ядро и все runtime-сервисы;
- `contur2_tui` содержит внешний terminal UI, controller, read-model и рендеринг.

Такое разделение важно по двум причинам. Во-первых, ядро не зависит от FTXUI и вообще от UI-фреймворка. Во-вторых, TUI можно заменять, тестировать и расширять без пересборки логики ОС.

Эта стадия также включает демонстрационные сценарии. Они нужны не как «примеры ради примеров», а как образовательный слой: через них удобно показывать пошаговое исполнение, историю состояний, autoplay и поведение различных подсистем ядра.

Иллюстрация для вставки в Word:

- Рисунок: wireframe dashboard с панелями Process, Scheduler и Memory.

## Что обязательно описать

1. Почему `contur2_tui` отделен от `contur2_lib`.
2. `IKernelDiagnostics` и `IKernelReadModel` как read-only мост.
3. Контракты `TuiSnapshot`, `TuiCommand`, `TuiPlaybackConfig`.
4. Поведение `tick(n)`, autoplay, pause, seek.
5. Структуру CLI-демонстраций и пошагового сценария в `main.cpp`.

## Архитектура внешнего UI

TUI в проекте построен как внешний consumer of kernel state. Он не вызывает внутренние поля ядра напрямую и не меняет состояние ОС в обход контроллера. Вместо этого UI получает immutable snapshot и отправляет команды через контроллер.

### MVC и граница между слоями

Диаграмма отражает базовый принцип: kernel lives in `contur2_lib`, а внешний UI строится поверх read-only boundary.

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

## Read-only bridge

### IKernelDiagnostics

`IKernelDiagnostics` — это намеренно read-only контракт, который открывает только диагностический доступ к состоянию kernel. Он нужен, чтобы внешний UI мог работать с состоянием ОС, не получая права менять его.

```cpp
class IKernelDiagnostics
{
    public:
    virtual ~IKernelDiagnostics() = default;

    [[nodiscard]] virtual Result<KernelDiagnosticsSnapshot> captureSnapshot() const = 0;
};
```

Сама диагностическая структура хранит runtime-agnostic snapshot ядра:

```cpp
struct KernelDiagnosticsSnapshot
{
    KernelSnapshot kernel;
};
```

### IKernelReadModel

`IKernelReadModel` — это адаптер между kernel diagnostics и UI model. Он преобразует kernel snapshot в удобную для TUI immutable DTO-модель.

```cpp
class IKernelReadModel
{
    public:
    virtual ~IKernelReadModel() = default;
    [[nodiscard]] virtual Result<TuiSnapshot> captureSnapshot() const = 0;
};
```

В `KernelReadModel` происходит явное преобразование полей: `KernelSnapshot` переводится в `TuiSnapshot`, а затем в него копируются scheduler- и memory-срезы, а также список процессов.

Такой слой полезен тем, что TUI не знает о внутренних kernel-классах. Он видит только DTO, а значит может быть протестирован без полного стека запуска ОС.

## TUI модели и команды

### TuiSnapshot

`TuiSnapshot` — это центральный immutable объект, которым оперируют view и controller.

```cpp
struct TuiSnapshot
{
    Tick currentTick = 0;
    std::size_t processCount = 0;
    std::vector<TuiProcessSnapshot> processes;
    TuiSchedulerSnapshot scheduler;
    TuiMemorySnapshot memory;
    std::uint64_t sequence = 0;
    std::uint64_t epoch = 0;
};
```

Он объединяет три большие области данных:

- список процессов и их приоритетов;
- scheduler state, включая ready/blocked/running и per-lane queues;
- memory state, включая virtual slots и frame ownership.

### TuiCommand и TuiPlaybackConfig

Управление UI сведено к небольшой группе команд:

```cpp
enum class TuiCommandKind : std::uint8_t
{
    Tick,
    AutoPlayStart,
    AutoPlayStop,
    Pause,
    SeekBackward,
    SeekForward,
};
```

```cpp
struct TuiPlaybackConfig
{
    std::uint32_t intervalMs = 100;
    std::size_t step = 1;
};
```

```cpp
struct TuiCommand
{
    TuiCommandKind kind = TuiCommandKind::Tick;
    std::size_t step = 1;
    std::uint32_t intervalMs = 100;
};
```

Валидация помогает не пропустить некорректные значения в controller:

```cpp
Result<void> validatePlaybackConfig(const TuiPlaybackConfig &config);
Result<void> validateCommand(const TuiCommand &command);
```

### Семантика команд

- `Tick` продвигает kernel на `n` шагов.
- `AutoPlayStart` переводит controller в playing state и запускает интервализированный autoplay.
- `AutoPlayStop` останавливает autoplay и возвращает controller в idle.
- `Pause` переводит воспроизведение в paused state.
- `SeekBackward` и `SeekForward` двигают cursor по retained history.

Иллюстрация для вставки в Word:

- Рисунок: скриншот TUI dashboard с процессами, scheduler и memory-панелями.

## Playback и history

### HistoryBuffer

История в UI хранится отдельно от kernel. Это bounded ring-buffer, который сохраняет immutable snapshots и позволяет перематывать состояние вперед и назад.

```cpp
class HistoryBuffer final
{
    public:
    explicit HistoryBuffer(std::size_t capacity);

    [[nodiscard]] Result<void> append(TuiHistoryEntry entry);
    [[nodiscard]] Result<void> seekBackward(std::size_t step);
    [[nodiscard]] Result<void> seekForward(std::size_t step);
    void moveToLatest() noexcept;

    [[nodiscard]] std::optional<std::reference_wrapper<const TuiHistoryEntry>> current() const noexcept;
    [[nodiscard]] std::optional<std::reference_wrapper<const TuiHistoryEntry>> latest() const noexcept;
};
```

По реализации видно, что buffer всегда держит cursor на актуальном entry, а при переполнении удаляет oldest entry. Это делает его предсказуемым для playback и при этом ограничивает потребление памяти.

### TuiController

`TuiController` — это orchestration layer, который связывает read-model, history и callback для продвижения kernel time.

```cpp
class ITuiController
{
    public:
    virtual ~ITuiController() = default;
    [[nodiscard]] virtual Result<void> dispatch(const TuiCommand &command) = 0;
    [[nodiscard]] virtual Result<void> advanceAutoplay(std::uint32_t elapsedMs) = 0;
    [[nodiscard]] virtual const TuiSnapshot &current() const noexcept = 0;
    [[nodiscard]] virtual TuiControllerState state() const noexcept = 0;
    [[nodiscard]] virtual std::size_t historySize() const noexcept = 0;
    [[nodiscard]] virtual std::size_t historyCursor() const noexcept = 0;
};
```

Внутри controller хранится текущее состояние, playback config, accumulated time и buffer истории. При каждом тик-цикле controller:

1. вызывает callback `tick(step)`;
2. берет новый snapshot из read-model;
3. добавляет snapshot в history;
4. синхронизирует `current` с текущим cursor.

### State machine playback

Состояния playback простые, но достаточные:

- `Idle` — autoplay не активен;
- `Playing` — controller накапливает wall-clock и запускает tick шаги по интервалу;
- `Paused` — autoplay приостановлен, но history и current snapshot сохраняются.

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

### Поведение tick(n), autoplay, pause, seek

`tick(n)` в этой архитектуре означает не один машинный такт, а вызов callback, который продвигает kernel на `n` логических шагов. В `main.cpp` это сделано через `kernel->runForTicks(step)`.

Autoplay работает как накопитель времени: controller хранит `accumulatedMs`, и когда оно достигает `intervalMs`, он вызывает `tick(step)` и записывает новый snapshot.

Seek работает по retained history, а не по обратному исполнению kernel. Это важный дизайн-выбор: UI не пытается откатить состояние ядра, а лишь переключается между уже сохраненными снимками.

## CLI-демонстрации

Деморежим в проекте не оформлен отдельным `Stepper`-файлом. На практике его роль выполняет entrypoint `src/app/main.cpp`, где собирается demo kernel, создаются демонстрационные процессы и запускается TUI.

### Сборка demo kernel

В `main.cpp` создается `DemoKernelBuild`, затем через `KernelBuilder` wiring-ятся clock, memory, mmu, virtual memory, cpu, execution engine, scheduler, dispatcher, tracer, filesystem, IPC manager и syscall table.

Дальше в `spawnDemoProcesses(...)` создается набор учебных процессов с разными приоритетами и программами:

- краткий арифметический процесс;
- цикл с counter-like поведением;
- CPU-heavy worker;
- долгие фоновые задачи.

### Пошаговый сценарий

После построения kernel приложение делает три вещи:

1. Создает `KernelDiagnostics` и `KernelReadModel`.
2. Подключает `TuiController`, который вызывает `kernel->runForTicks(step)`.
3. Запускает `FtxuiApp` с конфигурацией autoplay и лог-провайдером.

В entrypoint видно, что UI запускается уже на полностью готовом kernel, а после завершения app печатается trace dump:

```cpp
KernelDiagnostics diagnostics(*kernel);
KernelReadModel readModel(diagnostics);

TuiController controller(readModel, [&kernel](std::size_t step) { return kernel->runForTicks(step); }, 512);

FtxuiApp app(...);
app.run();

std::cout << renderKernelTraceDump(*build.traceSink);
```

Это и есть практический CLI/демо-слой: он поднимает учебный сценарий, позволяет крутить execution пошагово и сохраняет подробный trace output после завершения.

Иллюстрация для вставки в Word:

- Рисунок: скриншот пошагового демо в Debug.

## Проверка и воспроизводимость

Для подтверждения работы TUI и demo-слоя используется стандартный сценарий сборки и тестирования:

```bash
bash src/build.sh debug src
ctest --preset debug --output-on-failure
```

Проверяются следующие свойства:

- корректность read-only мостов `IKernelDiagnostics` и `IKernelReadModel`;
- сохранение immutable snapshot history при `tick`, `seek` и autoplay;
- переходы `Idle -> Playing -> Paused` и обратно;
- корректное поведение `advanceAutoplay(...)` при накоплении wall-clock;
- согласованность `current()` и cursor history;
- работа demo entrypoint и вывод trace dump после завершения приложения.

## Источники кода, использованные в отчете

- `src/include/contur/tui/tui_models.h`
- `src/include/contur/tui/tui_commands.h`
- `src/include/contur/kernel/i_kernel_diagnostics.h`
- `src/include/contur/tui/i_kernel_read_model.h`
- `src/contur/tui/kernel_read_model.cpp`
- `src/include/contur/tui/history_buffer.h`
- `src/contur/tui/history_buffer.cpp`
- `src/include/contur/tui/i_tui_controller.h`
- `src/contur/tui/tui_controller.cpp`
- `src/app/main.cpp`

## Критерии готовности

- В тексте объяснено разделение `contur2_tui` и `contur2_lib`.
- Показан read-only bridge от kernel diagnostics к TUI snapshot.
- Описаны `TuiSnapshot`, `TuiCommand` и `TuiPlaybackConfig`.
- Раскрыто поведение `tick(n)`, autoplay, pause и seek.
- Показана структура демо-старта через `main.cpp` и пошаговый сценарий.
- Диаграммы встроены в соответствующие смысловые разделы.

## Краткие выводы

Стадии 13-14 завершают проект внешним пользовательским слоем, не нарушая архитектурную границу между UI и kernel. TUI работает только через read-only диагностику и immutable snapshots, а значит не вмешивается в состояние ОС напрямую.

Вместе с history и autoplay это дает удобный образовательный интерфейс: можно пошагово исследовать работу ядра, перематывать историю, включать автоматическое воспроизведение и строить демонстрационные сценарии без изменения ядра или добавления специальных test hooks.

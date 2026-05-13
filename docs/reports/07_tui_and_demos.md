# Отчет 7. TUI и приложение визуализации ядра

## Охват этапов

- Этап 13: Terminal UI (TUI / MVC)
- Этап 14: App Shell — FTXUI-приложение в `src/app/main.cpp`

## Контекст и цель

UI-слой Contur 2 устроен как внешний модуль (`contur2_tui`), полностью изолированный от
ядра (`contur2_lib`). Ядро не знает о TUI; TUI получает данные только через read-only снимок
`KernelSnapshot` → `KernelDiagnostics` → `KernelReadModel` → `TuiSnapshot`. История
воспроизведения принадлежит контроллеру и **не** откатывает состояние ядра.

В качестве backend-а рендеринга используется **FTXUI**: `FtxuiRenderer` отвечает за
визуализацию, а интерактивная оболочка `FtxuiApp` — за клавиатурный ввод и autoplay; обе
подключаются из `src/app/main.cpp`.

Цели отчёта:

- зафиксировать DTO-контракты модели (`TuiProcessSnapshot`, `TuiSchedulerSnapshot`,
    `TuiMemorySnapshot`, `TuiSnapshot`, `TuiHistoryEntry`);
- описать команды (`TuiCommand`, `TuiCommandKind`, `TuiPlaybackConfig`) и стейт-машину
    контроллера (`Idle/Playing/Paused`);
- показать роли `IKernelDiagnostics`, `IKernelReadModel`, `HistoryBuffer`;
- зафиксировать backend-агностичный `IRenderer` + реальный backend `FtxuiRenderer` + оболочку
    `FtxuiApp`;
- показать клавиатурный контракт `FtxuiApp` и архитектурную инварианту
    «UI никогда не линкуется в `contur2_lib`».

## DTO-контракты модели

```cpp
struct TuiProcessSnapshot
{
  ProcessId id; std::string name;
  ProcessState state;
  PriorityLevel basePriority; PriorityLevel effectivePriority;
  std::int32_t nice; Tick cpuTime;
  std::optional<std::size_t> laneIndex;
};

struct TuiSchedulerSnapshot
{
  std::vector<ProcessId> readyQueue;
  std::vector<ProcessId> blockedQueue;
  std::vector<ProcessId> runningQueue;
  std::vector<std::vector<ProcessId>> perLaneReadyQueues;
  std::size_t readyCount, blockedCount;
  std::string policyName;
};

struct TuiMemorySnapshot
{
  std::size_t totalVirtualSlots, freeVirtualSlots;
  std::optional<std::size_t> totalFrames, freeFrames;
  std::vector<std::optional<ProcessId>> frameOwners;
};

struct TuiSnapshot
{
  Tick currentTick;
  std::size_t processCount;
  std::vector<TuiProcessSnapshot> processes;
  TuiSchedulerSnapshot scheduler;
  TuiMemorySnapshot memory;
  std::uint64_t sequence;
  std::uint64_t epoch;
};

struct TuiHistoryEntry { std::size_t sequence; TuiSnapshot snapshot; };
```

Префикс `Tui` отделяет UI-DTO от kernel-доменных типов (например, `KernelSnapshot` остаётся
ядро-обращённым; UI-слой работает с `TuiSnapshot`).

## Адаптеры: Diagnostics → ReadModel

Цепочка чтения: `IKernel::snapshot()` → `KernelDiagnostics` (адаптер диагностики) →
`KernelReadModel` (адаптер UI-снимка, реализует `IKernelReadModel`). `KernelReadModel`
**не** хранит симуляционное состояние и не имеет права писать в ядро; он только формирует
очередной `TuiSnapshot`.

### Схема: граница `contur2_lib` ↔ `contur2_tui`

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="140" parent="1" style="rounded=1;whiteSpace=wrap;html=1;dashed=1;fillColor=#f5f5f5;strokeColor=#666666;" value="contur2_lib (Kernel — без UI)" vertex="1"><mxGeometry x="40" y="40" width="300" height="280" as="geometry" /></mxCell>
    <mxCell id="141" parent="140" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="IKernel::snapshot()" vertex="1"><mxGeometry x="30" y="50" width="240" height="40" as="geometry" /></mxCell>
    <mxCell id="142" parent="140" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="IKernelDiagnostics" vertex="1"><mxGeometry x="30" y="110" width="240" height="40" as="geometry" /></mxCell>
    <mxCell id="143" parent="140" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="KernelDiagnostics (adapter)" vertex="1"><mxGeometry x="30" y="170" width="240" height="40" as="geometry" /></mxCell>
    <mxCell id="150" parent="1" style="rounded=1;whiteSpace=wrap;html=1;dashed=1;fillColor=#f5f5f5;strokeColor=#666666;" value="contur2_tui (внешний UI-модуль)" vertex="1"><mxGeometry x="400" y="40" width="400" height="380" as="geometry" /></mxCell>
    <mxCell id="151" parent="150" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="IKernelReadModel" vertex="1"><mxGeometry x="30" y="50" width="220" height="40" as="geometry" /></mxCell>
    <mxCell id="152" parent="150" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="KernelReadModel (adapter)" vertex="1"><mxGeometry x="30" y="100" width="240" height="40" as="geometry" /></mxCell>
    <mxCell id="153" parent="150" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#fff2cc;strokeColor=#d6b656;" value="HistoryBuffer (bounded ring)" vertex="1"><mxGeometry x="30" y="150" width="240" height="40" as="geometry" /></mxCell>
    <mxCell id="154" parent="150" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#e1d5e7;strokeColor=#9673a6;" value="ITuiController / TuiController" vertex="1"><mxGeometry x="30" y="200" width="260" height="40" as="geometry" /></mxCell>
    <mxCell id="155" parent="150" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="IRenderer (views: process/scheduler/memory/dashboard)" vertex="1"><mxGeometry x="30" y="250" width="340" height="40" as="geometry" /></mxCell>
    <mxCell id="156" parent="150" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#f8cecc;strokeColor=#b85450;" value="FtxuiRenderer (backend)" vertex="1"><mxGeometry x="30" y="300" width="240" height="40" as="geometry" /></mxCell>
    <mxCell id="157" parent="150" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#f8cecc;strokeColor=#b85450;" value="FtxuiApp (input loop + autoplay)" vertex="1"><mxGeometry x="30" y="340" width="260" height="40" as="geometry" /></mxCell>
    <mxCell id="160" edge="1" parent="1" source="141" target="142" style="endArrow=block;html=1;dashed=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="161" edge="1" parent="1" source="143" target="151" style="endArrow=classic;html=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="162" edge="1" parent="1" source="151" target="152" style="endArrow=block;html=1;dashed=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="163" edge="1" parent="1" source="152" target="154" style="endArrow=classic;html=1;" value="capture TuiSnapshot"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="164" edge="1" parent="1" source="154" target="153" style="endArrow=classic;html=1;" value="append history"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="165" edge="1" parent="1" source="154" target="155" style="endArrow=classic;html=1;" value="current()"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="166" edge="1" parent="1" source="156" target="155" style="endArrow=block;html=1;dashed=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="167" edge="1" parent="1" source="157" target="154" style="endArrow=classic;html=1;" value="dispatch(TuiCommand)"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Контроллер: команды, autoplay, история

`TuiCommand` обобщает действия — `tick(n)`, `autoplay(start/stop/pause/resume)`,
`seekBackward(n)`, `seekForward(n)` — параметризуемые через `TuiPlaybackConfig` (интервал и
stride). Стейт-машина контроллера простая: `Idle ↔ Playing ↔ Paused`.

`TuiController` реализует контракт `ITuiController`, владея кольцевым `HistoryBuffer` фиксированного
размера; `advanceAutoplay(elapsedMs)` накапливает время и продвигает «тик ядра» через
инжектированный `TickFn`.

```cpp
class TuiController final : public ITuiController
{
  public:
  using TickFn = std::function<Result<void>(std::size_t step)>;

  TuiController(const IKernelReadModel &readModel, TickFn tickFn, std::size_t historyCapacity = 256);

  [[nodiscard]] Result<void> dispatch(const TuiCommand &command) override;
  [[nodiscard]] Result<void> advanceAutoplay(std::uint32_t elapsedMs) override;

  [[nodiscard]] const TuiSnapshot &current() const noexcept override;
  [[nodiscard]] TuiControllerState state() const noexcept override;
  [[nodiscard]] std::size_t historySize() const noexcept override;
  [[nodiscard]] std::size_t historyCursor() const noexcept override;
};
```

Важное архитектурное правило: `seekBackward/seekForward` двигают только курсор внутри
истории UI и не откатывают ядро. Это упрощает воспроизведение и исключает по умолчанию
сложные сценарии «time-travel» в самой симуляции.

### Схема: стейт-машина playback

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="170" parent="1" style="ellipse;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="Idle" vertex="1"><mxGeometry x="60" y="100" width="100" height="50" as="geometry" /></mxCell>
    <mxCell id="171" parent="1" style="ellipse;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="Playing" vertex="1"><mxGeometry x="240" y="100" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="172" parent="1" style="ellipse;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="Paused" vertex="1"><mxGeometry x="440" y="100" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="173" edge="1" parent="1" source="170" target="171" style="endArrow=classic;html=1;" value="autoplay start / Space"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="174" edge="1" parent="1" source="171" target="172" style="endArrow=classic;html=1;" value="pause / Space"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="175" edge="1" parent="1" source="172" target="171" style="endArrow=classic;html=1;" value="resume / r"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="176" edge="1" parent="1" source="172" target="170" style="endArrow=classic;html=1;" value="stop / s"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="177" edge="1" parent="1" source="171" target="171" style="endArrow=classic;html=1;" value="advanceAutoplay tick"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="178" edge="1" parent="1" source="172" target="172" style="endArrow=classic;html=1;" value="seek ±N (cursor only)"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Backend `FtxuiRenderer` + оболочка `FtxuiApp`

`IRenderer` остаётся backend-агностичным контрактом; в проекте реализован один backend —
`FtxuiRenderer` поверх библиотеки [FTXUI](https://github.com/ArthurSonzogni/FTXUI). Поверх него
работает `FtxuiApp`, который:

- держит интерактивный экран FTXUI (`ScreenInteractive`);
- разбирает клавиатурные события и преобразует их в `TuiCommand`;
- крутит таймер autoplay (frame interval по умолчанию 33 мс);
- опционально читает kernel-логи через callback `logProvider` (в реальном `main.cpp` —
    `BufferSink` трассировщика) и отображает их в нижней панели.

Клавиатурный контракт `FtxuiApp` (взят из заголовка `ftxui_app.h`):

| Клавиша(и) | Действие |
|---|---|
| `Space` / `p` | Toggle Play / Pause |
| `t` / `n` | Один ручной тик |
| `←` / `h` | Seek назад на 1 шаг |
| `→` / `l` | Seek вперёд на 1 шаг |
| `Shift+←` / `H` | Seek назад на 10 шагов |
| `Shift+→` / `L` | Seek вперёд на 10 шагов |
| `+` | Ускорить autoplay (interval × ½) |
| `-` | Замедлить autoplay (interval × 2) |
| `↑` / `k` | Прокрутка логов вверх |
| `↓` / `j` | Прокрутка логов вниз |
| `r` | Resume autoplay с последнего снимка |
| `s` | Stop autoplay |
| `q` / `Esc` | Выход |

`FtxuiAppConfig` управляет дефолтным интервалом autoplay, шагом, частотой кадров и
границами скорости.

## Архитектурная инвариант: ядро без UI

В `src/CMakeLists.txt` явно зафиксировано: `contur2_lib` не зависит и не должен зависеть от
`contur2_tui`; `contur2_tui` тянет `ftxui::ftxui` и `contur2_lib`; исполняемый `contur2 (app)`
линкуется только через `contur2_tui` и `contur2_demos`. Это позволяет запускать ядро headless
(тесты, CI, native execution), не таща FTXUI в build-граф.

## Источники кода, использованные в отчёте

- `src/include/contur/tui/tui_models.h`
- `src/include/contur/tui/tui_commands.h`
- `src/include/contur/kernel/i_kernel_diagnostics.h`
- `src/include/contur/kernel/kernel_diagnostics.h` (+ `.cpp`)
- `src/include/contur/tui/i_kernel_read_model.h`
- `src/contur/tui/kernel_read_model.cpp`
- `src/include/contur/tui/history_buffer.h` (+ `.cpp`)
- `src/include/contur/tui/i_tui_controller.h`
- `src/contur/tui/tui_controller.cpp`
- `src/include/contur/tui/i_renderer.h`
- `src/include/contur/tui/process_view.h`, `scheduler_view.h`, `memory_map_view.h`,
    `dashboard.h`
- `src/include/contur/tui/ftxui_renderer.h` (+ `.cpp`)
- `src/include/contur/tui/ftxui_app.h` (+ `.cpp`)
- `src/app/main.cpp`
- `src/app/CMakeLists.txt`
- `src/CMakeLists.txt` (CMake-инварианта про `contur2_lib`/`contur2_tui`)

## Критерии готовности

- Описаны DTO-контракты модели и зафиксирован префикс `Tui` для UI-типов.
- Описана адаптерная цепочка `Kernel → Diagnostics → ReadModel → TuiSnapshot`.
- Описаны команды и стейт-машина контроллера; зафиксировано отсутствие kernel-rollback при
    `seek*`.
- Описан backend `FtxuiRenderer` + оболочка `FtxuiApp` с полным набором горячих клавиш.
- Зафиксирована CMake-инварианта «`contur2_lib` без UI».

## Краткие выводы

UI-слой Contur 2 представлен как полноценное FTXUI-приложение поверх MVC-контрактов. Снимки
ядра передаются через адаптеры, история живёт только в UI, а ядро остаётся headless и
независимым. Пользователь может ставить симуляцию на паузу, делать одиночные тики, seek-ать
по истории и наблюдать живой поток событий трассировщика, не зная о внутреннем устройстве
ядра.

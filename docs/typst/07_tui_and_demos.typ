#import "@preview/smk-sto:0.3.1": *
#import "@preview/fletcher:0.5.8" as fletcher: diagram, edge, node
#import "_meta.typ": *

#show: lab-report.with(
  institute: institute,
  department: department,
  work-number: 7,
  discipline: discipline,
  title: "Терминальный пользовательский интерфейс и приложение визуализации ядра",
  author: author,
  supervisor: supervisor,
  designation: designation,
)

= Цель работы

Спроектировать и реализовать внешний UI-модуль для симулятора Contur 2, полностью
изолированный от ядра. UI получает данные через read-only снимки, владеет собственной
историей воспроизведения и предоставляет пользователю интерактивный контроль над тиками
симуляции: запуск/пауза автопроигрывания, ручные шаги, перемещение по истории. В качестве
backend-а рендеринга используется библиотека FTXUI.

Задачи работы:

+ описать иммутабельные DTO-контракты модели (`TuiProcessSnapshot`, `TuiSchedulerSnapshot`,
  `TuiMemorySnapshot`, `TuiSnapshot`, `TuiHistoryEntry`);
+ описать команды контроллера (`TuiCommand`, `TuiCommandKind`, `TuiPlaybackConfig`) и его
  стейт-машину;
+ описать адаптерную цепочку «снимок ядра → диагностика → read-model → UI-снимок»;
+ реализовать backend-агностичный контракт `IRenderer` и FTXUI-реализацию рендерера и
  приложения;
+ зафиксировать архитектурную инварианту «`contur2_lib` без UI».

= Реализация

== DTO-контракты модели

Иммутабельные структуры, передаваемые из ядра в UI, определены в `tui_models.h`. Префикс
`Tui` отделяет UI-DTO от kernel-доменных типов:

```cpp
struct TuiProcessSnapshot {
    ProcessId id; std::string name;
    ProcessState state;
    PriorityLevel basePriority, effectivePriority;
    std::int32_t nice; Tick cpuTime;
    std::optional<std::size_t> laneIndex;
};

struct TuiSchedulerSnapshot {
    std::vector<ProcessId> readyQueue, blockedQueue, runningQueue;
    std::vector<std::vector<ProcessId>> perLaneReadyQueues;
    std::size_t readyCount, blockedCount;
    std::string policyName;
};

struct TuiMemorySnapshot {
    std::size_t totalVirtualSlots, freeVirtualSlots;
    std::optional<std::size_t> totalFrames, freeFrames;
    std::vector<std::optional<ProcessId>> frameOwners;
};

struct TuiSnapshot {
    Tick currentTick; std::size_t processCount;
    std::vector<TuiProcessSnapshot> processes;
    TuiSchedulerSnapshot scheduler;
    TuiMemorySnapshot memory;
    std::uint64_t sequence, epoch;
};

struct TuiHistoryEntry { std::size_t sequence; TuiSnapshot snapshot; };
```

== Адаптерная цепочка

UI получает данные не напрямую из ядра, а через цепочку адаптеров: `IKernel::snapshot()` →
`KernelDiagnostics` (адаптер диагностики) → `KernelReadModel` (адаптер UI-снимка). Это
позволяет ядру оставаться headless и независимо тестируемым, а UI — переезжать на любой
backend без правок в ядре. Цепочка чтения и общая структура подсистем показаны на
рисунке @fig:tui-boundary.

#figure(
  diagram(
    spacing: (2.6cm, 1.3cm),
    node-inset: 7pt,
    node((0, 0), [IKernel::snapshot()], fill: rgb("#dae8fc")),
    node((0, 1), [IKernelDiagnostics], fill: rgb("#d5e8d4")),
    node((0, 2), [KernelDiagnostics], fill: rgb("#ffe6cc")),
    node((1, 2), [IKernelReadModel], fill: rgb("#dae8fc")),
    node((1, 3), [KernelReadModel], fill: rgb("#d5e8d4")),
    node((1, 4), [TuiController], fill: rgb("#e1d5e7")),
    node((2, 4), [HistoryBuffer], fill: rgb("#fff2cc")),
    node((1, 5), [IRenderer], fill: rgb("#ffe6cc")),
    node((1, 6), [FtxuiRenderer / FtxuiApp], fill: rgb("#f8cecc")),
    edge((0, 0), (0, 1), "-->"),
    edge((0, 1), (0, 2), "-->"),
    edge((0, 2), (1, 2), "->"),
    edge((1, 2), (1, 3), "-->"),
    edge((1, 3), (1, 4), "->", [capture]),
    edge((1, 4), (2, 4), "->", [append]),
    edge((1, 4), (1, 5), "->", [current()]),
    edge((1, 6), (1, 5), "-->"),
  ),
  caption: [Граница `contur2_lib` ↔ `contur2_tui` и цепочка адаптеров],
) <fig:tui-boundary>

== Контроллер, команды и история

`TuiCommand` обобщает действия — `tick(n)`, `autoplay(start/stop/pause/resume)`,
`seekBackward(n)`, `seekForward(n)` — с параметрами из `TuiPlaybackConfig`. Стейт-машина
контроллера представлена тремя состояниями: `Idle`, `Playing`, `Paused`. Состояние
`Playing` отрабатывает шаги `advance tick`, состояние `Paused` обрабатывает команды `seek
±N` (см. рисунок @fig:playback-fsm).

```cpp
class TuiController final : public ITuiController {
    public:
    using TickFn = std::function<Result<void>(std::size_t step)>;

    TuiController(const IKernelReadModel &readModel, TickFn tickFn,
                  std::size_t historyCapacity = 256);

    [[nodiscard]] Result<void> dispatch(const TuiCommand &command) override;
    [[nodiscard]] Result<void> advanceAutoplay(std::uint32_t elapsedMs) override;

    [[nodiscard]] const TuiSnapshot &current() const noexcept override;
    [[nodiscard]] TuiControllerState state() const noexcept override;
    [[nodiscard]] std::size_t historySize() const noexcept override;
    [[nodiscard]] std::size_t historyCursor() const noexcept override;
};
```

`HistoryBuffer` — это кольцевой буфер ограниченного размера, хранящий снимки UI с курсором
для навигации. Архитектурное правило: `seekBackward` / `seekForward` двигают только курсор
внутри истории UI и не откатывают состояние ядра.

#figure(
  diagram(
    spacing: (3.2cm, 2.0cm),
    node-shape: fletcher.shapes.ellipse,
    node-inset: 8pt,
    node((0, 0), [Idle], fill: rgb("#dae8fc")),
    node((1, 0), [Playing], fill: rgb("#d5e8d4")),
    node((2, 0), [Paused], fill: rgb("#ffe6cc")),
    edge((0, 0), (1, 0), "->", [autoplay start / Space]),
    edge((1, 0), (2, 0), "->", [pause / Space], bend: 30deg),
    edge((2, 0), (1, 0), "->", [resume / r], bend: 30deg),
    edge((2, 0), (0, 0), "->", [stop / s], bend: 50deg, label-side: left),
  ),
  caption: [Стейт-машина playback контроллера],
) <fig:playback-fsm>

== FTXUI-рендерер и оболочка приложения

`IRenderer` остаётся backend-агностичным контрактом. В проекте реализован один backend —
`FtxuiRenderer` поверх библиотеки FTXUI. Поверх него работает `FtxuiApp`, который держит
интерактивный экран FTXUI (`ScreenInteractive`), разбирает клавиатурные события и
преобразует их в `TuiCommand`, ведёт таймер автопроигрывания (по умолчанию 33 мс на кадр)
и отображает kernel-логи через callback `logProvider` (в реальной точке входа —
`BufferSink` трассировщика). Клавиатурный контракт приложения приведён в таблице
@tab:keys.

#figure(
  table(
    columns: 2,
    align: (left, left),
    [Клавиша(и)], [Действие],
    [`Space` / `p`], [Toggle Play / Pause],
    [`t` / `n`], [Один ручной тик],
    [`←` / `h`], [Seek назад на 1 шаг],
    [`→` / `l`], [Seek вперёд на 1 шаг],
    [`Shift+←` / `H`], [Seek назад на 10 шагов],
    [`Shift+→` / `L`], [Seek вперёд на 10 шагов],
    [`+`], [Ускорить autoplay (interval × 1/2)],
    [`-`], [Замедлить autoplay (interval × 2)],
    [`↑` / `k`], [Прокрутка логов вверх],
    [`↓` / `j`], [Прокрутка логов вниз],
    [`r`], [Resume autoplay с последнего снимка],
    [`s`], [Stop autoplay],
    [`q` / `Esc`], [Выход],
  ),
  caption: [Клавиатурный контракт `FtxuiApp`],
) <tab:keys>

== Архитектурная инварианта «ядро без UI»

В корневом `CMakeLists.txt` явно зафиксировано: `contur2_lib` не зависит и не должен
зависеть от `contur2_tui`; `contur2_tui` тянет `ftxui::ftxui` и `contur2_lib`; исполняемое
приложение `contur2` линкуется только через `contur2_tui` и `contur2_demos`. Это позволяет
запускать ядро headless (тесты, CI, нативное исполнение) без FTXUI в build-графе.

= Заключение <s>

UI-слой представлен как полноценное FTXUI-приложение поверх MVC-контрактов. Снимки ядра
передаются через адаптеры, история живёт только в UI, а ядро остаётся headless и
независимым. Пользователь может ставить симуляцию на паузу, делать одиночные тики,
seek-ать по истории и наблюдать живой поток событий трассировщика, не зная о внутреннем
устройстве ядра.
#import "@preview/smk-sto:0.3.1": *
#import "@preview/fletcher:0.5.8" as fletcher: diagram, edge, node
#import "_meta.typ": *

#show: lab-report.with(
  institute: institute,
  department: department,
  work-number: 5,
  discipline: discipline,
  title: "Диспетчеризация, синхронизация и трассировка",
  author: author,
  supervisor: supervisor,
  designation: designation,
)

= Цель работы

Реализовать управляющий и наблюдательный слой ядра Contur 2: диспетчер процессов в
одно- и многопоточном режимах, набор примитивов синхронизации с двумя слоями ответственности,
обнаружение тупиковых ситуаций по двум графам ожиданий и подсистему трассировки с
нулевой стоимостью по умолчанию.

Задачи работы:

+ описать контракт `IDispatcher`, реализации `Dispatcher` и `MPDispatcher`;
+ ввести инжектируемую стратегию рантайма `IDispatchRuntime` и пул `DispatcherPool`;
+ разделить примитивы синхронизации на слои `KernelInternal` и `SimulatedResource`;
+ реализовать `DeadlockDetector` с двумя графами и алгоритмом банкира;
+ реализовать трассировку (`ITracer`, `Tracer`, `NullTracer`, sink-и).

= Реализация

== Контракт и иерархия диспетчера

Контракт жизненного цикла процесса декларирован в `IDispatcher`:

```cpp
class IDispatcher {
    public:
    [[nodiscard]] virtual Result<void> createProcess(std::unique_ptr<ProcessImage>, Tick) = 0;
    [[nodiscard]] virtual Result<void> dispatch(std::size_t tickBudget) = 0;
    [[nodiscard]] virtual Result<void> terminateProcess(ProcessId, Tick) = 0;
    virtual void tick() = 0;
    [[nodiscard]] virtual std::size_t processCount() const noexcept = 0;
    [[nodiscard]] virtual bool hasProcess(ProcessId) const noexcept = 0;
};
```

`Dispatcher` — однопоточная реализация, которая на каждом тике выбирает процесс через
`IScheduler`, исполняет его `IExecutionEngine` в пределах `tickBudget`, обрабатывает
`ExecutionResult` и обновляет состояния. `MPDispatcher` — обёртка над несколькими полосами
(lane-ами); распределение и продвижение полос делегируется инжектируемому
`IDispatchRuntime`. Сам `MPDispatcher` не создаёт рантайм неявно и возвращает
`InvalidState`, если он не был сконфигурирован композиционным корнем. Параллельное
исполнение полос обеспечивает `DispatcherPool`, читающий `HostThreadingConfig` (количество
host-потоков, режим детерминированности, поддержку work-stealing). Связи компонентов
показаны на рисунке @fig:dispatch-chain.

#figure(
  diagram(
    spacing: (2.6cm, 1.2cm),
    node-inset: 8pt,
    node((0, 0), [Kernel], fill: rgb("#dae8fc")),
    node((1, 0), [MPDispatcher], fill: rgb("#d5e8d4")),
    node((1, 1), [IDispatchRuntime], fill: rgb("#e1d5e7")),
    node((1, 2), [DispatcherPool], fill: rgb("#fff2cc")),
    node((2, -1), [Dispatcher \ lane 0], fill: rgb("#ffe6cc")),
    node((2, 0), [Dispatcher \ lane 1], fill: rgb("#ffe6cc")),
    node((2, 1), [Dispatcher \ lane n-1], fill: rgb("#ffe6cc")),
    edge((0, 0), (1, 0), "->", [tick / createProcess], label-side: left, label-sep: 0.9em),
    edge((1, 0), (1, 1), "->", [injected]),
    edge((1, 1), (1, 2), "-->", [implements]),
    edge((1, 0), (2, -1), "-->"),
    edge((1, 0), (2, 0), "-->"),
    edge((1, 0), (2, 1), "-->"),
  ),
  caption: [Композиция диспетчера и пула host-потоков],
) <fig:dispatch-chain>

== Синхронизация на двух слоях

Все примитивы синхронизации реализуют `ISyncPrimitive` и обязательно сообщают свой слой
через `layer()`:

```cpp
enum class SyncLayer : std::uint8_t { KernelInternal, SimulatedResource };

class ISyncPrimitive {
    public:
    [[nodiscard]] virtual Result<void> acquire(ProcessId pid) = 0;
    [[nodiscard]] virtual Result<void> release(ProcessId pid) = 0;
    [[nodiscard]] virtual Result<void> tryAcquire(ProcessId pid) = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual SyncLayer layer() const noexcept = 0;
};
```

Различие принципиально: `SimulatedResource` — это учебные примитивы (`Mutex`, `Semaphore`,
`CriticalSection`), видимые как объекты симуляции и попадающие в граф ожиданий процессов;
`KernelInternal` — блокировки самого ядра и рантайма, попадающие в отдельный lock-order
граф. `Mutex` и `Semaphore` дополнительно учитывают priority-inheritance/boost, чтобы
предотвратить priority inversion в моделируемых сценариях.

== Обнаружение дедлоков

`DeadlockDetector` поддерживает оба слоя анализа: симулируемый wait-for граф процессов и
ресурсов (`onAcquire`/`onRelease`/`onWait`, `hasDeadlock`, `getDeadlockedProcesses`) и
lock-order граф внутренних блокировок ядра (`onInternalLockAcquire`/`...Release`,
`hasInternalLockOrderCycle`). Дополнительно реализован классический алгоритм банкира для
проверки безопасности состояния:

```cpp
[[nodiscard]] bool isSafeState(
    const std::vector<ResourceAllocation> &current,
    const std::vector<ResourceAllocation> &maximum,
    const std::vector<std::uint32_t> &available) const;
```

Все варианты обнаружения цикла принимают `ThreadToken`, что обеспечивает корректное
построение графов при работе нескольких lane-ов одновременно. Иллюстрация двух графов
дана на рисунке @fig:deadlocks.

#figure(
  diagram(
    spacing: (1.8cm, 1.8cm),
    node-shape: fletcher.shapes.ellipse,
    node-inset: 6pt,
    node((0, 0), [P1], fill: rgb("#dae8fc")),
    node((1, 0), [R1], fill: rgb("#d5e8d4")),
    node((2, 0), [P2], fill: rgb("#dae8fc")),
    node((1, 1), [R2], fill: rgb("#d5e8d4")),
    edge((0, 0), (1, 0), "->", [waits]),
    edge((1, 0), (2, 0), "->", [held by]),
    edge((2, 0), (1, 1), "->", [waits], label-side: left),
    edge((1, 1), (0, 0), "->", [held by], label-side: left),

    node((4, 0), [L1], fill: rgb("#fff2cc")),
    node((6, 0), [L2], fill: rgb("#fff2cc")),
    node((5, 1), [L3], fill: rgb("#fff2cc")),
    edge((4, 0), (6, 0), "->", [before]),
    edge((6, 0), (5, 1), "->", [before], label-side: left),
    edge((5, 1), (4, 0), "->", [cycle], stroke: red, label-side: left),
  ),
  caption: [Wait-for граф процессов (слева) и lock-order граф ядра (справа)],
) <fig:deadlocks>

== Трассировка

Контракт `ITracer` определяет минимальный API: `trace(event)`, `pushScope`/`popScope`,
`currentDepth`, `setMinLevel`/`minLevel` и доступ к `IClock`. Реализованы две стратегии и
RAII-обёртка:

- `Tracer` — активная реализация, пишет в `ITraceSink`;
- `NullTracer` — no-op для Release-сборки или тестов;
- `TraceScope` — RAII-обёртка для `pushScope`/`popScope`;
- макросы `CONTUR_TRACE_SCOPE` / `CONTUR_TRACE` компилируются в no-op, если
  `CONTUR_TRACE_ENABLED` не определён.

Реализованы три sink-а: `ConsoleSink` (`stdout`), `FileSink` (файл), `BufferSink`
(in-memory; используется в TUI для live-просмотра и в тестах для проверки последовательности
событий). В многопоточном режиме `TraceEvent` несёт дополнительные метаданные —
идентификатор worker-а, sequence и epoch, — что обеспечивает воспроизводимый порядок
событий в детерминированном режиме. Структура слоя приведена на рисунке @fig:tracer.

#figure(
  diagram(
    spacing: (2.4cm, 1.4cm),
    node-inset: 7pt,
    node((0, 0), [Subsystems \ (CPU, MMU, Scheduler, IPC)], fill: rgb("#dae8fc")),
    node((1, 0), [ITracer], fill: rgb("#d5e8d4")),
    node((0, 1), [Tracer (active)], fill: rgb("#ffe6cc")),
    node((2, 1), [NullTracer (no-op)], fill: rgb("#ffe6cc")),
    node((0, 2), [ITraceSink], fill: rgb("#fff2cc")),
    node((0, 3), [ConsoleSink / FileSink / BufferSink], fill: rgb("#f8cecc")),
    edge((0, 0), (1, 0), "->", [trace()]),
    edge((0, 1), (1, 0), "-->"),
    edge((2, 1), (1, 0), "-->"),
    edge((0, 1), (0, 2), "->", [write(event)]),
    edge((0, 3), (0, 2), "-->"),
  ),
  caption: [Компоненты подсистемы трассировки и их связь],
) <fig:tracer>

= Заключение <s>

Диспетчер, синхронизация и трассировка образуют управляющий и наблюдательный слой ядра.
Диспетчер и рантайм разнесены по разным контрактам, благодаря чему режимы `N = 1` и `N > 1`
живут в одной кодовой базе без скрытых веток. Синхронизация разделена на учебный и
внутренний слои; обнаружение дедлоков работает на обоих графах одновременно. Трассировка
устроена как нулевая стоимость по умолчанию и многоступенчатый Observer при включённом
флаге, что позволяет TUI визуализировать ядро в реальном времени без модификаций самих
подсистем.
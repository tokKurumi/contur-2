# Отчет 5. Диспетчеризация, синхронизация и трассировка

## Покрываемые стадии

- Стадия 7: Dispatch + Synchronization
- Стадия 11: Host Multithreading Runtime
- Стадия 12: Tracing

## Контекст и цель

После того как в проекте были реализованы CPU, память, процессы и планирование, следующая задача — связать эти части в управляемый runtime. На этом этапе Contur 2 получает слой диспетчеризации, который отвечает за создание процессов, запуск исполнения, завершение, обработку блокировок и координацию одно- и многопоточного режима на хосте.

Одновременно с этим появляются два дополнительных слоя системной инфраструктуры:

- симулируемая синхронизация внутри модели ОС, где работают `Mutex` и `Semaphore`;
- трассировка, которая делает работу ядра наблюдаемой и пригодной для анализа.

Итог этой стадии — не просто «запуск процессов», а управляемый жизненный цикл, где есть предсказуемая диспетчеризация, контроль deadlock и подробная телеметрия исполнения.

Иллюстрация для вставки в Word:

- Рисунок: схема worker lanes и work stealing на уровне runtime.

## Что обязательно описать

1. Роли `Dispatcher`, `MPDispatcher`, `DispatcherPool`.
2. Разделение kernel-internal sync и simulated sync (`Mutex`, `Semaphore`).
3. Модель deadlock detection: simulated wait-for + internal lock-order.
4. Deterministic mode для `N > 1`.
5. `Tracer`, `TraceScope`, sink-архитектура и null-object поведение.

## Диспетчеризация процесса

Диспетчер в Contur 2 — это orchestration layer между scheduler, execution engine и виртуальной памятью. Он не выбирает политику планирования сам и не исполняет инструкции напрямую. Его задача — подготовить процесс, отдать его в execution engine, обработать результат и привести систему в согласованное состояние.

### Контракт IDispatcher

```cpp
class IDispatcher
{
    public:
    virtual ~IDispatcher() = default;

    [[nodiscard]] virtual Result<void> createProcess(std::unique_ptr<ProcessImage> process, Tick currentTick) = 0;
    [[nodiscard]] virtual Result<void> dispatch(std::size_t tickBudget) = 0;
    [[nodiscard]] virtual Result<void> terminateProcess(ProcessId pid, Tick currentTick) = 0;
    virtual void tick() = 0;
    [[nodiscard]] virtual std::size_t processCount() const noexcept = 0;
    [[nodiscard]] virtual bool hasProcess(ProcessId pid) const noexcept = 0;
};
```

Этот интерфейс задает минимальный набор операций, необходимых для жизненного цикла процесса:

- создание и загрузка `ProcessImage`;
- запуск одного dispatch-цикла;
- завершение процесса по запросу;
- продвижение локального времени диспетчера;
- проверка наличия процесса в управляемом наборе.

### Базовый Dispatcher

`Dispatcher` реализует единопоточную модель. Он хранит процессы, вызывает scheduler для выбора следующего кандидата, передает его в execution engine и обновляет виртуальную память при создании и завершении процесса.

Ключевая идея этой реализации в том, что диспетчер остается владельцем жизненного цикла, но не смешивает все обязанности в одном месте. Он использует:

- `IScheduler` для выбора процесса;
- `IExecutionEngine` для выполнения;
- `IVirtualMemory` для выделения и освобождения адресного пространства;
- `IClock` для единого временного порядка;
- `ITracer` для наблюдаемости.

Фрагмент логики `createProcess(...)` показывает последовательность действий: сначала выделяется slot во virtual memory, затем код загружается в сегмент, после чего процесс помещается в scheduler.

```cpp
auto alloc = impl_->virtualMemory.allocateSlot(pid, slotSize);
auto load = impl_->virtualMemory.loadSegment(pid, process->code());
auto enq = impl_->scheduler.enqueue(process->pcb(), currentTick);
```

Такое разделение полезно тем, что загрузка кода и регистрация процесса в scheduler не происходят «половинчато»: если один из шагов завершается ошибкой, ресурсы освобождаются, а процесс не остается в наполовину созданном состоянии.

### Схема: lifecycle + dispatcher sequence

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="100" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="Kernel" vertex="1"><mxGeometry x="40" y="60" width="100" height="50" as="geometry" /></mxCell>
    <mxCell id="101" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="Dispatcher" vertex="1"><mxGeometry x="200" y="60" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="102" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="Scheduler" vertex="1"><mxGeometry x="380" y="60" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="103" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="Engine" vertex="1"><mxGeometry x="550" y="60" width="100" height="50" as="geometry" /></mxCell>
    <mxCell id="104" edge="1" parent="1" source="100" target="101" style="endArrow=classic;html=1;" value="create/tick"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="105" edge="1" parent="1" source="101" target="102" style="endArrow=classic;html=1;" value="select"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="106" edge="1" parent="1" source="101" target="103" style="endArrow=classic;html=1;" value="execute"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

### Обработка результатов исполнения

`dispatch(...)` работает по четкой схеме: сначала scheduler выбирает процесс, затем execution engine исполняет его не более чем в пределах заданного tick budget. По результату диспетчер решает, что делать дальше:

- если budget исчерпан, процесс остается в системе и будет выбран позже;
- если возникло прерывание, процесс переводится в blocked состояние;
- если процесс завершился или произошла ошибка, он удаляется из scheduler и освобождает виртуальную память.

В этом месте dispatcher и runtime не скрывают логику за исключениями. Все решения возвращаются явно через `Result<void>` и `ExecutionResult`, что делает путь управления предсказуемым.

## Многопоточный runtime

### MPDispatcher

`MPDispatcher` — это координирующий слой, который распределяет процессы между несколькими `IDispatcher` и делегирует исполнение runtime-слою. Его роль в системе — не исполнять сам процесс, а маршрутизировать работу на несколько диспетчерских lanes.

```cpp
class MPDispatcher
{
    public:
    Result<void> createProcess(std::unique_ptr<ProcessImage> process, Tick currentTick) override;
    Result<void> dispatch(std::size_t tickBudget) override;
    Result<void> terminateProcess(ProcessId pid, Tick currentTick) override;
    void tick() override;
    std::size_t processCount() const noexcept override;
    bool hasProcess(ProcessId pid) const noexcept override;
};
```

Поведение здесь простое и важное одновременно:

- новый процесс назначается на dispatcher по стабильному правилу `pid % dispatchers.size()`;
- dispatch выполняется через host runtime, который управляет всеми lanes;
- завершение ищет процесс по всем диспетчерам, пока не найдет владельца.

Такой подход обеспечивает горизонтальное масштабирование диспетчеризации без изменения модели процессов.

### DispatcherPool и host multithreading

`DispatcherPool` реализует host-thread runtime: он создает worker threads и распределяет по ним задания `Dispatch` и `Tick` для набора dispatcher lanes.

Ключевая особенность этой реализации — наличие двух режимов работы:

- **deterministic mode**: статическое разбиение lanes между workers, чтобы получить воспроизводимый порядок событий;
- **dynamic mode**: динамическая балансировка с work stealing, когда свободный worker может подхватить очередную lane.

Из кода видно, что в deterministic mode используется стабильное stride-распределение:

```cpp
for (std::size_t laneIndex = workerIndex; laneIndex < jobLanes.size(); laneIndex += workerCount)
```

Это означает, что один и тот же набор lanes всегда будет обслуживаться одним и тем же порядком worker-ов. Такой режим критически важен для тестирования и сравнения результатов, потому что убирает случайность из многопоточной среды.

### Схема: worker lanes и work stealing

Иллюстрация для вставки в Word:

- Рисунок: схема worker lanes и work stealing.

## Синхронизация: kernel-internal и simulated

На этой стадии важно различать два уровня синхронизации.

### Kernel-internal sync

Это синхронизация внутри самого runtime и инфраструктурных классов. Она защищает внутренние структуры данных от гонок между host threads. Именно здесь используются обычные C++ примитивы и блокировки: mutex, shared_mutex, condition_variable, атомики.

### Simulated sync

Это уже модель синхронизации внутри самой симулируемой ОС. Здесь работают `Mutex`, `Semaphore` и связанные с ними правила приоритета и ожидания. Эти примитивы не защищают memory model C++ напрямую; они моделируют поведение процессов в рамках учебной ОС.

### Mutex и Semaphore

`Mutex` реализован как reentrant mutex с учетом владения и глубины рекурсии:

```cpp
class Mutex final : public ISyncPrimitive
{
    public:
    [[nodiscard]] Result<void> acquire(ProcessId pid) override;
    [[nodiscard]] Result<void> release(ProcessId pid) override;
    [[nodiscard]] Result<void> tryAcquire(ProcessId pid) override;
    [[nodiscard]] std::string_view name() const noexcept override;
    [[nodiscard]] SyncLayer layer() const noexcept override;

    [[nodiscard]] Result<void> registerProcessPriority(ProcessId pid, PriorityLevel basePriority);
    [[nodiscard]] PriorityLevel effectivePriority(ProcessId pid) const noexcept;
    [[nodiscard]] PriorityLevel basePriority(ProcessId pid) const noexcept;
};
```

`Semaphore` моделирует счетчик доступных единиц ресурса:

```cpp
class Semaphore final : public ISyncPrimitive
{
    public:
    explicit Semaphore(std::size_t initialCount = 1, std::size_t maxCount = 1);

    [[nodiscard]] Result<void> acquire(ProcessId pid) override;
    [[nodiscard]] Result<void> release(ProcessId pid) override;
    [[nodiscard]] Result<void> tryAcquire(ProcessId pid) override;
    [[nodiscard]] std::string_view name() const noexcept override;
    [[nodiscard]] SyncLayer layer() const noexcept override;
};
```

Смысл этого разделения в том, что host runtime защищает сам себя, а simulated sync управляет поведением процессов внутри модели. Эти уровни нельзя смешивать: если runtime начнет использовать simulated mutex для защиты собственных структур, модель станет некорректной.

## Deadlock detection

Детектор deadlock в проекте строится сразу на двух графах.

### Simulated wait-for graph

Этот граф отражает отношения ожидания между процессами и ресурсами внутри модели ОС. Узел здесь — это пара `{pid, threadToken}`, а ребро означает, что процесс ждет ресурс, удерживаемый другим узлом.

### Internal lock-order graph

Этот граф описывает порядок взятия внутренних блокировок runtime. Он не относится к симулируемым процессам напрямую, а нужен для предотвращения deadlock в host-слое.

Внутри `DeadlockDetector` хранятся оба представления:

```cpp
std::unordered_map<ResourceId, std::unordered_set<WaitNode, WaitNodeHash>> resourceOwners;
std::unordered_map<WaitNode, std::unordered_set<WaitNode, WaitNodeHash>, WaitNodeHash> waitFor;

std::unordered_map<ThreadToken, std::vector<ResourceId>> heldInternalLocks;
std::unordered_map<ResourceId, std::unordered_set<ResourceId>> lockOrderGraph;
```

Модель работает так:

1. При захвате ресурса узел добавляется в `resourceOwners`.
2. При освобождении очищаются исходящие ожидания.
3. `hasWaitForCycle()` ищет цикл в simulated graph через DFS.
4. `hasLockOrderCycle()` ищет цикл в internal graph.
5. При обнаружении цикла возвращается список узлов, участвующих в deadlock.

Это решение полезно тем, что позволяет анализировать сразу два класса проблем: дедлоки самой симуляции и дедлоки инфраструктуры runtime.

### Схема: двойной граф deadlock detection

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="110" parent="1" style="rounded=1;whiteSpace=wrap;html=1;dashed=1;" value="Simulated Wait-For Graph" vertex="1"><mxGeometry x="40" y="40" width="300" height="220" as="geometry" /></mxCell>
    <mxCell id="111" parent="1" style="rounded=1;whiteSpace=wrap;html=1;dashed=1;" value="Internal Lock-Order Graph" vertex="1"><mxGeometry x="380" y="40" width="300" height="220" as="geometry" /></mxCell>
    <mxCell id="112" parent="110" style="ellipse;whiteSpace=wrap;html=1;" value="P1" vertex="1"><mxGeometry x="30" y="70" width="50" height="40" as="geometry" /></mxCell>
    <mxCell id="113" parent="110" style="ellipse;whiteSpace=wrap;html=1;" value="R1" vertex="1"><mxGeometry x="130" y="70" width="50" height="40" as="geometry" /></mxCell>
    <mxCell id="114" edge="1" parent="110" source="112" target="113" style="endArrow=classic;html=1;" value="wait"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="115" parent="111" style="ellipse;whiteSpace=wrap;html=1;" value="L1" vertex="1"><mxGeometry x="30" y="70" width="50" height="40" as="geometry" /></mxCell>
    <mxCell id="116" parent="111" style="ellipse;whiteSpace=wrap;html=1;" value="L2" vertex="1"><mxGeometry x="130" y="70" width="50" height="40" as="geometry" /></mxCell>
    <mxCell id="117" edge="1" parent="111" source="115" target="116" style="endArrow=classic;html=1;" value="before"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Трассировка

### Контракт ITracer

Трассер в проекте — это общий интерфейс наблюдаемости для всех подсистем ядра. Он поддерживает явную отправку событий и вложенные scope-уровни:

```cpp
class ITracer
{
    public:
    virtual ~ITracer() = default;

    virtual void trace(const TraceEvent &event) = 0;
    virtual void pushScope(std::string_view subsystem, std::string_view operation) = 0;
    virtual void popScope() = 0;

    [[nodiscard]] virtual std::uint32_t currentDepth() const noexcept = 0;
    virtual void setMinLevel(TraceLevel level) noexcept = 0;
    [[nodiscard]] virtual TraceLevel minLevel() const noexcept = 0;
    [[nodiscard]] virtual const IClock &clock() const noexcept = 0;
};
```

Эта модель дает две важные возможности:

- трассировать события на уровне конкретных операций;
- вести иерархию вложенных вызовов через depth.

### Реализация Tracer

`Tracer` использует sink-архитектуру: события отправляются в `ITraceSink`, а сам tracer управляет уровнем логирования и глубиной стека.

```cpp
void Tracer::trace(const TraceEvent &event)
{
    if (static_cast<std::uint8_t>(event.level) < static_cast<std::uint8_t>(impl_->minLevel))
    {
        return;
    }

    std::lock_guard<std::mutex> lock(impl_->sinkMutex);
    impl_->sink->write(event);
}

void Tracer::pushScope(std::string_view subsystem, std::string_view operation)
{
    trace(makeTraceEvent(impl_->traceClock.now(), subsystem, operation, "enter", currentDepth()));
    ++Impl::depth;
}
```

Здесь важны три момента:

1. `trace(...)` фильтрует события по минимальному уровню.
2. Запись в sink защищена mutex-ом, чтобы несколько host threads не портили один поток логов.
3. `pushScope(...)` автоматически эмитит событие входа и увеличивает глубину.

### TraceScope и RAII

`TraceScope` автоматизирует пару `pushScope/popScope`:

```cpp
class TraceScope final
{
    public:
    TraceScope(ITracer &tracer, std::string_view subsystem, std::string_view operation)
        : tracer_(tracer)
        , active_(true)
    {
        tracer_.pushScope(subsystem, operation);
    }

    ~TraceScope()
    {
        if (active_)
        {
            tracer_.popScope();
        }
    }
};
```

Это делает трассировку устойчивой к ранним выходам и ошибкам: scope будет закрыт даже если функция вернется по error path.

### Null-object поведение и sink-архитектура

В коде отсутствует отдельный класс `NullTracer`, но его роль выполняется через `NullSink`: если sink не передан, `Tracer` подставляет пустую реализацию, которая просто игнорирует события. Это дает тот же практический эффект, что и null tracer: tracing infrastructure может быть выключена без изменения клиентского кода.

Иллюстрация для вставки в Word:

- Рисунок: пример trace-лога с глубиной вызовов.

## Проверка и воспроизводимость

Для подтверждения работы диспетчеризации, синхронизации и трассировки используется стандартный набор команд:

```bash
bash src/build.sh debug src
ctest --preset debug --output-on-failure
```

Проверяется:

- корректность создания и завершения процессов через dispatcher;
- стабильность многопоточного runtime в deterministic mode;
- поведение work stealing при включенной балансировке;
- отсутствие ложных deadlock-сигналов в internal lock-order graph;
- корректность trace scope depth и фильтрации по уровню;
- сохранение воспроизводимости логов в детерминированном режиме.

Иллюстрация для документа:

- Скриншот trace-лога с вложенными scope и timestamp-цепочкой.

## Источники кода, использованные в отчете

- `src/include/contur/dispatch/i_dispatcher.h`
- `src/contur/dispatch/dispatcher.cpp`
- `src/contur/dispatch/mp_dispatcher.cpp`
- `src/contur/dispatch/dispatcher_pool.cpp`
- `src/include/contur/dispatch/dispatcher_pool.h`
- `src/contur/sync/deadlock_detector.cpp`
- `src/include/contur/sync/mutex.h`
- `src/include/contur/sync/semaphore.h`
- `src/include/contur/tracing/i_tracer.h`
- `src/contur/tracing/tracer.cpp`
- `src/include/contur/tracing/trace_scope.h`
- `src/include/contur/tracing/trace_sink.h`

## Критерии готовности

- В тексте объяснены роли `Dispatcher`, `MPDispatcher` и `DispatcherPool`.
- Разделены kernel-internal sync и simulated sync.
- Показана модель deadlock detection на основе двух графов.
- Описан deterministic mode для многопоточного runtime.
- Раскрыта трассировка через `ITracer`, `TraceScope` и sink-архитектуру.
- Диаграммы размещены рядом с соответствующими разделами.

## Краткие выводы

Стадия dispatch + synchronization превращает Contur 2 в управляемый runtime, где процессы можно создавать, распределять, останавливать и наблюдать без потери целостности состояния. Разделение на dispatcher, host multithreading runtime и tracing делает архитектуру одновременно масштабируемой и диагностируемой.

Особенно важен тот факт, что система не смешивает simulated synchronization с внутренней синхронизацией runtime: это сохраняет корректность модели и позволяет отдельно анализировать как поведение процессов, так и стабильность самой инфраструктуры.

Трассировка завершает этот слой, предоставляя глубину вызовов, уровни событий и единый интерфейс наблюдаемости для всех ключевых подсистем.

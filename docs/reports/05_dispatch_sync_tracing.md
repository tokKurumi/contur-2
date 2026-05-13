# Отчет 5. Диспетчеризация, синхронизация и трассировка

## Охват этапов

- Этап 7: Dispatch + Synchronization
- Этап 11: Host Multithreading Runtime
- Этап 12: Tracing

## Контекст и цель

Эти три этапа решают одну общую задачу — сделать жизненный цикл процессов наблюдаемым,
повторяемым и безопасным как в однопоточном, так и в многопоточном режимах. Диспетчер
оркеструет переходы Ready → Running → Blocked → Terminated, синхронизация защищает разделяемые
ресурсы и проверяет отсутствие тупиков, а трассировка собирает события для пост-анализа и
визуализации в TUI.

Цели:

- описать роли `Dispatcher`, `MPDispatcher`, `DispatcherPool` и инжектируемого `IDispatchRuntime`;
- зафиксировать разделение `KernelInternal` и `SimulatedResource` для `ISyncPrimitive`;
- описать двойную модель обнаружения дедлоков: ожидание ресурсов + порядок взятия внутренних
    локов;
- показать слой трассировки (`ITracer`, `Tracer`, `NullTracer`, `TraceScope`, sink-и).

## Диспетчер: однопоточная и многопоточная композиция

`IDispatcher` — это контракт жизненного цикла:

```cpp
class IDispatcher
{
  public:
  [[nodiscard]] virtual Result<void> createProcess(std::unique_ptr<ProcessImage> process, Tick currentTick) = 0;
  [[nodiscard]] virtual Result<void> dispatch(std::size_t tickBudget) = 0;
  [[nodiscard]] virtual Result<void> terminateProcess(ProcessId pid, Tick currentTick) = 0;
  virtual void tick() = 0;
  [[nodiscard]] virtual std::size_t processCount() const noexcept = 0;
  [[nodiscard]] virtual bool hasProcess(ProcessId pid) const noexcept = 0;
};
```

`Dispatcher` — базовая реализация, которая собирает воедино `IScheduler`, `IExecutionEngine`,
`IVirtualMemory`, часы и трассировщик. Один тик диспетчера = одна итерация
«выбрать процесс → исполнить tickBudget → обработать `ExecutionResult` → обновить состояния».

`MPDispatcher` — обёртка над несколькими «полосами» (lane-ами) `IDispatcher`, через которые
проходят отдельные подмножества процессов. Распределение и продвижение полос делегируется
обязательно инжектируемому объекту `IDispatchRuntime`: сам `MPDispatcher` не создаёт рантайм
неявно и возвращает `InvalidState`, если он не был сконфигурирован композиционным корнем.

`DispatcherPool` — это реальный пул host-потоков, который выполняет полосы `MPDispatcher`
параллельно, читая `HostThreadingConfig` (`hostThreadCount`, `deterministicMode`,
`workStealingEnabled`). В детерминированном режиме пул синхронизируется на эпохах/барьерах,
чтобы обеспечить воспроизводимость порядка тиков при N>1.

### Схема: диспетчерская цепочка

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="100" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="Kernel" vertex="1"><mxGeometry x="40" y="40" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="101" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="IDispatcher (MPDispatcher)" vertex="1"><mxGeometry x="200" y="40" width="220" height="50" as="geometry" /></mxCell>
    <mxCell id="102" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#e1d5e7;strokeColor=#9673a6;" value="IDispatchRuntime" vertex="1"><mxGeometry x="200" y="120" width="220" height="50" as="geometry" /></mxCell>
    <mxCell id="103" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#fff2cc;strokeColor=#d6b656;" value="DispatcherPool (host threads)" vertex="1"><mxGeometry x="200" y="200" width="220" height="50" as="geometry" /></mxCell>
    <mxCell id="104" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="Dispatcher (lane 0)" vertex="1"><mxGeometry x="470" y="0" width="180" height="50" as="geometry" /></mxCell>
    <mxCell id="105" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="Dispatcher (lane 1)" vertex="1"><mxGeometry x="470" y="60" width="180" height="50" as="geometry" /></mxCell>
    <mxCell id="106" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="Dispatcher (lane N-1)" vertex="1"><mxGeometry x="470" y="120" width="180" height="50" as="geometry" /></mxCell>
    <mxCell id="110" edge="1" parent="1" source="100" target="101" style="endArrow=classic;html=1;" value="tick / createProcess"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="111" edge="1" parent="1" source="101" target="102" style="endArrow=classic;html=1;" value="injected"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="112" edge="1" parent="1" source="102" target="103" style="endArrow=block;html=1;dashed=1;" value="implements (parallel)"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="113" edge="1" parent="1" source="101" target="104" style="endArrow=classic;html=1;dashed=1;" value="lane ref"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="114" edge="1" parent="1" source="101" target="105" style="endArrow=classic;html=1;dashed=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="115" edge="1" parent="1" source="101" target="106" style="endArrow=classic;html=1;dashed=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Синхронизация: два слоя

Все примитивы реализуют `ISyncPrimitive` и обязательно сообщают о своём слое через `layer()`:

```cpp
enum class SyncLayer : std::uint8_t
{
  KernelInternal,
  SimulatedResource,
};

class ISyncPrimitive
{
  public:
  [[nodiscard]] virtual Result<void> acquire(ProcessId pid) = 0;
  [[nodiscard]] virtual Result<void> release(ProcessId pid) = 0;
  [[nodiscard]] virtual Result<void> tryAcquire(ProcessId pid) = 0;
  [[nodiscard]] virtual std::string_view name() const noexcept = 0;
  [[nodiscard]] virtual SyncLayer layer() const noexcept = 0;
};
```

Различие принципиально:

- `SimulatedResource` — это «учебные» примитивы (`Mutex`, `Semaphore`, `CriticalSection`),
    которые видны как объект симуляции и попадают в граф ожиданий процессов;
- `KernelInternal` — это блокировки самого ядра/рантайма (упорядоченное взятие хост-локов),
    которые попадают в отдельный lock-order граф.

`Mutex`/`Semaphore` дополнительно учитывают priority-inheritance/boost, чтобы предотвратить
priority inversion в моделируемых сценариях.

## Двойное обнаружение дедлоков

`DeadlockDetector` поддерживает оба слоя:

- симулируемый wait-for граф процессов и ресурсов (`onAcquire/onRelease/onWait`,
    `hasDeadlock`, `getDeadlockedProcesses`);
- lock-order граф внутренних блокировок ядра (`onInternalLockAcquire/...Release`,
    `hasInternalLockOrderCycle`);
- классический Банкер для проверки безопасности состояния:

```cpp
[[nodiscard]] bool isSafeState(
  const std::vector<ResourceAllocation> &current,
  const std::vector<ResourceAllocation> &maximum,
  const std::vector<std::uint32_t> &available
) const;
```

Thread-aware варианты `onAcquire/onRelease/onWait(...)` принимают `ThreadToken`, что позволяет
строить корректные графы при работе нескольких lane-ов одновременно.

### Схема: два графа дедлоков

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="120" parent="1" style="rounded=1;whiteSpace=wrap;html=1;dashed=1;fillColor=#f5f5f5;strokeColor=#666666;" value="Simulated Wait-For Graph (процессы и ресурсы)" vertex="1"><mxGeometry x="40" y="40" width="320" height="220" as="geometry" /></mxCell>
    <mxCell id="121" parent="120" style="ellipse;whiteSpace=wrap;html=1;fillColor=#dae8fc;" value="P1" vertex="1"><mxGeometry x="20" y="60" width="50" height="40" as="geometry" /></mxCell>
    <mxCell id="122" parent="120" style="ellipse;whiteSpace=wrap;html=1;fillColor=#d5e8d4;" value="R1" vertex="1"><mxGeometry x="130" y="60" width="50" height="40" as="geometry" /></mxCell>
    <mxCell id="123" parent="120" style="ellipse;whiteSpace=wrap;html=1;fillColor=#dae8fc;" value="P2" vertex="1"><mxGeometry x="240" y="60" width="50" height="40" as="geometry" /></mxCell>
    <mxCell id="124" parent="120" style="ellipse;whiteSpace=wrap;html=1;fillColor=#d5e8d4;" value="R2" vertex="1"><mxGeometry x="130" y="140" width="50" height="40" as="geometry" /></mxCell>
    <mxCell id="125" edge="1" parent="120" source="121" target="122" style="endArrow=classic;html=1;" value="waits"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="126" edge="1" parent="120" source="122" target="123" style="endArrow=classic;html=1;" value="held by"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="127" edge="1" parent="120" source="123" target="124" style="endArrow=classic;html=1;" value="waits"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="128" edge="1" parent="120" source="124" target="121" style="endArrow=classic;html=1;" value="held by"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="130" parent="1" style="rounded=1;whiteSpace=wrap;html=1;dashed=1;fillColor=#f5f5f5;strokeColor=#666666;" value="Internal Lock-Order Graph (kernel-internal)" vertex="1"><mxGeometry x="400" y="40" width="300" height="220" as="geometry" /></mxCell>
    <mxCell id="131" parent="130" style="ellipse;whiteSpace=wrap;html=1;fillColor=#fff2cc;" value="L1" vertex="1"><mxGeometry x="40" y="80" width="50" height="40" as="geometry" /></mxCell>
    <mxCell id="132" parent="130" style="ellipse;whiteSpace=wrap;html=1;fillColor=#fff2cc;" value="L2" vertex="1"><mxGeometry x="150" y="80" width="50" height="40" as="geometry" /></mxCell>
    <mxCell id="133" parent="130" style="ellipse;whiteSpace=wrap;html=1;fillColor=#fff2cc;" value="L3" vertex="1"><mxGeometry x="100" y="160" width="50" height="40" as="geometry" /></mxCell>
    <mxCell id="134" edge="1" parent="130" source="131" target="132" style="endArrow=classic;html=1;" value="acquired before"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="135" edge="1" parent="130" source="132" target="133" style="endArrow=classic;html=1;" value="acquired before"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="136" edge="1" parent="130" source="133" target="131" style="endArrow=classic;html=1;strokeColor=#b85450;" value="cycle"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Трассировка: `ITracer`, sink-и, `TraceScope`

`ITracer` определяет минимальный контракт: `trace(event)`, `pushScope/popScope`, `currentDepth`,
управление уровнем (`setMinLevel/minLevel`) и доступ к `IClock`. Реализации:

- `Tracer` — активная реализация, пишет в `ITraceSink`;
- `NullTracer` — все методы no-op-ы для Release-сборки/тестов;
- `TraceScope` — RAII-обёртка для `pushScope/popScope`;
- макросы `CONTUR_TRACE_SCOPE` / `CONTUR_TRACE` компилируются в no-op, если флаг
    `CONTUR_TRACE_ENABLED` не определён.

Sink-и: `ConsoleSink` (stdout), `FileSink` (файл), `BufferSink` (in-memory; используется в TUI
для live-просмотра и в тестах для проверки последовательности событий).

В многопоточном режиме `TraceEvent` несёт дополнительные метаданные: worker id, sequence,
epoch. Они позволяют восстановить корректный порядок событий между lane-ами и обеспечивают
воспроизводимость в детерминированном режиме.

### Схема: компоненты трассировки

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="140" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="Subsystems (CPU, MMU, Scheduler, IPC...)" vertex="1"><mxGeometry x="40" y="40" width="300" height="50" as="geometry" /></mxCell>
    <mxCell id="141" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="ITracer" vertex="1"><mxGeometry x="380" y="40" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="142" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="Tracer (active)" vertex="1"><mxGeometry x="540" y="0" width="180" height="40" as="geometry" /></mxCell>
    <mxCell id="143" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="NullTracer (no-op)" vertex="1"><mxGeometry x="540" y="60" width="180" height="40" as="geometry" /></mxCell>
    <mxCell id="144" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#fff2cc;strokeColor=#d6b656;" value="ITraceSink" vertex="1"><mxGeometry x="380" y="160" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="145" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#f8cecc;strokeColor=#b85450;" value="ConsoleSink" vertex="1"><mxGeometry x="540" y="120" width="160" height="40" as="geometry" /></mxCell>
    <mxCell id="146" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#f8cecc;strokeColor=#b85450;" value="FileSink" vertex="1"><mxGeometry x="540" y="170" width="160" height="40" as="geometry" /></mxCell>
    <mxCell id="147" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#f8cecc;strokeColor=#b85450;" value="BufferSink (TUI / tests)" vertex="1"><mxGeometry x="540" y="220" width="200" height="40" as="geometry" /></mxCell>
    <mxCell id="150" edge="1" parent="1" source="140" target="141" style="endArrow=classic;html=1;" value="CONTUR_TRACE_SCOPE / trace()"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="151" edge="1" parent="1" source="142" target="141" style="endArrow=block;html=1;dashed=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="152" edge="1" parent="1" source="143" target="141" style="endArrow=block;html=1;dashed=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="153" edge="1" parent="1" source="142" target="144" style="endArrow=classic;html=1;" value="write(event)"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="154" edge="1" parent="1" source="145" target="144" style="endArrow=block;html=1;dashed=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="155" edge="1" parent="1" source="146" target="144" style="endArrow=block;html=1;dashed=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="156" edge="1" parent="1" source="147" target="144" style="endArrow=block;html=1;dashed=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Источники кода, использованные в отчёте

- `src/include/contur/dispatch/i_dispatcher.h`
- `src/include/contur/dispatch/dispatcher.h` (+ `.cpp`)
- `src/include/contur/dispatch/mp_dispatcher.h` (+ `.cpp`)
- `src/include/contur/dispatch/i_dispatch_runtime.h`
- `src/include/contur/dispatch/serial_dispatch_runtime.h` (+ `.cpp`)
- `src/include/contur/dispatch/dispatcher_pool.h` (+ `.cpp`)
- `src/include/contur/dispatch/threading_config.h`
- `src/include/contur/sync/i_sync_primitive.h`
- `src/include/contur/sync/mutex.h` (+ `.cpp`)
- `src/include/contur/sync/semaphore.h` (+ `.cpp`)
- `src/include/contur/sync/critical_section.h` (+ `.cpp`)
- `src/include/contur/sync/deadlock_detector.h` (+ `.cpp`)
- `src/include/contur/tracing/i_tracer.h`
- `src/include/contur/tracing/tracer.h` (+ `.cpp`)
- `src/include/contur/tracing/null_tracer.h`
- `src/include/contur/tracing/trace_scope.h`
- `src/include/contur/tracing/trace_event.h`
- `src/include/contur/tracing/trace_sink.h`
- `src/include/contur/tracing/console_sink.h` (+ `.cpp`)
- `src/include/contur/tracing/file_sink.h` (+ `.cpp`)
- `src/include/contur/tracing/buffer_sink.h` (+ `.cpp`)

## Критерии готовности

- Описана многопоточная композиция: `MPDispatcher` + инжектируемый `IDispatchRuntime` +
    `DispatcherPool`, читающий `HostThreadingConfig`.
- Зафиксирован двухслойный контракт синхронизации (`SyncLayer`).
- Показана двойная модель дедлоков (wait-for процессов и lock-order ядра).
- Описан слой трассировки и его zero-cost-форма (`NullTracer`, `CONTUR_TRACE_*`-макросы).

## Краткие выводы

Диспетчер, синхронизация и трассировка вместе формируют управляющий и наблюдательный слой
ядра. Диспетчер и рантайм разнесены по разным контрактам — N=1 и N>1 живут в одной кодовой
базе без скрытых режимов. Синхронизация разделена на «учебный» и «внутренний» слои, и
обнаружение дедлоков работает на обоих графах одновременно. Трассировка устроена как нулевая
стоимость по умолчанию и как многоступенчатый Observer при включённой сборке — этого
достаточно, чтобы TUI визуализировал ядро в реальном времени без модификаций самих подсистем.

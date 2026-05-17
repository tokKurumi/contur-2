# Отчет 4. Планирование процессов

## Покрываемые стадии

- Стадия 6: Scheduling

## Контекст и цель

После того как в проекте были сформированы память, процесс, CPU и интерпретатор, следующим шагом стало добавление управления очередностью исполнения. На этом этапе Contur 2 получает полноценный слой планирования: ядро больше не принимает решение о порядке выполнения напрямую, а делегирует выбор стратегии объекту политики.

Это важно по двум причинам. Во-первых, разные алгоритмы планирования можно сравнивать на одном и том же наборе процессов без изменения ядра. Во-вторых, сам `Scheduler` становится не набором разрозненных if-ов, а стабильным хостом для политик, статистики и очередей состояний.

Цель отчета состоит в том, чтобы:

- показать, как работает контракт `ISchedulingPolicy`;
- описать все семь реализованных стратегий планирования;
- объяснить, как `Scheduler` управляет ready, blocked и running состояниями;
- показать роль статистики в предсказании CPU burst time.

Иллюстрация для вставки в Word:

- Рисунок: сравнительный график ожидания и отклика для семи алгоритмов.

## Что обязательно описать

1. Контракт `ISchedulingPolicy` и его методы `selectNext`, `shouldPreempt`.
2. Краткая теория всех 7 алгоритмов.
3. Как `Scheduler` управляет очередями состояний.
4. Как рассчитываются статистики и предсказания burst-time.

## Контракт планирования

В проекте планирование построено как стратегия. Политика получает неизменяемый снимок процессов в ready-очереди и решает, кого выбрать следующим и должен ли текущий running-процесс быть вытеснен.

```cpp
class ISchedulingPolicy
{
    public:
    virtual ~ISchedulingPolicy() = default;

    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    [[nodiscard]] virtual ProcessId
    selectNext(const std::vector<SchedulingProcessSnapshot> &readyQueue, const IClock &clock) const = 0;

    [[nodiscard]] virtual bool shouldPreempt(
        const SchedulingProcessSnapshot &running, const SchedulingProcessSnapshot &candidate, const IClock &clock
    ) const = 0;
};
```

Ключевая идея здесь — работа не с `PCB`, а со снимком процесса. Это убирает прямую связанность политики с внутренним состоянием ядра и упрощает тестирование: для проверки алгоритма достаточно подготовить набор `SchedulingProcessSnapshot` и подать его в policy.

### Снимок процесса для политик

```cpp
struct SchedulingProcessSnapshot
{
    ProcessId pid = INVALID_PID;
    Tick arrivalTime = 0;
    Tick lastStateChange = 0;
    Tick estimatedBurst = 0;
    Tick remainingBurst = 0;
    Tick totalWaitTime = 0;
    PriorityLevel effectivePriority = PriorityLevel::Normal;
    std::int32_t nice = NICE_DEFAULT;
};
```

В этом снимке есть все параметры, которые нужны для принятия решения: время поступления, накопленное ожидание, предсказанный burst, оставшееся время и эффективный приоритет.

### Схема: Strategy pattern

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="80" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="Scheduler" vertex="1"><mxGeometry x="70" y="90" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="81" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="ISchedulingPolicy" vertex="1"><mxGeometry x="270" y="90" width="150" height="50" as="geometry" /></mxCell>
    <mxCell id="82" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="FCFS" vertex="1"><mxGeometry x="470" y="40" width="100" height="40" as="geometry" /></mxCell>
    <mxCell id="83" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="RoundRobin" vertex="1"><mxGeometry x="470" y="90" width="100" height="40" as="geometry" /></mxCell>
    <mxCell id="84" parent="1" style="rounded=1;whiteSpace=wrap;html=1;" value="MLFQ" vertex="1"><mxGeometry x="470" y="140" width="100" height="40" as="geometry" /></mxCell>
    <mxCell id="85" edge="1" parent="1" source="80" target="81" style="endArrow=classic;html=1;" value="has"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="86" edge="1" parent="1" source="82" target="81" style="endArrow=block;html=1;dashed=1;" value="implements"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="87" edge="1" parent="1" source="83" target="81" style="endArrow=block;html=1;dashed=1;" value="implements"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="88" edge="1" parent="1" source="84" target="81" style="endArrow=block;html=1;dashed=1;" value="implements"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Теория алгоритмов

### FCFS

First Come First Served — базовый не вытесняющий алгоритм. Процесс выполняется в порядке поступления, и текущий running-процесс не прерывается из-за появления нового кандидата. Эта стратегия проста, предсказуема и удобна для начального сравнения, но может приводить к convoy effect, когда длинный процесс задерживает все остальные.

### SPN

Shortest Process Next выбирает процесс с минимальным предсказанным burst-time. В отличие от FCFS, здесь уже используется статистическая оценка длительности вычислительного участка. Алгоритм остается не вытесняющим, поэтому хорошо показывает, как предсказание burst влияет на очередь без добавления preemption.

### SRT

Shortest Remaining Time — вытесняющая версия подхода SPN. Если в ready-очередь приходит процесс с меньшим remaining burst, он может вытеснить текущий running-процесс. Это уменьшает среднее время ожидания коротких задач, но требует более частого пересмотра решения.

### HRRN

Highest Response Ratio Next учитывает не только burst, но и время ожидания. Выбор делается по отношению `(waiting + service) / service`, поэтому задача, которая долго ждала, постепенно поднимается в приоритете. Это полезно для снижения starvation.

### Priority

Приоритетное планирование выбирает процесс с лучшим effective priority. В проекте приоритет может быть динамическим и зависеть от внутренних характеристик процесса, в том числе от `nice`. Такая схема удобна для моделирования систем, где часть задач должна получать более высокий уровень обслуживания.

### Round Robin

Round Robin делит процессорное время на фиксированные кванты. Когда процесс исчерпывает свой time slice, он вытесняется и возвращается в конец очереди ready. Это дает более ровный отклик для интерактивных задач и обеспечивает понятную справедливость по времени.

### MLFQ

Multilevel Feedback Queue строит несколько уровней очередей с разными квантами времени. Процесс может переходить между уровнями в зависимости от поведения: короткие и интерактивные задачи быстрее обслуживаются на верхних уровнях, а CPU-bound процессы постепенно опускаются ниже. Это самый адаптивный алгоритм из набора.

### Сравнение по назначению

- FCFS и SPN дают простую модель без вытеснения.
- SRT и Round Robin лучше подходят для справедливого распределения времени.
- HRRN уменьшает голодание за счет учета времени ожидания.
- Priority моделирует обслуживание по относительной важности.
- MLFQ объединяет несколько принципов в одной адаптивной политике.

Иллюстрация для вставки в Word:

- Рисунок: Gantt-диаграмма для одного набора процессов.

## Scheduler и очереди состояний

`Scheduler` является контейнером для очередей и host-слоем для политики. Именно он хранит процессы, переводит их между состояниями и вызывает policy для выбора следующего кандидата.

```cpp
class Scheduler final : public IScheduler
{
    public:
    explicit Scheduler(std::unique_ptr<ISchedulingPolicy> policy, ITracer &tracer);
    ~Scheduler() override;

    [[nodiscard]] Result<void> enqueue(PCB &pcb, Tick currentTick) override;
    [[nodiscard]] Result<void> dequeue(ProcessId pid) override;
    [[nodiscard]] Result<ProcessId> selectNext(const IClock &clock) override;
    [[nodiscard]] Result<void> blockRunning(Tick currentTick) override;
    [[nodiscard]] Result<void> blockProcess(ProcessId pid, Tick currentTick) override;
    [[nodiscard]] Result<void> unblock(ProcessId pid, Tick currentTick) override;
    [[nodiscard]] Result<void> terminate(ProcessId pid, Tick currentTick) override;

    [[nodiscard]] std::vector<ProcessId> getQueueSnapshot() const override;
    [[nodiscard]] std::vector<ProcessId> getBlockedSnapshot() const override;

    [[nodiscard]] Result<void> configureLanes(std::size_t laneCount) override;
    [[nodiscard]] std::size_t laneCount() const noexcept override;
    [[nodiscard]] Result<void> enqueueToLane(PCB &pcb, std::size_t laneIndex, Tick currentTick) override;
    [[nodiscard]] Result<ProcessId> selectNextForLane(std::size_t laneIndex, const IClock &clock) override;
    [[nodiscard]] Result<ProcessId> stealNextForLane(std::size_t thiefLane, const IClock &clock) override;
    [[nodiscard]] std::vector<std::vector<ProcessId>> getPerLaneQueueSnapshot() const override;
    [[nodiscard]] std::vector<ProcessId> runningProcesses() const override;

    [[nodiscard]] Result<void> setPolicy(std::unique_ptr<ISchedulingPolicy> policy) override;
    [[nodiscard]] std::string_view policyName() const noexcept override;
};
```

Внутреннее состояние scheduler строится вокруг нескольких контейнеров:

```cpp
std::unordered_map<ProcessId, std::reference_wrapper<PCB>> processes;
std::unordered_map<ProcessId, std::size_t> processLane;
std::unordered_map<ProcessId, Tick> runStart;
std::vector<std::vector<std::reference_wrapper<PCB>>> readyByLane;
std::vector<std::reference_wrapper<PCB>> blocked;
std::vector<std::optional<std::reference_wrapper<PCB>>> runningByLane;
```

Это позволяет хранить в одном месте полный жизненный цикл процесса:

- ready-очередь содержит процессы, готовые к исполнению;
- blocked-очередь содержит процессы, ожидающие события или I/O;
- runningByLane хранит текущий процесс в каждом слоте исполнения;
- `processLane` фиксирует, на какой lane привязан процесс;
- `runStart` нужен для подсчета длительности CPU burst.

### Как выбирается следующий процесс

Работа `selectNextForLane` проходит по устойчивой схеме:

1. `Scheduler` собирает снимок ready-очереди.
2. Передает снимок в `policy->selectNext(...)`.
3. Проверяет, найден ли процесс в локальной очереди.
4. Если lane уже занят, вызывает `policy->shouldPreempt(...)`.
5. При вытеснении переводит running-процесс обратно в ready и фиксирует burst.
6. Назначает нового running-процесса и запоминает время начала его исполнения.

Таким образом, политика отвечает только за критерий выбора, а `Scheduler` отвечает за корректность переходов между состояниями и сохранение статистики.

### Схема: Gantt-шаблон

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="90" parent="1" style="shape=swimlane;whiteSpace=wrap;html=1;" value="CPU Timeline" vertex="1"><mxGeometry x="40" y="70" width="640" height="120" as="geometry" /></mxCell>
    <mxCell id="91" parent="90" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;" value="P1" vertex="1"><mxGeometry x="40" y="40" width="100" height="40" as="geometry" /></mxCell>
    <mxCell id="92" parent="90" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;" value="P2" vertex="1"><mxGeometry x="150" y="40" width="80" height="40" as="geometry" /></mxCell>
    <mxCell id="93" parent="90" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;" value="P3" vertex="1"><mxGeometry x="240" y="40" width="120" height="40" as="geometry" /></mxCell>
    <mxCell id="94" parent="90" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#f8cecc;" value="P1" vertex="1"><mxGeometry x="370" y="40" width="90" height="40" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Статистика и burst-time

Для алгоритмов, которые ориентируются на длительность вычислительного участка, проект использует класс `Statistics`. Он хранит историю bursts и вычисляет прогноз по EWMA.

```cpp
class Statistics
{
    public:
    explicit Statistics(double alpha = 0.5);
    ~Statistics();

    void recordBurst(ProcessId pid, Tick burst);
    [[nodiscard]] Tick predictedBurst(ProcessId pid) const noexcept;
    [[nodiscard]] bool hasPrediction(ProcessId pid) const noexcept;
    void clear(ProcessId pid);
    void reset();
    [[nodiscard]] double alpha() const noexcept;
};
```

Формула EWMA здесь задается как:

$$
\hat{B}_{n+1} = \alpha B_n + (1 - \alpha) \hat{B}_n
$$

где $B_n$ — наблюдаемый burst, а $\hat{B}_n$ — прошлое предсказание. Реализация использует таблицу предсказаний по `ProcessId`; при первом наблюдении значение просто сохраняется, а далее плавно уточняется.

Такая модель особенно важна для SPN, SRT и MLFQ, поскольку именно она определяет, насколько точно политика сможет оценить, какой процесс надо выбрать следующим.

### Как статистика включается в работу Scheduler

Когда процесс перестает быть running, `Scheduler` фиксирует длительность его выполнения и передает эту длительность в `Statistics::recordBurst(...)`. В результате следующий выбор опирается уже не только на статические поля процесса, но и на накопленную историю его поведения.

## Проверка и воспроизводимость

Для проверки планировщика используется стандартный сценарий сборки и тестирования:

```bash
bash src/build.sh debug src
ctest --preset debug --output-on-failure
```

Что должно подтверждаться тестами:

- корректность `selectNext` и `shouldPreempt` у всех политик;
- корректная работа `Scheduler` при переводе процессов между ready, blocked и running;
- устойчивость lane-based логики и work stealing;
- обновление burst statistics после завершения кванта или CPU burst;
- предсказуемое поведение политики на повторяемом наборе процессов.

Иллюстрация для вставки в Word:

- Скриншот тестов с результатами для scheduling policy и state transitions.

## Источники кода, использованные в отчете

- `src/include/contur/scheduling/i_scheduling_policy.h`
- `src/include/contur/scheduling/i_scheduler.h`
- `src/include/contur/scheduling/scheduler.h`
- `src/contur/scheduling/scheduler.cpp`
- `src/include/contur/scheduling/fcfs_policy.h`
- `src/contur/scheduling/fcfs_policy.cpp`
- `src/include/contur/scheduling/spn_policy.h`
- `src/contur/scheduling/spn_policy.cpp`
- `src/include/contur/scheduling/srt_policy.h`
- `src/contur/scheduling/srt_policy.cpp`
- `src/include/contur/scheduling/hrrn_policy.h`
- `src/contur/scheduling/hrrn_policy.cpp`
- `src/include/contur/scheduling/priority_policy.h`
- `src/contur/scheduling/priority_policy.cpp`
- `src/include/contur/scheduling/round_robin_policy.h`
- `src/contur/scheduling/round_robin_policy.cpp`
- `src/include/contur/scheduling/mlfq_policy.h`
- `src/contur/scheduling/mlfq_policy.cpp`
- `src/include/contur/scheduling/statistics.h`
- `src/contur/scheduling/statistics.cpp`

## Критерии готовности

- В тексте есть связка «архитектурное решение -> контракт -> поведение алгоритма».
- Описаны все 7 политик с их ключевыми отличиями.
- Показано, как `Scheduler` управляет ready, blocked и running состояниями.
- Объяснено, как работает multi-lane схема и где применяется work stealing.
- Раскрыта роль `Statistics` и EWMA-предсказания burst-time.
- Схемы расположены рядом с соответствующими смысловыми разделами.

## Краткие выводы

Этап scheduling превращает систему из набора исполняющих компонентов в управляемую среду, где поведение процессора определяется формализованной политикой. Разделение на `Scheduler`, `ISchedulingPolicy` и `Statistics` дает одновременно гибкость и проверяемость: ядро можно переключать между FCFS, SRT, Round Robin или MLFQ без изменения архитектуры остальной системы.

На этом уровне проект получает базу для анализа компромиссов между справедливостью, откликом и пропускной способностью, а также для дальнейшего расширения планировщика без нарушения существующих контрактов.

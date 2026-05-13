#import "@preview/smk-sto-004:0.1.0": *
#import "@preview/fletcher:0.5.8" as fletcher: diagram, edge, node
#import "_meta.typ": *

#show: lab-report.with(
  institute: institute,
  department: department,
  work-number: 4,
  discipline: discipline,
  title: "Планирование процессов: семь стратегий и инфраструктура очередей",
  author: author,
  supervisor: supervisor,
  designation: designation,
)

= Цель работы

Реализовать в Contur 2 универсальную инфраструктуру планирования: единый интерфейс
`ISchedulingPolicy`, семь сменяемых стратегий выбора процесса, центральный `IScheduler`,
управляющий очередями состояний и полосами (lane-ами), а также модуль `Statistics`
прогнозирования CPU-burst. Полученный слой обеспечивает корректное планирование как в
однопоточном режиме (`N = 1`), так и при использовании пула host-потоков (`N > 1`).

Задачи работы:

+ описать контракт `ISchedulingPolicy` со снимочной семантикой;
+ реализовать семь стратегий: FCFS, RR, SPN, SRT, HRRN, Priority, MLFQ;
+ реализовать `Scheduler` с ready/blocked-очередями и lane-ами;
+ реализовать прогноз CPU-burst через EWMA в `Statistics`;
+ предусмотреть в контракте `IScheduler` операции межlane-переноса (`stealNextForLane`).

= Реализация

== Контракт стратегии планирования

Стратегии работают исключительно со снимками состояния. Это жёсткий контрактный инвариант:
политика не владеет блокировками и не мутирует общий стейт; она получает иммутабельный
снимок ready-очереди и часы, и возвращает решение о следующем процессе.

```cpp
struct SchedulingProcessSnapshot {
    ProcessId pid;
    Tick arrivalTime, lastStateChange;
    Tick estimatedBurst, remainingBurst, totalWaitTime;
    PriorityLevel effectivePriority;
    std::int32_t nice;
};

class ISchedulingPolicy {
    public:
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    [[nodiscard]] virtual ProcessId selectNext(
        const std::vector<SchedulingProcessSnapshot> &readyQueue, const IClock &clock) const = 0;

    [[nodiscard]] virtual bool shouldPreempt(
        const SchedulingProcessSnapshot &running,
        const SchedulingProcessSnapshot &candidate,
        const IClock &clock) const = 0;
};
```

Контракт `Strategy` гарантирует три свойства: `selectNext` — чистая функция от снимка и
часов; `shouldPreempt` — чистая функция от двух снимков; стратегия взаимозаменяема без
изменений в остальном ядре. Иерархия классов показана на рисунке @fig:strategy.

#figure(
  diagram(
    spacing: (1.8cm, 0.7cm),
    node-inset: 6pt,
    node((0, 3), [Scheduler], fill: rgb("#dae8fc")),
    node((1, 3), [ISchedulingPolicy], fill: rgb("#d5e8d4")),
    node((2, 0), [FcfsPolicy], fill: rgb("#ffe6cc")),
    node((2, 1), [RoundRobinPolicy], fill: rgb("#ffe6cc")),
    node((2, 2), [SpnPolicy], fill: rgb("#ffe6cc")),
    node((2, 3), [SrtPolicy], fill: rgb("#ffe6cc")),
    node((2, 4), [HrrnPolicy], fill: rgb("#ffe6cc")),
    node((2, 5), [PriorityPolicy], fill: rgb("#ffe6cc")),
    node((2, 6), [MlfqPolicy], fill: rgb("#ffe6cc")),
    edge((0, 3), (1, 3), "->", [uses]),
    edge((2, 0), (1, 3), "-->"),
    edge((2, 1), (1, 3), "-->"),
    edge((2, 2), (1, 3), "-->"),
    edge((2, 3), (1, 3), "-->"),
    edge((2, 4), (1, 3), "-->"),
    edge((2, 5), (1, 3), "-->"),
    edge((2, 6), (1, 3), "-->"),
  ),
  caption: [Strategy-паттерн на семи стратегиях планирования],
) <fig:strategy>

== Семь стратегий

Все стратегии работают с одним и тем же снимком ready-очереди. Их семантика и учебная роль
сведены в таблице @tab:policies.

#figure(
  table(
    columns: 3,
    align: (left, left, left),
    [Стратегия], [Поведение], [Учебная роль],
    [FCFS],
    [Берёт процесс с наименьшим `arrivalTime`; без вытеснения.],
    [Показывает эффект «convoy» на длинных задачах.],

    [Round Robin],
    [Циркулярный обход; вытеснение по истечении кванта.],
    [Базовая equitable-стратегия для интерактивных систем.],

    [SPN], [Выбирает процесс с наименьшим `estimatedBurst`.], [Минимизирует среднее ожидание при точном прогнозе.],
    [SRT],
    [Preemptive-вариант SPN: короткий новый процесс вытесняет текущий.],
    [Дружелюбность к коротким приходящим задачам.],

    [HRRN],
    [Выбор по соотношению `(wait + burst) / burst`.],
    [Балансирует ожидание длинных задач и приоритет коротких.],

    [Priority], [Сортировка по `effectivePriority` и `nice`.], [Демонстрирует уровни приоритета и nice-сдвиги.],
    [MLFQ],
    [Несколько уровней очередей; понижение приоритета при исчерпании кванта.],
    [Учебный эталон general-purpose планировщиков.],
  ),
  caption: [Семантика и применимость стратегий планирования],
) <tab:policies>

== Очереди и lane-ы планировщика

`IScheduler` управляет ready-очередью (одной или несколькими lane-ами), общей
blocked-очередью и running-set по lane-ам. Ключевые методы — `enqueue`/`dequeue`,
`selectNext`, `blockProcess`/`unblock`, `terminate`, а также семейство `enqueueToLane`,
`selectNextForLane`, `stealNextForLane`, `getPerLaneQueueSnapshot`. Lane-ы и
work-stealing — часть основного контракта планировщика: они одинаково работают при `N = 1`
и при `N > 1`, без отдельных режимов в клиентском коде. Перемещение процесса между
очередями показано на рисунке @fig:scheduler-queues.

#figure(
  diagram(
    spacing: (4.8cm, 2.6cm),
    node-inset: 8pt,
    node((0, 0), [Lane 0: $[P_1, P_3, ...]$], fill: rgb("#dae8fc")),
    node((1, 0), [Lane 1: $[P_2, ...]$], fill: rgb("#dae8fc")),
    node((0, 1), [Running set], fill: rgb("#d5e8d4")),
    node((1, 1), [Running set], fill: rgb("#d5e8d4")),
    node((0.5, 2), [Blocked queue], fill: rgb("#f8cecc")),
    edge((0, 0), (0, 1), "->", [selectNextForLane], label-side: right, label-sep: 1.2em),
    edge((1, 0), (1, 1), "->", [selectNextForLane], label-side: left, label-sep: 1.2em),
    edge((0, 1), (0.5, 2), "->", [blockProcess], label-side: right, label-sep: 0.8em),
    edge((1, 1), (0.5, 2), "->", [blockProcess], label-side: left, label-sep: 0.8em),
    edge((0.5, 2), (0, 0), "-->", [unblock], bend: 110deg, label-side: left, label-sep: 0.8em, label-pos: 0.5),
    edge((0, 0), (1, 0), "-->", [stealNextForLane], bend: -55deg, label-sep: 0.6em),
  ),
  caption: [Движение процесса между очередями планировщика],
) <fig:scheduler-queues>

== Прогноз CPU-burst

Стратегии SPN и SRT опираются на поле `estimatedBurst` в снимке процесса. Это значение
поддерживается классом `Statistics`, который ведёт экспоненциально взвешенное скользящее
среднее (EWMA) с коэффициентом сглаживания $alpha$:

$ "predict"_(n+1) = alpha dot "burst"_n + (1 - alpha) dot "predict"_n $ <eq:ewma>

```cpp
class Statistics {
    public:
    explicit Statistics(double alpha = 0.5);

    void recordBurst(ProcessId pid, Tick burst);
    [[nodiscard]] Tick predictedBurst(ProcessId pid) const noexcept;
    [[nodiscard]] bool hasPrediction(ProcessId pid) const noexcept;
};
```

Формула @eq:ewma не требует хранения истории и достаточно точна для образовательной
демонстрации SPN/SRT.

= Заключение

Планирование в Contur 2 устроено по паттерну Strategy: любой из семи алгоритмов выбирается
во время сборки ядра и не требует изменений в остальном коде. Lane-ы и work-stealing уже
заложены в контракт `IScheduler`, что обеспечивает однородное поведение в режимах `N = 1` и
`N > 1`. Прогноз CPU-burst через EWMA обеспечивает работу SPN- и SRT-стратегий без хранения
истории и подготавливает почву для адаптивной диспетчеризации.

= Список использованных источников

+ `src/include/contur/scheduling/i_scheduling_policy.h`
+ `src/include/contur/scheduling/i_scheduler.h`
+ `src/include/contur/scheduling/scheduler.h`
+ `src/contur/scheduling/scheduler.cpp`
+ `src/include/contur/scheduling/fcfs_policy.h`
+ `src/include/contur/scheduling/round_robin_policy.h`
+ `src/include/contur/scheduling/spn_policy.h`
+ `src/include/contur/scheduling/srt_policy.h`
+ `src/include/contur/scheduling/hrrn_policy.h`
+ `src/include/contur/scheduling/priority_policy.h`
+ `src/include/contur/scheduling/mlfq_policy.h`
+ `src/include/contur/scheduling/statistics.h`
+ `src/contur/scheduling/statistics.cpp`

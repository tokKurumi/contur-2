# Отчет 4. Планирование процессов

## Охват этапов

- Этап 6: Scheduling

## Контекст и цель

Планирование — это место, где архитектура «процессы как данные» встречается с архитектурой
«стратегия как объект». Все семь алгоритмов планирования живут за одним интерфейсом
`ISchedulingPolicy`, который возвращает следующий PID и решение о вытеснении. `Scheduler` —
сам по себе единая инфраструктура очередей состояний и lane-ов; он не знает, по какому правилу
выбран следующий процесс.

Цель отчёта:

- зафиксировать контракт `ISchedulingPolicy` и его отделение от очередей;
- кратко описать семь реализованных стратегий и их применимость;
- показать, как `Scheduler` управляет ready/blocked-очередями, lane-ами и работой между ними
    (включая work-stealing на уровне планировщика);
- объяснить роль `Statistics` для прогноза CPU-burst (используется в SPN/SRT).

## Контракт `ISchedulingPolicy`

Политики работают исключительно со снимками состояния. Это жёсткий контрактный инвариант:
политика не владеет блокировками и не мутирует общий стейт; она получает иммутабельный
снимок ready-очереди и часы, и возвращает решение.

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

class ISchedulingPolicy
{
  public:
  virtual ~ISchedulingPolicy() = default;

  [[nodiscard]] virtual std::string_view name() const noexcept = 0;

  [[nodiscard]] virtual ProcessId
  selectNext(const std::vector<SchedulingProcessSnapshot> &readyQueue, const IClock &clock) const = 0;

  [[nodiscard]] virtual bool shouldPreempt(
      const SchedulingProcessSnapshot &running,
      const SchedulingProcessSnapshot &candidate,
      const IClock &clock
  ) const = 0;
};
```

Такой контракт обеспечивает три инварианта:

1. `selectNext` — чистая функция от снимка очереди и часов;
2. `shouldPreempt` — чистая функция от двух снимков;
3. Политика взаимозаменяема без влияния на остальное ядро (Strategy + DIP).

## Семь стратегий: семантика и применимость

Все семь стратегий реализованы как отдельные классы. Все они получают одинаковый снимок
ready-очереди и часы.

| Политика | Класс | Поведение | Когда полезна |
|---|---|---|---|
| FCFS — First Come First Served | `FcfsPolicy` | Берёт процесс с наименьшим `arrivalTime`. Без вытеснения. | Простейший показательный baseline; видна проблема «convoy effect». |
| Round Robin | `RoundRobinPolicy` | Циркулярный обход, фиксированный time slice (тиков); вытеснение по истечении quantum. | Базовая equitable-стратегия — фон большинства интерактивных систем. |
| SPN — Shortest Process Next | `SpnPolicy` | Берёт процесс с наименьшим `estimatedBurst`. Без вытеснения. | Минимизирует среднее ожидание, если оценка burst-а точная. |
| SRT — Shortest Remaining Time | `SrtPolicy` | Аналог SPN, но preemptive: новый короткий процесс вытесняет текущий. | Тот же эффект, что у SPN, но дружелюбнее к коротким приходящим задачам. |
| HRRN — Highest Response Ratio Next | `HrrnPolicy` | Выбирает по соотношению `(wait + burst) / burst`. Без вытеснения. | Балансирует ожидание длинных задач против преимущества коротких. |
| Priority (динамический) | `PriorityPolicy` | Сортировка по `effectivePriority` с учётом `nice`. | Демонстрирует системы с уровнями приоритета и nice-сдвигами. |
| MLFQ — Multilevel Feedback Queue | `MlfqPolicy` | Несколько уровней очередей с понижением приоритета процесса при исчерпании quantum. | Учебный эталон современных general-purpose планировщиков. |

### Схема: Strategy-паттерн на все 7 политик

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="80" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="Scheduler (IScheduler)" vertex="1"><mxGeometry x="40" y="120" width="180" height="60" as="geometry" /></mxCell>
    <mxCell id="81" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="ISchedulingPolicy" vertex="1"><mxGeometry x="270" y="120" width="180" height="60" as="geometry" /></mxCell>
    <mxCell id="82" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="FcfsPolicy" vertex="1"><mxGeometry x="510" y="20" width="140" height="35" as="geometry" /></mxCell>
    <mxCell id="83" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="RoundRobinPolicy" vertex="1"><mxGeometry x="510" y="60" width="160" height="35" as="geometry" /></mxCell>
    <mxCell id="84" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="SpnPolicy" vertex="1"><mxGeometry x="510" y="100" width="140" height="35" as="geometry" /></mxCell>
    <mxCell id="85" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="SrtPolicy" vertex="1"><mxGeometry x="510" y="140" width="140" height="35" as="geometry" /></mxCell>
    <mxCell id="86" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="HrrnPolicy" vertex="1"><mxGeometry x="510" y="180" width="140" height="35" as="geometry" /></mxCell>
    <mxCell id="87" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="PriorityPolicy" vertex="1"><mxGeometry x="510" y="220" width="140" height="35" as="geometry" /></mxCell>
    <mxCell id="88" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="MlfqPolicy" vertex="1"><mxGeometry x="510" y="260" width="140" height="35" as="geometry" /></mxCell>
    <mxCell id="90" edge="1" parent="1" source="80" target="81" style="endArrow=classic;html=1;" value="uses"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="91" edge="1" parent="1" source="82" target="81" style="endArrow=block;html=1;dashed=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="92" edge="1" parent="1" source="83" target="81" style="endArrow=block;html=1;dashed=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="93" edge="1" parent="1" source="84" target="81" style="endArrow=block;html=1;dashed=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="94" edge="1" parent="1" source="85" target="81" style="endArrow=block;html=1;dashed=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="95" edge="1" parent="1" source="86" target="81" style="endArrow=block;html=1;dashed=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="96" edge="1" parent="1" source="87" target="81" style="endArrow=block;html=1;dashed=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="97" edge="1" parent="1" source="88" target="81" style="endArrow=block;html=1;dashed=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Scheduler: очереди состояний и lane-ы

`IScheduler` управляет тремя видами состояний:

- ready-очередь (одна или несколько lane-ов для многопроцессорной модели);
- blocked-очередь (общая, межпроцессная);
- running-set (текущие исполняющиеся процессы по lane-ам).

Ключевые методы фасада — `enqueue/dequeue/selectNext`, `blockProcess/unblock`, `terminate`,
`enqueueToLane/selectNextForLane/stealNextForLane`, `getPerLaneQueueSnapshot`. Lane-ы и
work-stealing — часть основного контракта планировщика: они одинаково работают в режиме N=1
и при N>1, так что переключение между однопоточным и многопоточным рантаймом не требует
правок в кодовых клиентах.

### Схема: переход процесса между очередями `Scheduler`

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="100" parent="1" style="rounded=1;whiteSpace=wrap;html=1;dashed=1;fillColor=#f5f5f5;strokeColor=#666666;" value="Ready lanes" vertex="1"><mxGeometry x="40" y="40" width="280" height="170" as="geometry" /></mxCell>
    <mxCell id="101" parent="100" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="Lane 0: [P1, P3, ...]" vertex="1"><mxGeometry x="15" y="40" width="245" height="40" as="geometry" /></mxCell>
    <mxCell id="102" parent="100" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="Lane 1: [P2, ...]" vertex="1"><mxGeometry x="15" y="100" width="245" height="40" as="geometry" /></mxCell>
    <mxCell id="103" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="Running set per lane" vertex="1"><mxGeometry x="380" y="80" width="200" height="60" as="geometry" /></mxCell>
    <mxCell id="104" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#f8cecc;strokeColor=#b85450;" value="Blocked queue" vertex="1"><mxGeometry x="640" y="80" width="160" height="60" as="geometry" /></mxCell>
    <mxCell id="105" edge="1" parent="1" source="101" target="103" style="endArrow=classic;html=1;" value="selectNextForLane"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="106" edge="1" parent="1" source="102" target="103" style="endArrow=classic;html=1;" value="selectNextForLane"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="107" edge="1" parent="1" source="103" target="104" style="endArrow=classic;html=1;" value="blockProcess"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="108" edge="1" parent="1" source="104" target="101" style="endArrow=classic;html=1;dashed=1;" value="unblock(...)"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="109" edge="1" parent="1" source="101" target="102" style="endArrow=classic;html=1;dashed=1;" value="stealNextForLane"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Прогноз CPU-burst через `Statistics`

`SpnPolicy` и `SrtPolicy` опираются на `estimatedBurst` в снимке. Это значение поддерживается
классом `Statistics`, который ведёт экспоненциально взвешенное скользящее среднее (EWMA) с
коэффициентом `alpha`. Расчёт прост: при каждом наблюдении `burst` обновляется прогноз
`predicted = alpha * burst + (1 - alpha) * predicted_prev`.

```cpp
class Statistics
{
  public:
  explicit Statistics(double alpha = 0.5);

  void recordBurst(ProcessId pid, Tick burst);
  [[nodiscard]] Tick predictedBurst(ProcessId pid) const noexcept;
  [[nodiscard]] bool hasPrediction(ProcessId pid) const noexcept;

  void clear(ProcessId pid);
  void reset();
  [[nodiscard]] double alpha() const noexcept;
};
```

Это та же модель, что в учебнике Tanenbaum для прогноза burst-а: дешёвая, без истории, и
достаточная для образовательной демонстрации SPN/SRT.

## Шаблон Gantt-диаграммы (для конкретного набора процессов)

Дальше Gantt-блоки заполняются конкретными значениями в зависимости от выбранной политики
и приходов процессов. Шаблон ниже — пример вёрстки timeline в draw.io.

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="90" parent="1" style="shape=swimlane;whiteSpace=wrap;html=1;" value="CPU Timeline (один пример)" vertex="1"><mxGeometry x="40" y="70" width="640" height="120" as="geometry" /></mxCell>
    <mxCell id="91" parent="90" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;" value="P1" vertex="1"><mxGeometry x="40" y="40" width="100" height="40" as="geometry" /></mxCell>
    <mxCell id="92" parent="90" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;" value="P2" vertex="1"><mxGeometry x="150" y="40" width="80" height="40" as="geometry" /></mxCell>
    <mxCell id="93" parent="90" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;" value="P3" vertex="1"><mxGeometry x="240" y="40" width="120" height="40" as="geometry" /></mxCell>
    <mxCell id="94" parent="90" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#f8cecc;" value="P1" vertex="1"><mxGeometry x="370" y="40" width="90" height="40" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Источники кода, использованные в отчёте

- `src/include/contur/scheduling/i_scheduling_policy.h`
- `src/include/contur/scheduling/i_scheduler.h`
- `src/include/contur/scheduling/scheduler.h`
- `src/contur/scheduling/scheduler.cpp`
- `src/include/contur/scheduling/fcfs_policy.h` (+ `.cpp`)
- `src/include/contur/scheduling/round_robin_policy.h` (+ `.cpp`)
- `src/include/contur/scheduling/spn_policy.h` (+ `.cpp`)
- `src/include/contur/scheduling/srt_policy.h` (+ `.cpp`)
- `src/include/contur/scheduling/hrrn_policy.h` (+ `.cpp`)
- `src/include/contur/scheduling/priority_policy.h` (+ `.cpp`)
- `src/include/contur/scheduling/mlfq_policy.h` (+ `.cpp`)
- `src/include/contur/scheduling/statistics.h` (+ `.cpp`)

## Критерии готовности

- Дан контракт `ISchedulingPolicy` и обоснована его «pure»-семантика на снимках.
- Перечислены все семь политик и зафиксирована их учебная роль.
- Описано, как `IScheduler` управляет ready/blocked-очередями и lane-ами (для готовности к N>1).
- Описана модель прогноза burst через EWMA в `Statistics` и её связь с SPN/SRT.

## Краткие выводы

Планирование в Contur 2 выстроено так, что любой алгоритм — это одна заменяемая стратегия
внутри одинаковой инфраструктуры очередей. Это даёт два дивиденда: учебное сравнение семи
политик происходит «честно», на одних и тех же снимках; а многопоточный рантайм работает
поверх того же `IScheduler`, потому что lane-ы и work-stealing уже зашиты в его контракт.

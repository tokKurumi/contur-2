#import "@preview/smk-sto:0.3.1": *
#import "@preview/fletcher:0.5.8" as fletcher: diagram, edge, node
#import "_meta.typ": *

#show: lab-report.with(
  institute: institute,
  department: department,
  work-number: 2,
  discipline: discipline,
  title: "Подсистема памяти и модель процесса",
  author: author,
  supervisor: supervisor,
  designation: designation,
)

= Цель работы

Реализовать в симуляторе Contur 2 полноценную подсистему памяти с виртуальной адресацией,
страничным замещением и поддержкой свопа, а также целостную объектную модель процесса с
валидируемыми переходами состояний. Эти две подсистемы образуют слой, поверх которого
выполняются интерпретатор, планировщик и диспетчер.

Задачи работы:

+ описать контракты `IMemory`, `IMMU`, `IVirtualMemory`, `IPageReplacementPolicy`;
+ реализовать таблицу страниц `PageTable` с флагами `present`, `dirty`, `referenced`;
+ реализовать `Mmu` с поддержкой выделения кадров, трансляции и свопа;
+ реализовать четыре стратегии замещения страниц: FIFO, LRU, Clock, Optimal;
+ описать `PCB`, `ProcessImage` и валидатор переходов жизненного цикла процесса.

= Реализация

== Контракты подсистемы памяти

Базовый контракт линейной памяти задаёт `IMemory`: чтение и запись `Block`, размер
адресного пространства и операция очистки. Поверх `IMemory` строится `IMMU`, отвечающий за
виртуальную адресацию, выделение/освобождение кадров и страничный своп:

```cpp
class IMMU {
    public:
    [[nodiscard]] virtual Result<Block> read(ProcessId pid, MemoryAddress va) const = 0;
    [[nodiscard]] virtual Result<void> write(ProcessId pid, MemoryAddress va, const Block &b) = 0;

    [[nodiscard]] virtual Result<MemoryAddress> allocate(ProcessId pid, std::size_t pages) = 0;
    [[nodiscard]] virtual Result<void> deallocate(ProcessId pid) = 0;

    [[nodiscard]] virtual Result<void> swapIn(ProcessId pid, MemoryAddress va) = 0;
    [[nodiscard]] virtual Result<void> swapOut(ProcessId pid, MemoryAddress va) = 0;

    [[nodiscard]] virtual std::size_t totalFrames() const noexcept = 0;
    [[nodiscard]] virtual std::size_t freeFrames() const noexcept = 0;
};
```

Слой `IVirtualMemory` поднимается на уровень слотов процесса (`allocate/loadSegment`),
предоставляя удобный фасад для диспетчера. Структура слоёв показана на рисунке
@fig:memory-layers.

#figure(
  diagram(
    spacing: (2.0cm, 1.6cm),
    node-inset: 8pt,
    node((0, 0), [Execution / CPU], fill: rgb("#dae8fc")),
    node((1, 0), [IMMU / Mmu], fill: rgb("#d5e8d4")),
    node((2, -1), [PageTable], fill: rgb("#ffe6cc")),
    node((2, 1), [IMemory], fill: rgb("#f8cecc")),
    node((1, 2), [IVirtualMemory], fill: rgb("#e1d5e7")),
    edge((0, 0), (1, 0), "->", [read/write]),
    edge((1, 0), (2, -1), "->", [translate], label-side: right),
    edge((1, 0), (2, 1), "->", [frame I/O], label-side: left),
    edge((1, 2), (1, 0), "->", [allocate], label-side: left),
  ),
  caption: [Слои подсистемы памяти и их взаимодействие],
) <fig:memory-layers>

== Таблица страниц и MMU

`PageTable` хранит флаги отображения для каждой виртуальной страницы:

```cpp
struct PageTableEntry {
    FrameId frameId = INVALID_FRAME;
    bool present = false;
    bool dirty = false;
    bool referenced = false;
};
```

Операции `map`, `unmap`, `translate`, `setReferenced`, `setDirty` поддерживают
работу MMU и стратегий замещения. Сам `Mmu` хранит:

```cpp
std::unordered_map<ProcessId, PageTable> pageTables;
std::unordered_set<FrameId> freeFrames;
std::unordered_map<FrameId, ProcessId> frameOwners;
std::unordered_map<SwapKey, Block, SwapKeyHash> swapSpace;
```

При `allocate(...)` MMU пытается взять свободный кадр; при его отсутствии — обращается к
стратегии замещения и вытесняет жертву в `swapSpace`. На время первичного выделения для
процесса MMU отдельно запрещает вытеснять страницы того же процесса, исключая «самоэвикцию»
и ошибку уже на первом обращении. Последовательность операций при чтении показана на
рисунке @fig:mmu-read.

#figure(
  diagram(
    spacing: (2.4cm, 1.2cm),
    node-inset: 8pt,
    node((0, 0), [read(pid, va)], fill: rgb("#dae8fc")),
    node((1, 0), [PageTable.translate], fill: rgb("#d5e8d4")),
    node((1, 1), [setReferenced + onAccess], fill: rgb("#ffe6cc")),
    node((0, 1), [IMemory.read(frame)], fill: rgb("#f8cecc")),
    edge((0, 0), (1, 0), "->"),
    edge((1, 0), (1, 1), "->", [frame], label-side: left),
    edge((1, 1), (0, 1), "->"),
  ),
  caption: [Последовательность операций чтения через MMU],
) <fig:mmu-read>

== Стратегии замещения страниц

Все стратегии реализуют общий контракт `IPageReplacementPolicy` с методами `selectVictim`,
`onAccess`, `onLoad`, `reset`. Сравнительные характеристики четырёх реализованных
алгоритмов приведены в таблице @tab:replacement.

#figure(
  table(
    columns: 3,
    align: (left, center, center),
    [Алгоритм], [Относительные page faults], [Стоимость сопровождения],
    [FIFO], [высокие], [низкая],
    [LRU], [средние], [средняя],
    [Clock], [средние], [низкая (бит обращения)],
    [Optimal], [минимальные (нижняя граница)], [неприменим на практике],
  ),
  caption: [Качественное сравнение стратегий замещения страниц],
) <tab:replacement>

Учебная роль каждой реализации:

- `FifoReplacement` — простейший baseline, демонстрирует эффект «convoy»;
- `LruReplacement` — практический компромисс качества и стоимости;
- `ClockReplacement` — approximate-LRU с минимальными накладными расходами;
- `OptimalReplacement` — теоретический эталон для сравнения промахов.

== Модель процесса

Объектная модель процесса в проекте складывается из трёх типов. `ProcessState` —
перечисление шести состояний, `Priority` — структура из базового, эффективного приоритета и
значения `nice`, а `PCB` инкапсулирует идентификатор, имя, состояние, приоритет, тайминги и
адресные метаданные процесса.

```cpp
enum class ProcessState : std::uint8_t {
    New, Ready, Running, Blocked, Suspended, Terminated,
};

[[nodiscard]] constexpr bool isValidTransition(ProcessState from, ProcessState to) noexcept;
```

Метод `PCB::setState(...)` обновляет состояние только при допустимом переходе и одновременно
ведёт учёт времени: каждое изменение состояния увеличивает соответствующий счётчик
(`totalCpuTime`, `totalWaitTime`, `totalBlockedTime`). Жизненный цикл процесса изображён на
рисунке @fig:proc-lifecycle.

#figure(
  diagram(
    spacing: (2.2cm, 1.8cm),
    node-shape: fletcher.shapes.ellipse,
    node-inset: 7pt,
    node((0, 0), [New]),
    node((0, 1), [Suspended]),
    node((1, 0), [Ready]),
    node((2, 0), [Running]),
    node((3, 0), [Terminated]),
    node((2, 1), [Blocked]),
    edge((0, 0), (1, 0), "->", [admit]),
    edge((1, 0), (2, 0), "->", [dispatch], bend: 30deg),
    edge((2, 0), (1, 0), "->", [preempt], bend: 30deg),
    edge((2, 0), (3, 0), "->", [exit]),
    edge((2, 0), (2, 1), "->", [wait I/O], label-side: left),
    edge((2, 1), (1, 0), "->", [event], bend: 30deg, label-side: left),
    edge((1, 0), (0, 1), "->", [swap out], bend: -20deg, label-side: left),
    edge((0, 1), (1, 0), "->", [swap in], bend: -20deg, label-side: left),
    edge((2, 1), (0, 1), "->", [swap out], bend: 30deg, label-side: right),
  ),
  caption: [Жизненный цикл процесса и допустимые переходы],
) <fig:proc-lifecycle>

Образ процесса `ProcessImage` объединяет `PCB`, регистровый файл и сегмент кода:

```cpp
struct ProcessImage::Impl {
    PCB pcb;
    RegisterFile registers;
    std::vector<Block> code;
};
```

Таким образом одна сущность инкапсулирует всё необходимое для диспетчеризации, исполнения и
сохранения контекста при переключении процессов.

= Заключение <s>

Подсистема памяти разделена на три уровня — линейная память, MMU с таблицей страниц и
управление виртуальными слотами процесса; четыре стратегии замещения подключаются по
единому контракту. Модель процесса представлена связкой `PCB` + `ProcessImage` с валидацией
переходов и накоплением таймингов. На полученных интерфейсах строится исполнительная часть
ядра, рассмотренная в следующем отчёте.
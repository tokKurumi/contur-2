# Отчет 2. Память и процессы

## Охват этапов

- Этап 2: Memory Subsystem
- Этап 3: Process Model

## Контекст и цель

После формирования архитектурного фундамента следующий шаг в Contur 2 связан с двумя базовыми сущностями операционной системы: памятью и процессом. На этом уровне проект получает полноценную модель виртуального адресного пространства, таблицы страниц, стратегии замещения, а также формализованный жизненный цикл процесса с проверяемыми переходами состояний.

Ключевая задача этапов 2-3 состоит в том, чтобы соединить:

- безопасный доступ к памяти через интерфейсы и `Resпult<T>`;
- изоляцию процессов через отдельные адресные пространства;
- модель процесса, пригодную для диспетчеризации и планирования.

## Подсистема памяти: интерфейсы и роли

Архитектура памяти строится слоями. Нижний слой — линейная физическая память, верхний — виртуальные слоты процесса, а между ними — MMU с трансляцией и swap-механикой.

Базовый контракт хранения блоков задается `IMemory`:

```cpp
class IMemory
{
  public:
  virtual ~IMemory() = default;
  [[nodiscard]] virtual Result<Block> read(MemoryAddress address) const = 0;
  [[nodiscard]] virtual Result<void> write(MemoryAddress address, const Block &block) = 0;
  [[nodiscard]] virtual std::size_t size() const noexcept = 0;
  virtual void clear() = 0;
};
```

Контракт управления виртуальной памятью задается `IMMU`:

```cpp
class IMMU
{
  public:
  virtual ~IMMU() = default;

  [[nodiscard]] virtual Result<Block> read(ProcessId processId, MemoryAddress virtualAddress) const = 0;
  [[nodiscard]] virtual Result<void> write(ProcessId processId, MemoryAddress virtualAddress, const Block &block) = 0;

  [[nodiscard]] virtual Result<MemoryAddress> allocate(ProcessId processId, std::size_t pageCount) = 0;
  [[nodiscard]] virtual Result<void> deallocate(ProcessId processId) = 0;

  [[nodiscard]] virtual Result<void> swapIn(ProcessId processId, MemoryAddress virtualAddress) = 0;
  [[nodiscard]] virtual Result<void> swapOut(ProcessId processId, MemoryAddress virtualAddress) = 0;
};
```

Иллюстрация для документа:

- Логическая схема «CPU -> MMU -> IMemory» с пометками `read/write/translate`.

### Схема: слои подсистемы памяти (контрактная)

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" />
    <mxCell id="1" parent="0" />
    <mxCell id="10" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="Execution / CPU" vertex="1">
      <mxGeometry x="40" y="70" width="140" height="60" as="geometry" />
    </mxCell>
    <mxCell id="11" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="IMMU / Mmu" vertex="1">
      <mxGeometry x="240" y="70" width="140" height="60" as="geometry" />
    </mxCell>
    <mxCell id="12" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="PageTable" vertex="1">
      <mxGeometry x="440" y="30" width="130" height="60" as="geometry" />
    </mxCell>
    <mxCell id="13" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#f8cecc;strokeColor=#b85450;" value="IMemory" vertex="1">
      <mxGeometry x="440" y="110" width="130" height="60" as="geometry" />
    </mxCell>
    <mxCell id="14" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#e1d5e7;strokeColor=#9673a6;" value="IVirtualMemory" vertex="1">
      <mxGeometry x="240" y="180" width="140" height="60" as="geometry" />
    </mxCell>
    <mxCell id="20" edge="1" parent="1" source="10" target="11" style="endArrow=classic;html=1;" value="read/write">
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
    <mxCell id="21" edge="1" parent="1" source="11" target="12" style="endArrow=classic;html=1;" value="translate">
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
    <mxCell id="22" edge="1" parent="1" source="11" target="13" style="endArrow=classic;html=1;" value="frame IO">
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
    <mxCell id="23" edge="1" parent="1" source="14" target="11" style="endArrow=classic;html=1;" value="allocate/loadSegment">
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
  </root>
</mxGraphModel>
```

## Реализация MMU: трансляция, выделение, swap

В реализации `Mmu` сохранены три критические ответственности:

- per-process таблицы страниц (`std::unordered_map<ProcessId, PageTable>`);
- учет свободных кадров (`freeFrames`) и владельцев кадров (`frameOwners`);
- simulated swap space для вытесненных страниц.

Ключевой фрагмент структуры состояния:

```cpp
std::unordered_map<ProcessId, PageTable> pageTables;
std::unordered_set<FrameId> freeFrames;
std::unordered_map<FrameId, ProcessId> frameOwners;
std::unordered_map<SwapKey, Block, SwapKeyHash> swapSpace;
```

При `allocate(...)` MMU сначала пытается взять свободный кадр, а при его отсутствии обращается к стратегии замещения. В коде отдельно учитывается важное ограничение: запрещается вытеснять страницу того же процесса во время его первичного выделения, чтобы не получить «самоэвикцию» и ошибку уже на первом доступе.

Логика доступа при `read(...)`/`write(...)` включает:

- трансляцию страницы через `PageTable::translate(...)`;
- обновление битов `referenced/dirty`;
- уведомление policy через `onAccess(frame)`.

В `swapIn(...)` и `swapOut(...)` присутствует симметричная схема: чтение блока кадра, запись в `swapSpace`, `unmap`/`map`, перенос владельца кадра и обновление policy.

### Схема: последовательность `read` через MMU

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" />
    <mxCell id="1" parent="0" />
    <mxCell id="30" parent="1" style="shape=process;whiteSpace=wrap;html=1;" value="read(pid, vaddr)" vertex="1">
      <mxGeometry x="40" y="90" width="140" height="50" as="geometry" />
    </mxCell>
    <mxCell id="31" parent="1" style="shape=process;whiteSpace=wrap;html=1;" value="PageTable.translate" vertex="1">
      <mxGeometry x="230" y="90" width="150" height="50" as="geometry" />
    </mxCell>
    <mxCell id="32" parent="1" style="shape=process;whiteSpace=wrap;html=1;" value="setReferenced + onAccess" vertex="1">
      <mxGeometry x="430" y="90" width="170" height="50" as="geometry" />
    </mxCell>
    <mxCell id="33" parent="1" style="shape=process;whiteSpace=wrap;html=1;" value="IMemory.read(frame)" vertex="1">
      <mxGeometry x="650" y="90" width="150" height="50" as="geometry" />
    </mxCell>
    <mxCell id="34" edge="1" parent="1" source="30" target="31" style="endArrow=classic;html=1;" value="">
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
    <mxCell id="35" edge="1" parent="1" source="31" target="32" style="endArrow=classic;html=1;" value="frame">
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
    <mxCell id="36" edge="1" parent="1" source="32" target="33" style="endArrow=classic;html=1;" value="">
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
  </root>
</mxGraphModel>
```

## Таблица страниц и виртуальные слоты

`PageTable` хранит флаги, критичные для корректной работы MMU и policy:

```cpp
struct PageTableEntry
{
  FrameId frameId = INVALID_FRAME;
  bool present = false;
  bool dirty = false;
  bool referenced = false;
};
```

Поведение ключевых методов:

- `map(virtualPage, frame)` — связывает виртуальную страницу с физическим кадром и ставит `present=true`;
- `unmap(...)` — снимает отображение;
- `translate(...)` — возвращает `FrameId` или ошибку (`InvalidAddress`/`SegmentationFault`);
- `setReferenced(...)`, `setDirty(...)` — поддерживают диагностику и page-replacement.

Слой `VirtualMemory` поднимает абстракцию до слотов процесса:

```cpp
auto allocResult = impl_->mmu.allocate(processId, size);
impl_->slots[processId] = SlotInfo{processId, size};
```

Таким образом `VirtualMemory` управляет выделением и загрузкой сегментов, а детали трансляции и кадров остаются внутри MMU.

Иллюстрация для документа:

- Схема «виртуальные страницы процесса -> физические кадры -> swap» с выделением `present` и `dirty`.

### Схема: виртуальные страницы процесса -> физические кадры -> swap

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" />
    <mxCell id="1" parent="0" />

    <mxCell id="70" parent="1" style="rounded=1;whiteSpace=wrap;html=1;dashed=1;fillColor=#f5f5f5;strokeColor=#666666;" value="PageTable (process P)" vertex="1">
      <mxGeometry x="40" y="40" width="260" height="220" as="geometry" />
    </mxCell>

    <mxCell id="71" parent="70" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="VP 0 -> F 2 | present=1, dirty=0" vertex="1">
      <mxGeometry x="20" y="30" width="220" height="40" as="geometry" />
    </mxCell>
    <mxCell id="72" parent="70" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="VP 1 -> F 5 | present=1, dirty=1" vertex="1">
      <mxGeometry x="20" y="80" width="220" height="40" as="geometry" />
    </mxCell>
    <mxCell id="73" parent="70" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="VP 2 -> none | present=0 (swapped)" vertex="1">
      <mxGeometry x="20" y="130" width="220" height="50" as="geometry" />
    </mxCell>

    <mxCell id="74" parent="1" style="rounded=1;whiteSpace=wrap;html=1;dashed=1;fillColor=#f5f5f5;strokeColor=#666666;" value="Physical Frames" vertex="1">
      <mxGeometry x="360" y="40" width="230" height="220" as="geometry" />
    </mxCell>

    <mxCell id="75" parent="74" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="Frame 2: code/data VP0" vertex="1">
      <mxGeometry x="20" y="30" width="190" height="40" as="geometry" />
    </mxCell>
    <mxCell id="76" parent="74" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="Frame 5: code/data VP1" vertex="1">
      <mxGeometry x="20" y="80" width="190" height="40" as="geometry" />
    </mxCell>
    <mxCell id="77" parent="74" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffffff;strokeColor=#999999;" value="Frame 8: free" vertex="1">
      <mxGeometry x="20" y="130" width="190" height="40" as="geometry" />
    </mxCell>

    <mxCell id="78" parent="1" style="rounded=1;whiteSpace=wrap;html=1;dashed=1;fillColor=#f5f5f5;strokeColor=#666666;" value="Swap Space" vertex="1">
      <mxGeometry x="650" y="70" width="180" height="130" as="geometry" />
    </mxCell>
    <mxCell id="79" parent="78" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#f8cecc;strokeColor=#b85450;" value="(P, VP2) -> swapped block" vertex="1">
      <mxGeometry x="15" y="40" width="150" height="40" as="geometry" />
    </mxCell>

    <mxCell id="80" edge="1" parent="1" source="71" target="75" style="endArrow=classic;html=1;" value="translate">
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
    <mxCell id="81" edge="1" parent="1" source="72" target="76" style="endArrow=classic;html=1;" value="translate">
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
    <mxCell id="82" edge="1" parent="1" source="73" target="79" style="endArrow=classic;html=1;dashed=1;" value="swapOut / swapIn">
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
  </root>
</mxGraphModel>
```

## Стратегии замещения страниц: FIFO, LRU, Clock, Optimal

Контракт replacement policy единый:

```cpp
class IPageReplacementPolicy
{
  public:
  virtual ~IPageReplacementPolicy() = default;
  [[nodiscard]] virtual std::string_view name() const noexcept = 0;
  [[nodiscard]] virtual FrameId selectVictim(const PageTable &pageTable) = 0;
  virtual void onAccess(FrameId frame) = 0;
  virtual void onLoad(FrameId frame) = 0;
  virtual void reset() = 0;
};
```

Смысл алгоритмов:

- FIFO: вытесняет самый ранний загруженный кадр;
- LRU: вытесняет наименее недавно использованный кадр;
- Clock: второй шанс по биту обращения;
- Optimal: учебный эталон с известной наперед последовательностью обращений.

Практическая интерпретация в контексте лабораторной:

- FIFO проще и показателен для базовой демонстрации;
- LRU ближе к реальному рабочему компромиссу;
- Clock показывает, как approximate-LRU достигается дешевле;
- Optimal нужен как теоретическая нижняя граница для сравнения промахов страниц.

### Схема: сравнительный график алгоритмов замещения (page faults и стоимость)

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" />
    <mxCell id="1" parent="0" />

    <mxCell id="40" parent="1" style="rounded=1;whiteSpace=wrap;html=1;dashed=1;fillColor=#f5f5f5;strokeColor=#666666;" value="Относительные page faults (меньше лучше)" vertex="1">
      <mxGeometry x="40" y="30" width="390" height="290" as="geometry" />
    </mxCell>

    <mxCell id="41" parent="1" style="shape=line;strokeWidth=2;strokeColor=#444444;" vertex="1">
      <mxGeometry x="70" y="275" width="330" height="1" as="geometry" />
    </mxCell>
    <mxCell id="42" parent="1" style="shape=line;strokeWidth=2;strokeColor=#444444;" vertex="1">
      <mxGeometry x="70" y="70" width="1" height="206" as="geometry" />
    </mxCell>

    <mxCell id="43" parent="1" style="rounded=0;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="" vertex="1">
      <mxGeometry x="95" y="95" width="48" height="180" as="geometry" />
    </mxCell>
    <mxCell id="44" parent="1" style="rounded=0;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="" vertex="1">
      <mxGeometry x="170" y="125" width="48" height="150" as="geometry" />
    </mxCell>
    <mxCell id="45" parent="1" style="rounded=0;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="" vertex="1">
      <mxGeometry x="245" y="140" width="48" height="135" as="geometry" />
    </mxCell>
    <mxCell id="46" parent="1" style="rounded=0;whiteSpace=wrap;html=1;fillColor=#f8cecc;strokeColor=#b85450;" value="" vertex="1">
      <mxGeometry x="320" y="200" width="48" height="75" as="geometry" />
    </mxCell>

    <mxCell id="47" parent="1" style="text;html=1;strokeColor=none;fillColor=none;align=center;verticalAlign=middle;" value="FIFO" vertex="1">
      <mxGeometry x="90" y="282" width="58" height="20" as="geometry" />
    </mxCell>
    <mxCell id="48" parent="1" style="text;html=1;strokeColor=none;fillColor=none;align=center;verticalAlign=middle;" value="LRU" vertex="1">
      <mxGeometry x="165" y="282" width="58" height="20" as="geometry" />
    </mxCell>
    <mxCell id="49" parent="1" style="text;html=1;strokeColor=none;fillColor=none;align=center;verticalAlign=middle;" value="Clock" vertex="1">
      <mxGeometry x="238" y="282" width="62" height="20" as="geometry" />
    </mxCell>
    <mxCell id="50" parent="1" style="text;html=1;strokeColor=none;fillColor=none;align=center;verticalAlign=middle;" value="Optimal" vertex="1">
      <mxGeometry x="312" y="282" width="64" height="20" as="geometry" />
    </mxCell>

    <mxCell id="51" parent="1" style="text;html=1;strokeColor=none;fillColor=none;align=left;verticalAlign=middle;" value="выше" vertex="1">
      <mxGeometry x="45" y="80" width="30" height="20" as="geometry" />
    </mxCell>
    <mxCell id="52" parent="1" style="text;html=1;strokeColor=none;fillColor=none;align=left;verticalAlign=middle;" value="ниже" vertex="1">
      <mxGeometry x="45" y="255" width="30" height="20" as="geometry" />
    </mxCell>

    <mxCell id="53" parent="1" style="rounded=1;whiteSpace=wrap;html=1;dashed=1;fillColor=#f5f5f5;strokeColor=#666666;" value="Относительная вычислительная стоимость (меньше лучше)" vertex="1">
      <mxGeometry x="470" y="30" width="390" height="290" as="geometry" />
    </mxCell>

    <mxCell id="54" parent="1" style="shape=line;strokeWidth=2;strokeColor=#444444;" vertex="1">
      <mxGeometry x="500" y="275" width="330" height="1" as="geometry" />
    </mxCell>
    <mxCell id="55" parent="1" style="shape=line;strokeWidth=2;strokeColor=#444444;" vertex="1">
      <mxGeometry x="500" y="70" width="1" height="206" as="geometry" />
    </mxCell>

    <mxCell id="56" parent="1" style="rounded=0;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="" vertex="1">
      <mxGeometry x="525" y="210" width="48" height="65" as="geometry" />
    </mxCell>
    <mxCell id="57" parent="1" style="rounded=0;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="" vertex="1">
      <mxGeometry x="600" y="150" width="48" height="125" as="geometry" />
    </mxCell>
    <mxCell id="58" parent="1" style="rounded=0;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="" vertex="1">
      <mxGeometry x="675" y="125" width="48" height="150" as="geometry" />
    </mxCell>
    <mxCell id="59" parent="1" style="rounded=0;whiteSpace=wrap;html=1;fillColor=#f8cecc;strokeColor=#b85450;" value="" vertex="1">
      <mxGeometry x="750" y="80" width="48" height="195" as="geometry" />
    </mxCell>

    <mxCell id="60" parent="1" style="text;html=1;strokeColor=none;fillColor=none;align=center;verticalAlign=middle;" value="FIFO" vertex="1">
      <mxGeometry x="520" y="282" width="58" height="20" as="geometry" />
    </mxCell>
    <mxCell id="61" parent="1" style="text;html=1;strokeColor=none;fillColor=none;align=center;verticalAlign=middle;" value="LRU" vertex="1">
      <mxGeometry x="595" y="282" width="58" height="20" as="geometry" />
    </mxCell>
    <mxCell id="62" parent="1" style="text;html=1;strokeColor=none;fillColor=none;align=center;verticalAlign=middle;" value="Clock" vertex="1">
      <mxGeometry x="668" y="282" width="62" height="20" as="geometry" />
    </mxCell>
    <mxCell id="63" parent="1" style="text;html=1;strokeColor=none;fillColor=none;align=center;verticalAlign=middle;" value="Optimal" vertex="1">
      <mxGeometry x="742" y="282" width="64" height="20" as="geometry" />
    </mxCell>

    <mxCell id="64" parent="1" style="text;html=1;strokeColor=none;fillColor=none;align=left;verticalAlign=middle;" value="ниже" vertex="1">
      <mxGeometry x="475" y="255" width="30" height="20" as="geometry" />
    </mxCell>
    <mxCell id="65" parent="1" style="text;html=1;strokeColor=none;fillColor=none;align=left;verticalAlign=middle;" value="выше" vertex="1">
      <mxGeometry x="475" y="80" width="30" height="20" as="geometry" />
    </mxCell>

    <mxCell id="66" parent="1" style="text;html=1;strokeColor=none;fillColor=none;align=center;verticalAlign=middle;whiteSpace=wrap;" value="График иллюстрирует типичный компромисс: меньше page faults обычно требует большей стоимости сопровождения метаданных" vertex="1">
      <mxGeometry x="120" y="330" width="680" height="30" as="geometry" />
    </mxCell>
  </root>
</mxGraphModel>
```

## Модель процесса: состояния, приоритеты, образ процесса

Этап 3 вводит целостную объектную модель процесса через `PCB` и `ProcessImage`.

`ProcessState` и валидатор переходов:

```cpp
enum class ProcessState : std::uint8_t
{
  New,
  Ready,
  Running,
  Blocked,
  Suspended,
  Terminated,
};

[[nodiscard]] constexpr bool isValidTransition(ProcessState from, ProcessState to) noexcept;
```

В `pcb.cpp` переходы дополнительно завязаны на учет времени:

```cpp
Tick elapsed = currentTick - impl_->timing.lastStateChange;
switch (impl_->state)
{
case ProcessState::Running:
  impl_->timing.totalCpuTime += elapsed;
  break;
case ProcessState::Ready:
  impl_->timing.totalWaitTime += elapsed;
  break;
case ProcessState::Blocked:
  impl_->timing.totalBlockedTime += elapsed;
  break;
default:
  break;
}
```

Это означает, что сама смена состояния одновременно становится точкой накопления статистики процесса.

`ProcessImage` объединяет три ключевых компонента:

- `PCB` (метаданные и lifecycle);
- `RegisterFile` (контекст CPU);
- `std::vector<Block>` (код сегмента процесса).

```cpp
struct ProcessImage::Impl
{
  PCB pcb;
  RegisterFile registers;
  std::vector<Block> code;
};
```

Таким образом на уровне одной сущности хранится все, что нужно для диспетчеризации, исполнения и сохранения контекста.

### Схема: структура процесса и связь с памятью

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" />
    <mxCell id="1" parent="0" />
    <mxCell id="50" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="ProcessImage" vertex="1">
      <mxGeometry x="70" y="80" width="170" height="70" as="geometry" />
    </mxCell>
    <mxCell id="51" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="PCB" vertex="1">
      <mxGeometry x="290" y="40" width="120" height="55" as="geometry" />
    </mxCell>
    <mxCell id="52" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="RegisterFile" vertex="1">
      <mxGeometry x="290" y="110" width="120" height="55" as="geometry" />
    </mxCell>
    <mxCell id="53" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#f8cecc;strokeColor=#b85450;" value="Code Segment (Block[])" vertex="1">
      <mxGeometry x="290" y="180" width="170" height="55" as="geometry" />
    </mxCell>
    <mxCell id="54" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#e1d5e7;strokeColor=#9673a6;" value="VirtualMemory/MMU" vertex="1">
      <mxGeometry x="520" y="180" width="160" height="55" as="geometry" />
    </mxCell>
    <mxCell id="55" edge="1" parent="1" source="50" target="51" style="endArrow=diamond;html=1;" value="has">
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
    <mxCell id="56" edge="1" parent="1" source="50" target="52" style="endArrow=diamond;html=1;" value="has">
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
    <mxCell id="57" edge="1" parent="1" source="50" target="53" style="endArrow=diamond;html=1;" value="has">
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
    <mxCell id="58" edge="1" parent="1" source="53" target="54" style="endArrow=classic;html=1;" value="load/read via MMU">
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
  </root>
</mxGraphModel>
```

### Схема: жизненный цикл процесса

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" />
    <mxCell id="1" parent="0" />
    <mxCell id="60" parent="1" style="ellipse;whiteSpace=wrap;html=1;" value="New" vertex="1"><mxGeometry x="60" y="100" width="90" height="50" as="geometry" /></mxCell>
    <mxCell id="61" parent="1" style="ellipse;whiteSpace=wrap;html=1;" value="Ready" vertex="1"><mxGeometry x="210" y="100" width="90" height="50" as="geometry" /></mxCell>
    <mxCell id="62" parent="1" style="ellipse;whiteSpace=wrap;html=1;" value="Running" vertex="1"><mxGeometry x="360" y="100" width="100" height="50" as="geometry" /></mxCell>
    <mxCell id="63" parent="1" style="ellipse;whiteSpace=wrap;html=1;" value="Blocked" vertex="1"><mxGeometry x="360" y="210" width="100" height="50" as="geometry" /></mxCell>
    <mxCell id="64" parent="1" style="ellipse;whiteSpace=wrap;html=1;" value="Suspended" vertex="1"><mxGeometry x="510" y="210" width="110" height="50" as="geometry" /></mxCell>
    <mxCell id="65" parent="1" style="ellipse;whiteSpace=wrap;html=1;" value="Terminated" vertex="1"><mxGeometry x="540" y="100" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="66" edge="1" parent="1" source="60" target="61" style="endArrow=classic;html=1;" value="admit"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="67" edge="1" parent="1" source="61" target="62" style="endArrow=classic;html=1;" value="dispatch"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="68" edge="1" parent="1" source="62" target="61" style="endArrow=classic;html=1;" value="preempt"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="69" edge="1" parent="1" source="62" target="63" style="endArrow=classic;html=1;" value="wait I/O"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="70" edge="1" parent="1" source="63" target="61" style="endArrow=classic;html=1;" value="event"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="71" edge="1" parent="1" source="61" target="64" style="endArrow=classic;html=1;" value="swap out"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="72" edge="1" parent="1" source="63" target="64" style="endArrow=classic;html=1;" value="swap out"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="73" edge="1" parent="1" source="64" target="61" style="endArrow=classic;html=1;" value="swap in"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="74" edge="1" parent="1" source="62" target="65" style="endArrow=classic;html=1;" value="exit"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Проверка и воспроизводимость этапов 2-3

Минимальный сценарий проверки после интеграции memory/process:

```bash
bash src/build.sh debug src
ctest --preset debug --output-on-failure
```

Для этапа памяти подтверждается:

- корректный `map/unmap/translate`;
- возврат `InvalidAddress`/`SegmentationFault` на невалидных обращениях;
- устойчивость MMU при дефиците кадров и swap-переходах.

Для этапа процесса подтверждается:

- корректность допустимых/недопустимых переходов `ProcessState`;
- обновление таймингов в `PCB::setState(...)`;
- целостность `ProcessImage` как контейнера `PCB + RegisterFile + code`.

Иллюстрация для документа:

- Лог тестов memory/process с акцентом на успешные негативные кейсы (ошибочные адреса, недопустимые переходы).

## Источники кода, использованные в отчете

- `src/include/contur/memory/i_memory.h`
- `src/include/contur/memory/i_mmu.h`
- `src/include/contur/memory/page_table.h`
- `src/contur/memory/page_table.cpp`
- `src/contur/memory/mmu.cpp`
- `src/include/contur/memory/i_virtual_memory.h`
- `src/contur/memory/virtual_memory.cpp`
- `src/include/contur/memory/i_page_replacement.h`
- `src/include/contur/memory/fifo_replacement.h`
- `src/include/contur/memory/lru_replacement.h`
- `src/include/contur/memory/clock_replacement.h`
- `src/include/contur/memory/optimal_replacement.h`
- `src/include/contur/process/state.h`
- `src/include/contur/process/pcb.h`
- `src/contur/process/pcb.cpp`
- `src/include/contur/process/process_image.h`
- `src/contur/process/process_image.cpp`

## Критерии готовности

- В тексте раскрыта связка «контракт -> реализация -> наблюдаемое поведение».
- Объяснены роли `IMemory`, `IMMU`, `IVirtualMemory`, `PageTable`.
- Показано смысловое различие FIFO/LRU/Clock/Optimal.
- Дана структурная модель `PCB` и `ProcessImage`.
- Приведена валидация жизненного цикла процесса через `isValidTransition(...)` и `PCB::setState(...)`.

## Краткие выводы

Этапы 2-3 превращают архитектурный каркас в рабочую системную модель: память становится изолированной и управляемой через MMU и таблицы страниц, а процесс получает формализованное состояние, тайминги и контекст исполнения. В результате проект получает стабильный слой, на котором уже можно корректно строить планирование, диспетчеризацию и исполнение программ без нарушения инвариантов памяти и lifecycle-переходов.

# Отчет 1. Архитектурный фундамент

## Охват этапов

- Этап 0: Project Scaffolding
- Этап 1: Foundation (`core/` + `arch/`)

## Контекст и цель

Contur 2 строится как учебный симулятор операционной системы с фокусом на архитектурной прозрачности: каждый слой должен быть понятен, воспроизводим и проверяем тестами. На первых двух этапах формируется технический каркас, на который затем опираются память, CPU, планировщик и ядро.

В рамках этого отчета фиксируются:

- архитектурные принципы, принятые в основании проекта;
- сборочная схема и стандартизированный запуск;
- фундаментальные типы/контракты (`core`, `arch`);
- проверка того, что база действительно воспроизводима.

Иллюстрация для вставки в Word (сразу после этого абзаца):

- Рисунок: дерево репозитория с выделением `src/include/contur`, `src/contur`, `src/app`, `src/tests`.
- Что добавить: скрин структуры директорий из IDE или терминала.

## Этап scaffold: как собран технический каркас

На стадии scaffold проект получил единый вход в сборку через CMake presets и стандартизированный shell-скрипт. Это критично для команды и CI: все разработчики запускают одинаковую цепочку действий.

Фрагмент из `src/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(contur2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CONTUR2_BUILD_TUI "Build TUI layer" ON)
option(CONTUR2_BUILD_APP "Build application executable" ON)
```

Практический эффект:

- фиксируется единый стандарт языка (C++20);
- экспортируется `compile_commands.json` для инструментов анализа;
- сразу задается разделение на библиотеку ядра и пользовательский слой.

### Схема: контейнерная карта компонентов (C4)

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" />
    <mxCell id="1" parent="0" />
    <mxCell id="10" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="contur2 (app)" vertex="1">
      <mxGeometry x="40" y="70" width="150" height="60" as="geometry" />
    </mxCell>
    <mxCell id="11" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="contur2_demos" vertex="1">
      <mxGeometry x="240" y="70" width="150" height="60" as="geometry" />
    </mxCell>
    <mxCell id="12" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="contur2_lib" vertex="1">
      <mxGeometry x="140" y="180" width="150" height="60" as="geometry" />
    </mxCell>
    <mxCell id="13" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#f8cecc;strokeColor=#b85450;" value="tests (GTest)" vertex="1">
      <mxGeometry x="430" y="180" width="150" height="60" as="geometry" />
    </mxCell>
    <mxCell id="20" edge="1" parent="1" source="10" target="12" style="endArrow=classic;html=1;" value="link" >
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
    <mxCell id="21" edge="1" parent="1" source="11" target="12" style="endArrow=classic;html=1;" value="link" >
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
    <mxCell id="22" edge="1" parent="1" source="13" target="12" style="endArrow=classic;html=1;" value="test" >
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
  </root>
</mxGraphModel>
```

Иллюстрация для вставки в Word (сразу после схемы):

- Рисунок: скрин CMake Targets из IDE.
- Что добавить: видимые цели `contur2_lib`, `contur2_tui`, `contur2`, тестовые таргеты.

## Принципы архитектуры, заложенные на фундаменте

В ранней фазе зафиксированы четыре ключевых решения: DIP, PIMPL, `Result<T>`, smart pointers.

### DIP и интерфейсные контракты

Верхние слои зависят от абстракций. Базовый пример — `IClock`:

```cpp
class IClock
{
  public:
  virtual ~IClock() = default;
  [[nodiscard]] virtual Tick now() const noexcept = 0;
  virtual void tick() = 0;
  virtual void reset() = 0;
};
```

Это позволяет менять реализацию времени без переписывания клиентов, а также упрощает модульные тесты.

### PIMPL как компиляционный firewall

```cpp
class SimulationClock final : public IClock
{
  public:
  SimulationClock();
  ~SimulationClock() override;
  SimulationClock(SimulationClock &&) noexcept;
  SimulationClock &operator=(SimulationClock &&) noexcept;

  [[nodiscard]] Tick now() const noexcept override;
  void tick() override;
  void reset() override;

  private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
```

Польза: стабильные публичные заголовки, меньшая перекомпиляция при изменениях внутри `.cpp`.

### Схема: зависимости слоев через интерфейсы

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" />
    <mxCell id="1" parent="0" />
    <mxCell id="31" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="Application / Demos" vertex="1">
      <mxGeometry x="40" y="50" width="180" height="60" as="geometry" />
    </mxCell>
    <mxCell id="32" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="Kernel Services" vertex="1">
      <mxGeometry x="280" y="50" width="160" height="60" as="geometry" />
    </mxCell>
    <mxCell id="33" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="Core Contracts (I*)" vertex="1">
      <mxGeometry x="500" y="50" width="180" height="60" as="geometry" />
    </mxCell>
    <mxCell id="34" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#f8cecc;strokeColor=#b85450;" value="Concrete Implementations" vertex="1">
      <mxGeometry x="500" y="160" width="180" height="60" as="geometry" />
    </mxCell>
    <mxCell id="35" edge="1" parent="1" source="31" target="32" style="endArrow=classic;html=1;" value="uses">
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
    <mxCell id="36" edge="1" parent="1" source="32" target="33" style="endArrow=classic;html=1;" value="depends on interfaces">
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
    <mxCell id="37" edge="1" parent="1" source="34" target="33" style="endArrow=block;html=1;dashed=1;" value="implements">
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
  </root>
</mxGraphModel>
```

Иллюстрация для вставки в Word (сразу после этой схемы):

- Рисунок: фрагмент кода, где объект создается как конкретный класс, но передается как интерфейс.
- Что добавить: скрин из файла и подпись «инверсия зависимости на практике».

### `Result<T>` и формализация ошибок

```cpp
enum class ErrorCode : std::int32_t
{
  Ok = 0,
  OutOfMemory,
  InvalidPid,
  InvalidAddress,
  DivisionByZero,
  InvalidInstruction,
  // ...
  NotImplemented,
};

template <typename T> class [[nodiscard]] Result
{
  public:
  [[nodiscard]] static Result ok(T value);
  [[nodiscard]] static Result error(ErrorCode code);
  [[nodiscard]] bool isOk() const noexcept;
  [[nodiscard]] bool isError() const noexcept;
  [[nodiscard]] const T &value() const &;
  [[nodiscard]] ErrorCode errorCode() const noexcept;
};
```

Такой подход нужен для предсказуемого runtime-потока без неявных исключений в горячих участках.

### Схема: поток обработки результата операции

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" />
    <mxCell id="1" parent="0" />
    <mxCell id="41" parent="1" style="shape=process;whiteSpace=wrap;html=1;" value="Operation" vertex="1">
      <mxGeometry x="40" y="80" width="120" height="50" as="geometry" />
    </mxCell>
    <mxCell id="42" parent="1" style="rhombus;whiteSpace=wrap;html=1;" value="isOk?" vertex="1">
      <mxGeometry x="210" y="75" width="90" height="60" as="geometry" />
    </mxCell>
    <mxCell id="43" parent="1" style="shape=process;whiteSpace=wrap;html=1;fillColor=#d5e8d4;" value="use value()" vertex="1">
      <mxGeometry x="350" y="40" width="120" height="50" as="geometry" />
    </mxCell>
    <mxCell id="44" parent="1" style="shape=process;whiteSpace=wrap;html=1;fillColor=#f8cecc;" value="handle errorCode()" vertex="1">
      <mxGeometry x="350" y="120" width="140" height="50" as="geometry" />
    </mxCell>
    <mxCell id="45" edge="1" parent="1" source="41" target="42" style="endArrow=classic;html=1;" value="Result&lt;T&gt;">
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
    <mxCell id="46" edge="1" parent="1" source="42" target="43" style="endArrow=classic;html=1;" value="yes">
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
    <mxCell id="47" edge="1" parent="1" source="42" target="44" style="endArrow=classic;html=1;" value="no">
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
  </root>
</mxGraphModel>
```

## Этап foundation: что добавили в `core` и `arch`

### Базовый словарь типов

```cpp
using ProcessId = std::uint32_t;
using MemoryAddress = std::uint32_t;
using Tick = std::uint64_t;
using RegisterValue = std::int32_t;

constexpr ProcessId INVALID_PID = 0;
constexpr MemoryAddress NULL_ADDRESS = 0xFFFFFFFF;
constexpr std::uint8_t REGISTER_COUNT = 16;
constexpr Tick DEFAULT_TIME_SLICE = 4;
```

Единая типизация убирает платформенные неоднозначности и делает сигнатуры функций самодокументируемыми.

### Событийная шина наблюдателей

```cpp
template <typename... Args> class Event
{
  public:
  using Callback = std::function<void(Args...)>;
  [[nodiscard]] SubscriptionId subscribe(Callback callback);
  bool unsubscribe(SubscriptionId id);
  void emit(Args... args) const;
  [[nodiscard]] std::size_t subscriberCount() const noexcept;
};
```

Эта конструкция важна для будущей связки «ядро -> трейсинг/статистика/UI» без жесткой связности модулей.

### ISA и модель регистров

```cpp
enum class Instruction : std::uint8_t
{
  Nop = 0,
  Mov,
  Add,
  Sub,
  Mul,
  Div,
  // ...
  Interrupt,
  Halt,
};
```

```cpp
enum class Register : std::uint8_t
{
  R0 = 0,
  // ...
  ProgramCounter = 14,
  StackPointer = 15,
};
```

Перечисление `Instruction` фиксирует базовый набор команд симулируемой архитектуры. В нем уже есть арифметические операции (`Mov`, `Add`, `Sub`, `Mul`, `Div`), логические операции, переходы, работа со стеком, обращения к памяти, программные прерывания и завершение выполнения. Такой набор показывает, что интерпретатор на этом этапе проектируется не как абстрактный «исполнитель команд», а как конкретная модель процессорного контура с понятной семантикой каждой инструкции.

Перечисление `Register` задает регистровый файл из 16 позиций и разделяет общие регистры и специальные системные регистры. Регистры `R0`–`R13` используются как универсальные рабочие ячейки, `ProgramCounter` хранит адрес следующей инструкции, а `StackPointer` указывает вершину стека. Такое деление важно для последующих стадий, потому что сразу формирует основу для исполнения подпрограмм, ветвлений, сохранения контекста и переключения процессов.

Вместе эти два перечисления образуют минимальный архитектурный контракт уровня ISA: набор допустимых команд и пространство, в котором эти команды работают. Именно на этой базе затем строятся CPU, интерпретатор, диспетчер процессов и механизмы сохранения состояния.

### Схема: мини-поток fetch-decode-execute на foundation-уровне

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" />
    <mxCell id="1" parent="0" />
    <mxCell id="51" parent="1" style="shape=process;whiteSpace=wrap;html=1;" value="PC -> fetch" vertex="1">
      <mxGeometry x="40" y="90" width="120" height="50" as="geometry" />
    </mxCell>
    <mxCell id="52" parent="1" style="shape=process;whiteSpace=wrap;html=1;" value="decode opcode" vertex="1">
      <mxGeometry x="200" y="90" width="130" height="50" as="geometry" />
    </mxCell>
    <mxCell id="53" parent="1" style="shape=process;whiteSpace=wrap;html=1;" value="execute ALU/ctrl" vertex="1">
      <mxGeometry x="370" y="90" width="140" height="50" as="geometry" />
    </mxCell>
    <mxCell id="54" parent="1" style="shape=process;whiteSpace=wrap;html=1;" value="update registers" vertex="1">
      <mxGeometry x="550" y="90" width="140" height="50" as="geometry" />
    </mxCell>
    <mxCell id="55" edge="1" parent="1" source="51" target="52" style="endArrow=classic;html=1;" value="">
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
    <mxCell id="56" edge="1" parent="1" source="52" target="53" style="endArrow=classic;html=1;" value="">
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
    <mxCell id="57" edge="1" parent="1" source="53" target="54" style="endArrow=classic;html=1;" value="">
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
  </root>
</mxGraphModel>
```

## Сборочный поток и воспроизводимость

Для воспроизведения состояния после первых этапов используется единый скрипт:

```bash
bash src/build.sh debug src
ctest --preset debug --output-on-failure
```

Актуальный фрагмент `build.sh`, отвечающий за непротиворечивую конфигурацию Conan + CMake:

```bash
conan install "${SOURCE_DIR}/tests" \
    -of "${CONAN_OUTPUT_DIR}" \
    -s build_type="${CONAN_BUILD_TYPE}" \
    -s compiler.cppstd=20 \
    --build=missing

cmake --preset "${PRESET}" \
    -S "${SOURCE_DIR}" \
    -DCMAKE_PREFIX_PATH="${CONAN_GENERATORS_DIR}" \
    -DCMAKE_MODULE_PATH="${CONAN_GENERATORS_DIR}"
```

### Схема: пайплайн сборки

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" />
    <mxCell id="1" parent="0" />
    <mxCell id="30" parent="1" style="shape=process;whiteSpace=wrap;html=1;" value="conan install" vertex="1">
      <mxGeometry x="40" y="80" width="120" height="50" as="geometry" />
    </mxCell>
    <mxCell id="31" parent="1" style="shape=process;whiteSpace=wrap;html=1;" value="cmake --preset" vertex="1">
      <mxGeometry x="200" y="80" width="140" height="50" as="geometry" />
    </mxCell>
    <mxCell id="32" parent="1" style="shape=process;whiteSpace=wrap;html=1;" value="cmake --build" vertex="1">
      <mxGeometry x="380" y="80" width="130" height="50" as="geometry" />
    </mxCell>
    <mxCell id="33" parent="1" style="shape=process;whiteSpace=wrap;html=1;" value="ctest --preset" vertex="1">
      <mxGeometry x="550" y="80" width="130" height="50" as="geometry" />
    </mxCell>
    <mxCell id="34" edge="1" parent="1" source="30" target="31" style="endArrow=classic;html=1;" value="" >
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
    <mxCell id="35" edge="1" parent="1" source="31" target="32" style="endArrow=classic;html=1;" value="" >
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
    <mxCell id="36" edge="1" parent="1" source="32" target="33" style="endArrow=classic;html=1;" value="" >
      <mxGeometry relative="1" as="geometry" />
    </mxCell>
  </root>
</mxGraphModel>
```

Иллюстрация для вставки в Word (сразу после пайплайна):

- Рисунок: скрин терминала с успешным завершением `build.sh`.
- Что добавить: строки с configure/build/test summary.

## Источники кода, использованные в отчете

- `src/CMakeLists.txt`
- `src/CMakePresets.json`
- `src/build.sh`
- `src/include/contur/core/types.h`
- `src/include/contur/core/error.h`
- `src/include/contur/core/clock.h`
- `src/include/contur/core/event.h`
- `src/include/contur/arch/instruction.h`
- `src/include/contur/arch/register_file.h`

## Критерии готовности

- В тексте есть связка «архитектурное решение -> фрагмент кода -> практический эффект».
- Схемы распределены по контексту, а не вынесены отдельным блоком в конец.
- В документ встроены места под иллюстрации в тех разделах, где они логически объясняют материал.
- Отражены выводы по двум базовым этапам, на которых строятся следующие стадии проекта.

## Краткие выводы

На начальном цикле разработки удалось сформировать устойчивый каркас проекта: воспроизводимую сборку, единые доменные типы, предсказуемую модель ошибок, интерфейсные контракты и базовую архитектурную модель CPU/ISA. Это делает последующие этапы (память, процессы, планирование, ядро) расширением уже согласованной базы, а не набором несвязанных изменений.

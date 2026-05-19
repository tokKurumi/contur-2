#import "@preview/smk-sto:0.3.1": *
#import "@preview/fletcher:0.5.8" as fletcher: diagram, edge, node
#import "_meta.typ": *

#show: lab-report.with(
  institute: institute,
  department: department,
  work-number: 3,
  discipline: discipline,
  title: "Ядро исполнения: ALU, CPU, устройства и движок интерпретации",
  author: author,
  supervisor: supervisor,
  designation: designation,
)

= Цель работы

Построить замкнутый вычислительный контур симулятора Contur 2: арифметико-логическое
устройство, центральный процессор с циклом выборки/декодирования/исполнения, абстракцию
устройства ввода-вывода и движок интерпретации, обёрнутый за единый контракт
`IExecutionEngine`. Тот же контракт используется альтернативной реализацией — движком
нативного исполнения.

Задачи работы:

+ реализовать чистое ALU с возвратом ошибок через `Result<RegisterValue>`;
+ реализовать `Cpu` с поддержкой полного набора инструкций ISA проекта;
+ описать контракт `IDevice` и реализовать `ConsoleDevice` / `NetworkDevice`;
+ реализовать реестр устройств `DeviceManager` со строгой диспетчеризацией по `DeviceId`;
+ описать контракт `IExecutionEngine` и реализовать `InterpreterEngine` с тик-бюджетом.

= Реализация

== Арифметико-логическое устройство

`ALU` спроектирован как чистая (без состояния) функциональная единица: на вход — пара
регистровых значений, на выход — результат, обёрнутый в `Result<RegisterValue>`. Деление на
ноль не выбрасывает исключение, а возвращает `ErrorCode::DivisionByZero`:

```cpp
class ALU {
    public:
    [[nodiscard]] Result<RegisterValue> add(RegisterValue a, RegisterValue b) const noexcept;
    [[nodiscard]] Result<RegisterValue> sub(RegisterValue a, RegisterValue b) const noexcept;
    [[nodiscard]] Result<RegisterValue> mul(RegisterValue a, RegisterValue b) const noexcept;
    [[nodiscard]] Result<RegisterValue> div(RegisterValue a, RegisterValue b) const noexcept;

    [[nodiscard]] Result<RegisterValue> bitwiseAnd(RegisterValue a, RegisterValue b) const noexcept;
    [[nodiscard]] Result<RegisterValue> bitwiseOr(RegisterValue a, RegisterValue b) const noexcept;
    [[nodiscard]] Result<RegisterValue> bitwiseXor(RegisterValue a, RegisterValue b) const noexcept;
    [[nodiscard]] Result<RegisterValue> shiftLeft(RegisterValue a, RegisterValue b) const noexcept;
    [[nodiscard]] Result<RegisterValue> shiftRight(RegisterValue a, RegisterValue b) const noexcept;

    [[nodiscard]] RegisterValue compare(RegisterValue a, RegisterValue b) const noexcept;

    static constexpr RegisterValue ZERO_FLAG = 1; // a == b
    static constexpr RegisterValue SIGN_FLAG = 2; // a < b
};
```

Метод `compare` возвращает битовую маску флагов (нулевой и знаковый), что соответствует
регистру флагов в реальных архитектурах и упрощает реализацию условных переходов.

== Центральный процессор

`Cpu` хранит ссылку на `IMemory`, владеет ALU и регистром флагов сравнения. Метод
`step(RegisterFile &regs)` выполняет один цикл выборки/декодирования/исполнения над
переданным регистровым файлом и возвращает `Interrupt` — причину остановки текущего шага
(штатное продолжение, программное прерывание, ошибка). Каждая инструкция кодируется
структурой `Block` с полями `code`, `reg`, `operand`, `state` (последний задаёт режим
адресации: 0 — immediate, 1 — register-register). Последовательность операций показана на
рисунке @fig:cpu-cycle.

#figure(
  diagram(
    spacing: (2.2cm, 0.9cm),
    node-inset: 7pt,
    node((0, 0), [InterpreterEngine.execute(...)], fill: rgb("#dae8fc")),
    node((0, 1), [Cpu.step(regs)], fill: rgb("#d5e8d4")),
    node((1, 1), [IMemory.read(PC)], fill: rgb("#ffe6cc")),
    node((0, 2), [decode Block], fill: rgb("#fff2cc")),
    node((0, 3), [ALU / control / I/O], fill: rgb("#e1d5e7")),
    node((0, 4), [update RegisterFile/PC/flags], fill: rgb("#f8cecc")),
    node((0, 5), [return Interrupt], fill: rgb("#d5e8d4")),
    edge((0, 0), (0, 1), "->"),
    edge((0, 1), (1, 1), "->", [fetch]),
    edge((1, 1), (0, 2), "->", [Block], label-side: right),
    edge((0, 2), (0, 3), "->", [execute]),
    edge((0, 3), (0, 4), "->", [writeback]),
    edge((0, 4), (0, 5), "->"),
    edge((0, 5), (0, 0), "-->", [next iteration], bend: 60deg, label-side: left),
  ),
  caption: [Цикл выборки--декодирования--исполнения одной инструкции],
) <fig:cpu-cycle>

Возвращаемое значение `Interrupt` принимает значения `Ok`, `Exit`, `DivByZero`, `Error`,
`SystemCall`, `DeviceIO`, `NetworkIO`, `Timer`, `PageFault`. Эти варианты различают штатное
продолжение, ошибки данных, программные системные вызовы и асинхронные внешние причины.

== Устройства ввода-вывода

Контракт `IDevice` объединяет реальные и симулируемые устройства за пятью методами: `id`,
`name`, `read`, `write`, `isReady`. Реализованы две стратегии:

- `ConsoleDevice` — вывод на стандартный поток с буфером эха для чтения;
- `NetworkDevice` — bounded-deque с FIFO-семантикой и ошибками `BufferFull`/`BufferEmpty`.

`DeviceManager` хранит реестр устройств через `unique_ptr<IDevice>` и диспетчеризует
операции по `DeviceId`. Поверх него можно подключать произвольные устройства без правок в
CPU и движках исполнения. Связи компонентов показаны на рисунке @fig:devices.

#figure(
  diagram(
    spacing: (3.2cm, 1.4cm),
    node-inset: 8pt,
    node((0, 1), [Cpu], fill: rgb("#dae8fc")),
    node((1, 1), [DeviceManager], fill: rgb("#d5e8d4")),
    node((2, 0), [ConsoleDevice], fill: rgb("#ffe6cc")),
    node((2, 2), [NetworkDevice], fill: rgb("#ffe6cc")),
    edge((0, 1), (1, 1), "->", [read/write(DeviceId)], label-side: left, label-sep: 0.9em),
    edge((1, 1), (2, 0), "-->", [dispatch], label-side: left),
    edge((1, 1), (2, 2), "-->", [dispatch], label-side: right),
  ),
  caption: [Подключение CPU и устройств через DeviceManager],
) <fig:devices>

== Движок интерпретации

`IExecutionEngine` — единая точка входа в исполнение, через которую работает диспетчер. Её
реализуют оба движка исполнения: интерпретирующий и нативный.

```cpp
class IExecutionEngine {
    public:
    [[nodiscard]] virtual ExecutionResult execute(ProcessImage &process, std::size_t tickBudget) = 0;
    virtual void halt(ProcessId pid) = 0;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
};
```

`InterpreterEngine` инкапсулирует связку `Cpu + IMemory + ALU` и на каждом вызове `execute`
выполняет до `tickBudget` шагов CPU, агрегирует прерывания и возвращает `ExecutionResult` с
одной из причин остановки: `BudgetExhausted`, `ProcessExited`, `Error`, `Interrupted`,
`Halted`. Структура `ExecutionResult` создаётся через статические фабрики
(`budgetExhausted`, `exited`, `error`, `interrupted`, `halted`), что исключает
несогласованные комбинации полей.

= Заключение

Вычислительный контур симулятора замкнут на чистом ALU, явных регистрах и небоксируемых
прерываниях. Цикл выборки--декодирования--исполнения CPU прозрачен и тестируется покомандно.
Устройства подключаются как сменяемые стратегии. Контракт `IExecutionEngine` стандартизирует
запуск процессов независимо от их реализации (байт-код или нативный host-процесс), что
обеспечивает встройку второго движка исполнения без изменений в ядре, диспетчере и
планировщике.

= Список использованных источников

+ `src/include/contur/cpu/alu.h`
+ `src/contur/cpu/alu.cpp`
+ `src/include/contur/cpu/i_cpu.h`
+ `src/include/contur/cpu/cpu.h`
+ `src/contur/cpu/cpu.cpp`
+ `src/include/contur/arch/instruction.h`
+ `src/include/contur/arch/interrupt.h`
+ `src/include/contur/arch/register_file.h`
+ `src/include/contur/io/i_device.h`
+ `src/include/contur/io/console_device.h`
+ `src/include/contur/io/network_device.h`
+ `src/include/contur/io/device_manager.h`
+ `src/contur/io/device_manager.cpp`
+ `src/include/contur/execution/i_execution_engine.h`
+ `src/include/contur/execution/execution_context.h`
+ `src/include/contur/execution/interpreter_engine.h`
+ `src/contur/execution/interpreter_engine.cpp`

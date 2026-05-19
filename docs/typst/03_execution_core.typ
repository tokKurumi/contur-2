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

= Охват этапов

- Этап 4: CPU + I/O
- Этап 5: Interpreter Engine

== Контекст и цель

После того как сформированы базовые типы, память и модель процесса, проект получает
вычислительный контур — слой, который превращает байт-код процесса в наблюдаемое поведение.
Этот отчёт фиксирует, как устроены ALU и CPU, как организован цикл выборки, декодирования и исполнения,
как I/O-устройства подключаются к CPU и как `InterpreterEngine` инкапсулирует этот цикл за
единым строгим контрактом `IExecutionEngine`. Тот же контракт используется и нативным
движком исполнения (см. отчёт 8).

= Реализация

== ALU как чистая вычислительная функция

`ALU` намеренно не хранит состояние: он принимает на вход значения регистров и возвращает `Result<RegisterValue>`. Деление на ноль не выбрасывает исключение — оно явно возвращается вызывающему через `ErrorCode::DivisionByZero`. Такое решение поддерживает архитектурный принцип «никаких исключений в runtime-горячих путях», заложенный в фундаменте проекта.

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

Метод `compare` намеренно возвращает не пару `bool`, а битовую маску флагов (`ZERO_FLAG`/`SIGN_FLAG`), что соответствует поведению регистра флагов в реальных архитектурах и упрощает реализацию инструкций условного перехода (`JumpEqual`, `JumpLess`, и т. д.).

== CPU и цикл fetch–decode–execute

`Cpu` хранит ссылку на `IMemory`, владеет ALU и регистром флагов сравнения. Метод
`step(RegisterFile &regs)` выполняет один цикл выборки, декодирования и исполнения над
переданным регистровым файлом и возвращает `Interrupt` — причину остановки текущего шага
(штатное продолжение, программное прерывание, ошибка). Каждая инструкция кодируется
структурой `Block` с полями `code`, `reg`, `operand`, `state`.

Последовательность операций показана на рисунке @fig:cpu-cycle.

#figure(
  diagram(
    spacing: (2.2cm, 0.9cm),
    node-inset: 7pt,
    node((0, 0), [InterpreterEngine.execute(...)], fill: rgb("#dae8fc")),
    node((0, 1), [Cpu.step(regs)], fill: rgb("#d5e8d4")),
    node((1, 1), [IMemory.read(PC)], fill: rgb("#ffe6cc")),
    node((0, 2), [декодирование Block], fill: rgb("#fff2cc")),
    node((0, 3), [операции ALU / управление / ввод-вывод], fill: rgb("#e1d5e7")),
    node((0, 4), [обновление RegisterFile / PC / флагов], fill: rgb("#f8cecc")),
    node((0, 5), [возврат Interrupt], fill: rgb("#d5e8d4")),
    edge((0, 0), (0, 1), "->"),
    edge((0, 1), (1, 1), "->", [выборка]),
    edge((1, 1), (0, 2), "->", [Block], label-side: right),
    edge((0, 2), (0, 3), "->", [исполнение]),
    edge((0, 3), (0, 4), "->", [запись результата]),
    edge((0, 4), (0, 5), "->"),
    edge((0, 5), (0, 0), "-->", [следующая итерация], bend: 60deg, label-side: left),
  ),
  caption: [Цикл выборки, декодирования и исполнения одной инструкции],
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
    edge((0, 1), (1, 1), "->", [чтение/запись(DeviceId)], label-side: left, label-sep: 0.9em),
    edge((1, 1), (2, 0), "-->", [диспетчеризация], label-side: left),
    edge((1, 1), (2, 2), "-->", [диспетчеризация], label-side: right),
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

= Заключение <s>

Вычислительный контур симулятора замкнут на чистом ALU, явных регистрах и небоксируемых
прерываниях. Цикл выборки, декодирования и исполнения CPU прозрачен и тестируется покомандно.
Устройства подключаются как сменяемые стратегии. Контракт `IExecutionEngine` стандартизирует
запуск процессов независимо от их реализации, байт-кодной или нативной, что обеспечивает
встройку второго движка исполнения без изменений в ядре, диспетчере и планировщике.

- Описана семантика `Block`-инструкции и связь полей с ISA.
- Показана связка ALU/CPU/IMemory без скрытых глобальных состояний.
- Перечислены и объяснены значения `Interrupt` и `StopReason`.
- Описан контракт `IExecutionEngine` и то, что `InterpreterEngine` — лишь одна из стратегий.
- Указано, как `DeviceManager` обеспечивает единый путь к разнотипным устройствам.
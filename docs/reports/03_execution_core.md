# Отчет 3. Ядро исполнения

## Охват этапов

- Этап 4: CPU + I/O
- Этап 5: Interpreter Engine

## Контекст и цель

После того как сформированы базовые типы, память и модель процесса, проект получает
вычислительный контур — слой, который превращает байт-код процесса в наблюдаемое поведение.
Этот отчёт фиксирует, как устроены ALU и CPU, как организован цикл выборки/декодирования/исполнения,
как I/O-устройства подключаются к CPU и как `InterpreterEngine` инкапсулирует этот цикл за
единым строгим контрактом `IExecutionEngine`. Тот же контракт используется и нативным
движком исполнения (см. отчёт 8).

Цели этапов 4–5:

- иметь чистое (без побочных эффектов) ALU, отдающее ошибки через `Result<T>`;
- запустить цикл fetch–decode–execute, который однозначно сопоставляет один шаг CPU
    с одной строкой `Block`-кода;
- встроить I/O-устройства в архитектуру как первоклассный, заменяемый компонент через
    `IDevice`/`DeviceManager`;
- предоставить вышестоящим слоям (диспетчеру, ядру) единый объект-стратегию исполнения —
    `IExecutionEngine`.

## ALU как чистая вычислительная функция

`ALU` намеренно не хранит состояние: он принимает на вход значения регистров и возвращает
`Result<RegisterValue>`. Деление на ноль не выбрасывает исключение — оно явно возвращается
вызывающему через `ErrorCode::DivisionByZero`. Такое решение поддерживает архитектурный принцип
«никаких исключений в runtime-горячих путях», заложенный в фундаменте проекта.

```cpp
class ALU
{
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

Метод `compare` намеренно возвращает не пару `bool`, а битовую маску флагов (`ZERO_FLAG`/`SIGN_FLAG`),
что соответствует поведению регистра флагов в реальных архитектурах и упрощает реализацию инструкций
условного перехода (`JumpEqual`, `JumpLess`, и т. д.).

## CPU и цикл fetch–decode–execute

`Cpu` реализует контракт `ICPU` и инкапсулирует за PIMPL-границей ссылку на `IMemory` и
собственный `ALU`. Внутренне CPU также поддерживает регистр флагов сравнения и обращается к
`RegisterFile` процесса через явный параметр `step(RegisterFile &regs)`. Это сохраняет CPU
без неявного владения контекстом — контекст всегда «приносит» вызывающий (как правило,
`InterpreterEngine`).

Каждая инструкция кодируется одной структурой `Block`, в которой хранится:

- `code` — код инструкции (`Instruction`);
- `reg` — индекс целевого/исходного регистра;
- `operand` — непосредственное значение, адрес памяти или индекс второго регистра;
- `state` — режим адресации (0 — immediate, 1 — register-register).

Возврат из `step` — это значение `Interrupt`, описывающее причину остановки текущего шага
(штатное продолжение, программное прерывание, ошибка и т. д.):

```cpp
class Cpu final : public ICPU
{
  public:
  explicit Cpu(IMemory &memory);

  [[nodiscard]] Interrupt step(RegisterFile &regs) override;
  void reset() noexcept override;
  [[nodiscard]] RegisterValue flags() const noexcept;

  private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
```

### Схема: цикл одной инструкции

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="60" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="InterpreterEngine.execute(...)" vertex="1"><mxGeometry x="40" y="40" width="200" height="50" as="geometry" /></mxCell>
    <mxCell id="61" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="Cpu.step(regs)" vertex="1"><mxGeometry x="290" y="40" width="140" height="50" as="geometry" /></mxCell>
    <mxCell id="62" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="IMemory.read(PC)" vertex="1"><mxGeometry x="480" y="40" width="160" height="50" as="geometry" /></mxCell>
    <mxCell id="63" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#fff2cc;strokeColor=#d6b656;" value="decode Block (code, reg, operand, state)" vertex="1"><mxGeometry x="290" y="120" width="280" height="50" as="geometry" /></mxCell>
    <mxCell id="64" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#e1d5e7;strokeColor=#9673a6;" value="ALU op / control flow / I/O" vertex="1"><mxGeometry x="290" y="200" width="200" height="50" as="geometry" /></mxCell>
    <mxCell id="65" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#f8cecc;strokeColor=#b85450;" value="update RegisterFile / PC / flags" vertex="1"><mxGeometry x="290" y="280" width="240" height="50" as="geometry" /></mxCell>
    <mxCell id="66" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="return Interrupt" vertex="1"><mxGeometry x="40" y="280" width="160" height="50" as="geometry" /></mxCell>
    <mxCell id="70" edge="1" parent="1" source="60" target="61" style="endArrow=classic;html=1;"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="71" edge="1" parent="1" source="61" target="62" style="endArrow=classic;html=1;" value="fetch"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="72" edge="1" parent="1" source="62" target="63" style="endArrow=classic;html=1;" value="Block"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="73" edge="1" parent="1" source="63" target="64" style="endArrow=classic;html=1;" value="execute"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="74" edge="1" parent="1" source="64" target="65" style="endArrow=classic;html=1;" value="writeback"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="75" edge="1" parent="1" source="65" target="66" style="endArrow=classic;html=1;" value="Interrupt"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="76" edge="1" parent="1" source="66" target="60" style="endArrow=classic;html=1;dashed=1;" value="next iteration"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Обработка прерываний и ошибок

В рамках одного шага CPU может вернуть несколько ключевых значений `Interrupt`:

- `Ok` — продолжать исполнение;
- `Exit` — процесс попросил завершиться (`Halt` / `Int Exit`);
- `DivByZero`, `Error` — фатальная ошибка ALU/инструкции;
- `SystemCall`, `DeviceIO`, `NetworkIO`, `Timer`, `PageFault` — события, которые требуют
    участия ядра (синхронные системные вызовы или асинхронные внешние причины).

`InterpreterEngine` интерпретирует возвращаемое `Interrupt` уже в контексте `ExecutionResult`:
он отвечает за финализацию причины остановки. Это поведение зафиксировано в `execution_context.h`
через `StopReason` и набор фабрик `ExecutionResult::budgetExhausted/exited/error/interrupted/halted`.

## I/O-устройства: `IDevice` и `DeviceManager`

В архитектуре устройства ввода-вывода представлены как полноценная стратегическая абстракция.
Каждое устройство получает `DeviceId`, имя и тройку методов `read/write/isReady`. Это позволяет
описать одинаково и консоль, и сетевой буфер, и любое будущее устройство.

```cpp
class IDevice
{
  public:
  virtual ~IDevice() = default;

  [[nodiscard]] virtual DeviceId id() const noexcept = 0;
  [[nodiscard]] virtual std::string_view name() const noexcept = 0;
  [[nodiscard]] virtual Result<RegisterValue> read() = 0;
  [[nodiscard]] virtual Result<void> write(RegisterValue value) = 0;
  [[nodiscard]] virtual bool isReady() const noexcept = 0;
};
```

В проекте реализованы две конкретные стратегии:

- `ConsoleDevice` — пишет в stdout и поддерживает буфер эхо для чтения;
- `NetworkDevice` — bounded-deque с FIFO-семантикой и явными ошибками `BufferFull`/`BufferEmpty`.

`DeviceManager` — это реестр устройств, который владеет ими через `unique_ptr<IDevice>` и
диспетчеризует операции по `DeviceId`. Так CPU/интерпретатор могут читать и писать в устройства
без какого-либо ветвления по типам — `device.write(value)` через `DeviceManager` достаточно.

### Схема: подключение CPU и устройств через DeviceManager

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="70" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="Cpu" vertex="1"><mxGeometry x="40" y="100" width="100" height="50" as="geometry" /></mxCell>
    <mxCell id="71" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="DeviceManager" vertex="1"><mxGeometry x="220" y="100" width="160" height="50" as="geometry" /></mxCell>
    <mxCell id="72" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="ConsoleDevice" vertex="1"><mxGeometry x="450" y="50" width="150" height="50" as="geometry" /></mxCell>
    <mxCell id="73" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="NetworkDevice" vertex="1"><mxGeometry x="450" y="150" width="150" height="50" as="geometry" /></mxCell>
    <mxCell id="74" edge="1" parent="1" source="70" target="71" style="endArrow=classic;html=1;" value="read/write(DeviceId)"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="75" edge="1" parent="1" source="71" target="72" style="endArrow=block;html=1;dashed=1;" value="dispatch"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="76" edge="1" parent="1" source="71" target="73" style="endArrow=block;html=1;dashed=1;" value="dispatch"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## InterpreterEngine: стратегия исполнения

`InterpreterEngine` реализует `IExecutionEngine` и оборачивает связку `Cpu + IMemory + ALU`
в единую точку входа `execute(ProcessImage&, std::size_t tickBudget)`. На каждый вызов он
исполняет до `tickBudget` шагов CPU, аккумулирует прерывания и возвращает
`ExecutionResult` с одной из причин остановки:

- `BudgetExhausted` — бюджет тиков исчерпан (вытеснение);
- `ProcessExited` — процесс завершился штатно (Halt / Int Exit);
- `Error` — невосстановимая ошибка инструкции;
- `Interrupted` — поднято прерывание, требующее вмешательства ядра;
- `Halted` — процесс был принудительно остановлен через `halt()`.

```cpp
class IExecutionEngine
{
  public:
  virtual ~IExecutionEngine() = default;

  [[nodiscard]] virtual ExecutionResult execute(ProcessImage &process, std::size_t tickBudget) = 0;
  virtual void halt(ProcessId pid) = 0;
  [[nodiscard]] virtual std::string_view name() const noexcept = 0;
};
```

Эта же абстракция используется и второй реализацией движка исполнения — `NativeEngine`,
который запускает настоящий host-процесс. Ядру не нужно знать, исполняется ли процесс
байт-кодом или это реальный host-процесс: оба варианта подключаются через один и тот же
контракт.

### Схема: компоненты вычислительного контура

```xml
<mxGraphModel dx="1773" dy="644" grid="1" gridSize="10" guides="1" tooltips="1" connect="1" arrows="1" fold="1" page="1" pageScale="1" pageWidth="827" pageHeight="1169" math="0" shadow="0">
  <root>
    <mxCell id="0" /><mxCell id="1" parent="0" />
    <mxCell id="80" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#dae8fc;strokeColor=#6c8ebf;" value="Dispatcher" vertex="1"><mxGeometry x="40" y="60" width="130" height="50" as="geometry" /></mxCell>
    <mxCell id="81" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#d5e8d4;strokeColor=#82b366;" value="IExecutionEngine" vertex="1"><mxGeometry x="220" y="60" width="160" height="50" as="geometry" /></mxCell>
    <mxCell id="82" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="InterpreterEngine" vertex="1"><mxGeometry x="430" y="20" width="160" height="40" as="geometry" /></mxCell>
    <mxCell id="83" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#ffe6cc;strokeColor=#d79b00;" value="NativeEngine" vertex="1"><mxGeometry x="430" y="80" width="200" height="40" as="geometry" /></mxCell>
    <mxCell id="84" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#e1d5e7;strokeColor=#9673a6;" value="Cpu" vertex="1"><mxGeometry x="220" y="160" width="100" height="50" as="geometry" /></mxCell>
    <mxCell id="85" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#e1d5e7;strokeColor=#9673a6;" value="ALU" vertex="1"><mxGeometry x="350" y="160" width="100" height="50" as="geometry" /></mxCell>
    <mxCell id="86" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#f8cecc;strokeColor=#b85450;" value="IMemory" vertex="1"><mxGeometry x="480" y="160" width="120" height="50" as="geometry" /></mxCell>
    <mxCell id="87" parent="1" style="rounded=1;whiteSpace=wrap;html=1;fillColor=#fff2cc;strokeColor=#d6b656;" value="DeviceManager" vertex="1"><mxGeometry x="630" y="160" width="150" height="50" as="geometry" /></mxCell>
    <mxCell id="90" edge="1" parent="1" source="80" target="81" style="endArrow=classic;html=1;" value="execute(...)"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="91" edge="1" parent="1" source="82" target="81" style="endArrow=block;html=1;dashed=1;" value="implements"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="92" edge="1" parent="1" source="83" target="81" style="endArrow=block;html=1;dashed=1;" value="implements"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="93" edge="1" parent="1" source="82" target="84" style="endArrow=classic;html=1;" value="step"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="94" edge="1" parent="1" source="84" target="85" style="endArrow=classic;html=1;" value="uses"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="95" edge="1" parent="1" source="84" target="86" style="endArrow=classic;html=1;" value="fetch/load/store"><mxGeometry relative="1" as="geometry" /></mxCell>
    <mxCell id="96" edge="1" parent="1" source="84" target="87" style="endArrow=classic;html=1;dashed=1;" value="I/O via interrupt"><mxGeometry relative="1" as="geometry" /></mxCell>
  </root>
</mxGraphModel>
```

## Источники кода, использованные в отчёте

- `src/include/contur/cpu/alu.h`
- `src/include/contur/cpu/i_cpu.h`
- `src/include/contur/cpu/cpu.h`
- `src/contur/cpu/cpu.cpp`
- `src/include/contur/arch/instruction.h`
- `src/include/contur/arch/interrupt.h`
- `src/include/contur/arch/register_file.h`
- `src/include/contur/io/i_device.h`
- `src/include/contur/io/device_manager.h`
- `src/include/contur/io/console_device.h`
- `src/include/contur/io/network_device.h`
- `src/include/contur/execution/i_execution_engine.h`
- `src/include/contur/execution/execution_context.h`
- `src/include/contur/execution/interpreter_engine.h`
- `src/contur/execution/interpreter_engine.cpp`

## Критерии готовности

- Описана семантика `Block`-инструкции и связь полей с ISA.
- Показана связка ALU/CPU/IMemory без скрытых глобальных состояний.
- Перечислены и объяснены значения `Interrupt` и `StopReason`.
- Описан контракт `IExecutionEngine` и то, что `InterpreterEngine` — лишь одна из стратегий.
- Указано, как `DeviceManager` обеспечивает единый путь к разнотипным устройствам.

## Краткие выводы

Вычислительный контур замкнут, наблюдаем и тестируем: ALU выдаёт ошибки данных, CPU честно
проходит цикл fetch–decode–execute через `Block`-инструкции, а устройства подключаются как
сменяемые стратегии. Поверх этого `IExecutionEngine` стандартизирует точку входа для любых
реализаций исполнения: байт-кодного `InterpreterEngine` и нативного `NativeEngine` — ядру,
диспетчеру и планировщику не нужно знать, какой из них используется.

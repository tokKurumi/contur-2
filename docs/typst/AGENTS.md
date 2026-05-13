# Working with the `smk-sto-004` Typst template

> **Note for the AI:** the document you are producing is in **Russian**.
> All headings, body text, captions, list items, table headers, formula
> explanations, and bibliography titles MUST be written in Russian. Only
> identifiers (label names, file names, package directives) stay in Latin.

This package implements **СМК СТО 004–2020** of MGU im. N.P. Ogarev — the
internal standard for lab-work reports («Правила оформления отчётов о
лабораторных работах»). All page sizes, margins, fonts, heading rules,
list markers, figure/table captions, formula numbering, appendix lettering
and the title page (Приложение А) are already wired up. **Your job is to
write the report — not to re-style it.**

## How to start a report

A minimal `report.typ` is:

```typst
#import "@preview/smk-sto-004:0.1.0": *

#show: lab-report.with(
  institute: "Институт электроники и светотехники",
  department: "Кафедра метрологии, стандартизации и сертификации",
  work-number: 1,
  discipline: "Поверка средств измерений электрических величин",
  title: "Измерение в цепях постоянного тока",
  author: (
    name: "И.О. Фамилия",
    direction: "27.03.01 Стандартизация и метрология",
  ),
  supervisor: (
    name: "И.О. Фамилия",
    position: "канд. техн. наук, доц.",
  ),
  designation: (direction: "27.03.01", variant: "08"),
)

= Цель работы
Текст...
```

Everything except the obvious user data has sensible defaults pulled from
the standard:

| Field           | Default                                                         |
|-----------------|-----------------------------------------------------------------|
| `ministry`      | «Министерство науки и высшего образования Российской Федерации» |
| `organization`  | full / short names of «МГУ им. Н.П. Огарёва»                    |
| `city`          | «Саранск»                                                       |
| `year`          | current year (`datetime.today()`)                               |

Only override these if the user explicitly asks.

## API surface

Imported with `#import "@preview/smk-sto-004:0.1.0": *`:

- `lab-report(..., body)` — main show rule. Use as `#show: lab-report.with(...)`.
- `appendix` — section rule. After `#show: appendix`, every `= Heading`
  becomes a numbered appendix (А, Б, В, …). Figures, tables and equations
  are renumbered relative to the appendix letter.
- `sign-field(position, name)` — pre-formatted signature row used on the
  title page. You normally don't call it directly.
- `format-designation(...)` — turns `(direction: ..., variant: ...)` into
  the canonical «ЛР–02069964–DDD–NN–YY» string.
- `enum-letter(n)` — Russian alphabetic enum marker (а, б, в, …); use
  `#set enum(numbering: enum-letter)` when the user lists points «а)»,
  «б)», «в)» per СТО 8.1.5.
- `table-label(num: ..., caption: ...)` — letter-spaced «Т а б л и ц а N – ...»
  label, useful for manual table headings (the `#figure(...)` path uses it
  automatically).
- `table-continuation(num)` — produces «Продолжение таблицы N» (pass
  `kind: "Окончание"` for the last fragment).

## What `lab-report` already enforces

You do **not** need to set any of these manually — adding extra `#set`
rules will only fight the template:

- A4 paper, margins 30 / 15 / 20 / 20 mm (left/right/top/bottom).
- Times New Roman 14 pt, 1.5-line spacing, paragraph indent 1.25 cm.
- Hyphenation disabled (per СТО 8.1.5: «Переносы слов в заголовках не
  допускаются» — and overall the standard expects whole words).
- Headings: numbered `1`, `1.1`, `1.1.1`, bold, indented by 1.25 cm,
  followed by one blank line; level-1 headings start a new page.
- Structural headings (Содержание, Введение, Заключение, Список
  использованных источников, Перечень сокращений и обозначений,
  Термины и определения) are auto-detected, centered, uppercased and
  un-numbered.
- Lists: marker «–», nested indent 1.25 cm.
- Enums: «1)», «2)», … by default (СТО 8.1.5 examples).
- Figure caption: «Рисунок N – Подпись», centered, below the figure.
- Table caption: «Т а б л и ц а N – Подпись» (letter-spaced «Таблица»),
  left-aligned above the table.
- Math equation numbering `(1)`, right-aligned; cross-references render
  as «(1)» per СТО 8.4.4.
- Page number bottom-center; no page number on the title page.
- Quotes: «ёлочки» for double, ‹одиночные› for single.

## Writing the body — patterns

### Headings

Just use `=`, `==`, `===`. The numbering, indent and spacing are
automatic.

```typst
= Цель работы
== Подраздел
=== Пункт
```

For structural sections, write the canonical title verbatim — the
template will recognize and re-style it:

```typst
= Введение
...
= Заключение
```

### Lists, enums, lettered items

```typst
- первый пункт;
- второй пункт;
- третий пункт.

+ Сначала собрать схему.
+ Затем подать напряжение.
+ Снять показания.

#set enum(numbering: enum-letter)
+ освоение приёмов наблюдения;
+ освоение методов измерения;
+ интерпретация результатов.
```

### Figures

```typst
#figure(
  image("plots/u-of-i.svg", width: 80%),
  caption: [Зависимость напряжения от силы тока],
) <fig:ui>

В соответствии с рисунком @fig:ui ...
```

### Tables

```typst
#figure(
  table(
    columns: 3,
    align: center,
    [Опыт], [$U$, В], [$I$, А],
    [1], [5,0], [0,10],
    [2], [5,0], [0,20],
  ),
  caption: [Результаты измерений],
) <tab:results>

Сведения сведены в таблицу @tab:results.
```

### Formulas

```typst
Сопротивление вычисляют по закону Ома:

$ R = U / I $ <eq:ohm>

где $U$ — напряжение, В; \
    $I$ — сила тока, А.

Из формулы @eq:ohm видно, что ...
```

The «где»-block is plain text — write it the way the standard prescribes
(СТО 8.4.2): start with «где» on the abzac, comma-separate items, give
each variable on a new line.

### Block diagrams (boxes + arrows)

`diagram`, `node`, `edge` are re-exported from `fletcher`. Wrap in
`#figure(...)` to get a standard-compliant «Рисунок N – ...» caption:

```typst
#figure(
  diagram(
    node((0, 0), [contur2 (app)], fill: rgb("#dae8fc")),
    node((2, 0), [contur2_demos],  fill: rgb("#d5e8d4")),
    node((1, 1), [contur2_lib],    fill: rgb("#ffe6cc")),
    node((3, 1), [tests (GTest)],  fill: rgb("#f8cecc")),
    edge((0, 0), (1, 1), "->", [link]),
    edge((2, 0), (1, 1), "->", [link]),
    edge((3, 1), (1, 1), "->", [test]),
  ),
  caption: [Граф зависимостей модулей],
) <fig:deps>
```

When the user pastes a drawio/mxGraph XML, translate it node-by-node:
each `mxCell` with `vertex="1"` → `node((x, y), [label], fill: ...)`,
each `mxCell` with `edge="1"` → `edge((x1, y1), (x2, y2), "->", [label])`.
Map drawio's `fillColor`/`strokeColor` directly via `rgb("#...")`.

Defaults set by the template: rounded rectangles, 0.5 pt black stroke,
8 pt inset, 1.2 cm spacing, centered horizontally. Override any
`fletcher.diagram` argument if needed (`spacing`, `node-stroke`,
`edge-stroke`, etc.).

### Bibliography

```typst
#bibliography(
  "references.bib",
  style: "gost-r-705-2008-numeric",
  title: [Список использованных источников],
)
```

(The title is auto-detected as a structural heading.)

### Appendices

```typst
= Заключение
...

#bibliography(...)

#show: appendix

= Дополнительные расчёты
Текст приложения А ...

= Графики зависимостей
Текст приложения Б ...
```

Each `=` after `#show: appendix` produces a centered «Приложение Х /
(обязательное) / Название» on a new page. Figures, tables and equations
inside become «А.1», «А.2», «(А.1)», etc.

## Things to avoid

- **Do not** add manual `#set page(...)`, `#set text(...)`, `#set par(...)`
  unless the user asks for a specific deviation. The template's defaults
  are the standard.
- **Do not** number structural sections (Содержание, Введение,
  Заключение, Список использованных источников) — they are by definition
  non-numbered and the template handles them.
- **Do not** add the title page manually — `lab-report` emits it.
- **Do not** use em dash «—» for «Рисунок N – ...» / «Таблица N – ...»
  separators — the template already uses en dash «–» per the standard.
  In running prose either dash is fine; match what the user already
  wrote.
- **Do not** rename the appendix supplement — the value `[Приложение]`
  is what the outline rule keys on to render «Приложение А Название» in
  the table of contents.

## Quick reference: standard sections of a lab report (СТО 7.1)

A complete report has, in order:

1. Title page (auto, from `lab-report.with(...)`)
2. `= Цель работы` (or «Цель или задачи работы»)
3. `= Программа и методика эксперимента`
4. `= Результаты измерений (наблюдений)`
5. `= Обработка экспериментальных данных и оценка погрешностей`
6. `= Анализ результатов и выводы` (sometimes labeled `= Выводы` or
   `= Заключение`)
7. Optional `#bibliography(...)` titled «Список использованных источников»
8. Optional `#show: appendix` followed by appendix headings

`#outline()` may be placed right after the title-page block to produce
«Содержание».

## File layout for a user project

```
report/
├── main.typ           ← the .typ file you edit
├── references.bib     ← optional, for the bibliography
└── assets/            ← images, plots
```

Compile with:

```
typst compile main.typ
```

# Working with the `smk-sto` Typst template

> **Note for the AI:** the document you are producing is in **Russian**.
> All headings, body text, captions, list items, table headers, formula
> explanations, and bibliography titles MUST be written in Russian. Only
> identifiers (label names, file names, package directives) stay in Latin.

This package implements **two** internal standards of MGU im. N.P. Ogarev:

- **СМК СТО 004–2020** — lab-work reports («Правила оформления отчётов
  о лабораторных работах»). Show rule: `lab-report`.
- **СМК СТО 014–2025** — practice reports («Практика обучающихся высшего
  образования. Общие требования, правила оформления отчётности»). Show
  rule: `practice-report`. Delegates general formatting (title page,
  bibliography, appendices) to **СМК СТО 006–2025** «Общие требования
  к построению, изложению и оформлению документов учебной деятельности».

The standards share ~90% of formatting rules (page size, margins, fonts,
heading numbering, lists, captions, formulas, appendices), so most of the
internals are shared. The two flavors differ in their **title page** and
**document designation** (`ЛР–…` vs `ОП–…`).

**Public API uses a clear prefix:** every lab-related export starts with
`lab-report`, every practice-related export starts with `practice-report`.
Pick the right family for the document you are writing — they are not
interchangeable.

**Your job is to write the report — not to re-style it.**

## How to start a lab-work report (СТО 004–2020)

A minimal `report.typ`:

```typst
#import "@preview/smk-sto:0.3.1": *

#show: lab-report.with(
  institute: "Институт электроники и светотехники",
  department: "Кафедра метрологии, стандартизации и сертификации",
  work-number: 1,
  discipline: "Поверка средств измерений электрических величин",
  title: "Измерение в цепях постоянного тока",
  author: (
    name: "И. И. Иванов",
    direction: "27.03.01 Стандартизация и метрология",
  ),
  supervisor: (
    name: "П. П. Петров",
    position: "канд. техн. наук, доц.",
  ),
  designation: (direction: "27.03.01", variant: "01"),
)

= Цель работы
Текст...
```

## How to start a practice report (СТО 014–2025)

```typst
#import "@preview/smk-sto:0.3.1": *

#show: practice-report.with(
  institute: "Институт электроники и светотехники",
  department: "Кафедра метрологии, стандартизации и сертификации",
  kind: "учебная",                          // вид: «учебная» / «производственная»
  practice-type: "ознакомительная",         // тип в соответствии с ОПОП ВО
  author: (
    name: "И. И. Иванов",
    course: 1,
    group: "101М",
  ),
  direction: (code: "27.03.01", name: "Стандартизация и метрология"),
  profile: "Метрология и метрологическое обеспечение",
  location: "г. Саранск, ФГБОУ ВО «МГУ им. Н. П. Огарёва», кафедра ...",
  period: (start: "08.09.2025", end: "27.12.2025"),
  designation: (kind: "У", direction: "27.03.01", variant: "01"),
  supervisor-uni: (
    name: "П. П. Петров",
    position: "канд. техн. наук, доц.",
  ),
  year: 2025,
)

= Введение <s>
Текст...
```

`kind` is the *вид* practice (учебная / производственная). `practice-type`
is the more specific *тип* per ОПОП ВО (ознакомительная,
научно-исследовательская работа, преддипломная, педагогическая, и т. п.).
The designation kind code is `У` for учебная, `П` for производственная.

> **Personal data:** use neutral placeholders (`И. И. Иванов`,
> `П. П. Петров`) when you assemble an example or template — never paste
> real student or supervisor names unless the user explicitly provides
> them.

## Defaults shared by both reports

Everything except the obvious user data has sensible defaults pulled from
the standard:

| Field           | Default                                                    |
|-----------------|------------------------------------------------------------|
| `ministry`      | «Министерство науки и высшего образования Российской Федерации» |
| `organization`  | full / short names of «МГУ им. Н.П. Огарёва»               |
| `city`          | «Саранск»                                                  |
| `year`          | current year (`datetime.today()`)                          |

Only override these if the user explicitly asks.

## API surface

Imported with `#import "@preview/smk-sto:0.3.1": *`:

**Lab reports (СТО 004–2020):**

- `lab-report(..., body)` — main show rule. Use as `#show: lab-report.with(...)`.
- `lab-report-title-page(...)` — standalone renderer for the title page
  (Приложение А). Use only if you need to detach the title from
  `lab-report`; normally call `lab-report` and the title comes for free.
- `lab-report-designation(...)` — formats «ЛР–02069964–DDD–NN–YY» from
  `(direction: ..., variant: ...)`.

**Practice reports (СТО 014–2025):**

- `practice-report(..., body)` — main show rule. Use as `#show: practice-report.with(...)`.
- `practice-report-title-page(...)` — standalone renderer for the title
  page (Приложение В).
- `practice-report-designation(...)` — formats
  «ОП–02069964–В–DD.NN.NN–NN–YYYY» from `(kind: "У", direction: "...",
  variant: "...")`.
- `practice-report-task(...)` — задание на практику (Приложение Б):
  the practice assignment form. Each form starts on a new page and uses
  its own page numbering.
- `practice-report-diary(...)` — дневник практики (Приложение Г):
  table of practice entries «Дата / Содержание / Замечания».
- `practice-report-survey(...)` — анкета обучающегося (Приложение Д):
  11-question questionnaire with checkboxes and 1–5 ratings. Pass
  selected answers as `answers: (q1: "да", q7: 5, ...)`.
- `practice-report-feedback(...)` — отзыв руководителя от профильной
  организации (Приложение Ж): six numbered sections + signature.

**Common helpers (apply to both flavors):**

- `appendix` — section rule. After `#show: appendix`, every `= Heading`
  becomes a numbered appendix (А, Б, В, …). Figures, tables and equations
  are renumbered relative to the appendix letter.
- `where-block(...)` — formats the «где …»-block under a formula with a
  hanging indent (СТО 8.4.2). Each pair is `(symbol, description)`:
  ```typst
  #where-block(
    ($U$, [напряжение, В]),
    ($I$, [сила тока, А]),
  )
  ```
  Always prefer this helper over manual `\` line breaks — it produces a
  properly aligned column.
- `enum-letter(n)` — Russian alphabetic enum marker (а, б, в, …); use
  `#set enum(numbering: enum-letter)` when the user lists points «а)»,
  «б)», «в)» per СТО 8.1.5.
- `sign-field(position, name)` — pre-formatted signature row used on the
  title pages. You normally don't call it directly.
- `table-label(num: ..., caption: ...)` — letter-spaced «Т а б л и ц а N – ...»
  label, useful for manual table headings (the `#figure(...)` path uses it
  automatically).
- `table-continuation(num)` — produces «Продолжение таблицы N» (pass
  `kind: "Окончание"` for the last fragment).
- `diagram`, `node`, `edge` — re-exported from `fletcher` for block
  diagrams.

## What `lab-report` / `practice-report` already enforce

You do **not** need to set any of these manually — adding extra `#set`
rules will only fight the template:

- A4 paper, margins 30 / 15 / 20 / 20 mm (left/right/top/bottom).
- Times New Roman 14 pt, 1.5-line spacing, paragraph indent 1.25 cm.
- Hyphenation disabled (СТО 004 8.1.5: «Переносы слов в заголовках не
  допускаются»; СТО 014 5.7 allows hyphenation outside headings — the
  template keeps the stricter labs behavior for both).
- Headings: numbered `1`, `1.1`, `1.1.1`, bold, indented by 1.25 cm,
  followed by one blank line; level-1 headings start a new page.
- Structural headings — marked with the `<s>` label, centered, uppercased,
  un-numbered, do not advance the chapter counter.
  Auto-generated unnumbered headings (`#outline()`, `#bibliography(title: ...)`)
  are styled the same way without needing the label.
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

For structural sections (Введение, Заключение, Перечень сокращений
и обозначений, Термины и определения — anything that, per СТО, must be
centered, uppercased, and excluded from chapter numbering), tag the
heading with the `<s>` label:

```typst
= Введение <s>
...
= Заключение <s>
```

The label is the marker — the title text itself can be anything. The
template will not match by title, so any heading you'd like to style
this way just needs the `<s>` tag. The chapter counter is not advanced
for these headings, so numbered chapters around them stay sequential.

`#outline()` and `#bibliography(title: ...)` produce their own
unnumbered level-1 heading, which is auto-styled the same way — you do
not need to add `<s>` to them.

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

$ R = U / I, $ <eq:ohm>

#where-block(
  ($U$, [напряжение, В]),
  ($I$, [сила тока, А]),
)

Из формулы @eq:ohm видно, что ...
```

The standard requires (СТО 8.4.2): start with «где» on the abzac, no
colon after, each variable on a new line. Use `where-block(...)` — it
produces the properly aligned column. Do **not** glue lines with `\` and
trailing spaces — that breaks alignment.

### Block diagrams (boxes + arrows)

`diagram`, `node`, `edge` are re-exported from `fletcher`. Wrap in
`#figure(...)` to get a standard-compliant «Рисунок N – ...» caption:

```typst
#figure(
  diagram(
    node((0, 0), [Модуль А],   fill: rgb("#dae8fc")),
    node((2, 0), [Модуль Б],   fill: rgb("#d5e8d4")),
    node((1, 1), [Ядро],       fill: rgb("#ffe6cc")),
    node((3, 1), [Тесты],      fill: rgb("#f8cecc")),
    edge((0, 0), (1, 1), "->", [use]),
    edge((2, 0), (1, 1), "->", [use]),
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
  style: "gost-r-7-0-100-2018-numeric.csl",
  title: [Список использованных источников],
)
```

The CSL file `gost-r-7-0-100-2018-numeric.csl` is bundled with the
template (in the same directory as `lab-main.typ` / `practice-main.typ`)
and implements **ГОСТ Р 7.0.100–2018** «Библиографическая запись.
Библиографическое описание. Общие требования и правила составления» —
действующий российский стандарт оформления списка использованных
источников. СТО 006–2025 п. 7.10.2 ссылается именно на этот стандарт
(плюс вспомогательные ГОСТ 7.11, ГОСТ Р 7.0.12, ГОСТ Р 7.0.80). Не
заменяйте на устаревший `gost-r-705-2008-numeric` без явной просьбы
пользователя.

Note: Typst applies the same CSL to both in-text citations and the
bibliography list. СТО 006 формально различает две системы: ссылки
в тексте — по ГОСТ Р 7.0.5 (п. 8.5.2), список — по ГОСТ Р 7.0.100
(п. 7.10.2). Для конкретной страницы ссылки используйте
`#cite(<key>, supplement: [с. 51])` — получится `[1, с. 51]`.

(The bibliography heading is unnumbered by Typst, so it's auto-styled
as a structural heading — no `<s>` needed.)

### Appendices

```typst
= Заключение <s>
...

#bibliography(...)

#show: appendix

= Дополнительные расчёты
Текст приложения А ...

= Графики зависимостей
Текст приложения Б ...
```

Each `=` after `#show: appendix` produces a centered «ПРИЛОЖЕНИЕ Х /
(обязательное) / Название» on a new page (заголовок прописными по
СТО 006–2025 п. 7.11.4). Figures, tables and equations inside become
«А.1», «А.2», «(А.1)», etc.

By default the appendix prints «(обязательное)» under the letter, as
in СТО 004–2020. To switch the status, pass it via `appendix.with`:
`#show: appendix.with(status: "справочное")` — accepts «обязательное»
(default), «рекомендуемое», «справочное» or `none` to omit the line
entirely (СТО 006–2025 п. 7.11.4: статус указывается «при необходимости»).

Available appendix letters: А, Б, В, Г, Д, Е, Ж, И, К, Л, М, Н, П, Р,
С, Т, У, Ф, Х, Ц, Ш, Щ, Э, Ю, Я (без Ё, З, Й, О, Ч, Ъ, Ы, Ь — по СТО
006–2025 п. 7.11.4). Те же буквы используются `enum-letter` для
буквенных перечислений «а)», «б)», … по СТО 006 п. 8.4.2.

## Things to avoid

- **Do not** mix the two families. A lab report uses `lab-report` (with
  `work-number`, `discipline`, `title`); a practice report uses
  `practice-report` (with `kind`, `practice-type`, `period`,
  `direction`, `profile`, `location`). Sending lab-specific fields to
  `practice-report` (or vice versa) is a usage error.
- **Do not** add manual `#set page(...)`, `#set text(...)`, `#set par(...)`
  unless the user asks for a specific deviation. The template's defaults
  are the standard.
- **Do not** number structural sections (Введение, Заключение, …) —
  they are by definition non-numbered. Tag them with `<s>` and the
  template handles the rest.
- **Do not** add the title page manually — `lab-report` / `practice-report`
  emit it.
- **Do not** use em dash «—» for «Рисунок N – ...» / «Таблица N – ...»
  separators — the template already uses en dash «–» per the standard.
  In running prose either dash is fine; match what the user already
  wrote.
- **Do not** rename the appendix supplement — the value `[ПРИЛОЖЕНИЕ]`
  (прописными, по СТО 006–2025 п. 7.11.4) is what the outline rule
  keys on to render «ПРИЛОЖЕНИЕ А Название» in the table of contents.
- **Do not** paste real personal data (names, group numbers, university
  staff) into examples or templates — use `И. И. Иванов`, `П. П. Петров`,
  generic group codes like `101М`, etc.

## Quick reference: standard sections

### Lab report (СТО 004 п. 7.1)

A complete report has, in order:

1. Title page (auto, from `lab-report.with(...)`)
2. `= Цель работы` (or «Цель или задачи работы»)
3. `= Программа и методика эксперимента`
4. `= Результаты измерений (наблюдений)`
5. `= Обработка экспериментальных данных и оценка погрешностей`
6. `= Анализ результатов и выводы` (sometimes labeled `= Выводы` or
   `= Заключение <s>`)
7. Optional `#bibliography(...)` titled «Список использованных источников»
8. Optional `#show: appendix` followed by appendix headings

`#outline()` may be placed right after the title-page block to produce
«Содержание».

### Practice report (СТО 014 п. 5.1.1)

A complete report has:

1. Title page (auto, from `practice-report.with(...)`)
2. Main part — chapters covering the work done per the practice task
3. `= Заключение <s>` — summary of results
4. Optional `#bibliography(...)` titled «Список использованных источников»
5. Optional appendices via `#show: appendix`

Additional documents accompany the report (с собственной нумерацией):
practice task, diary, student survey, supervisor feedback, confirmation
from the host organization. Their generators will be added in future
versions.


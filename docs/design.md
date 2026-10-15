# Quill design notes

Quill is split into a portable core (`src/core`, plain C++17, no Haiku
headers) and the Haiku application (`src/app`). The core is unit tested on
any POSIX system under AddressSanitizer; the application is tested on Haiku
with `hey`.

## Document model (`Document.h`, `Format.h`)

A **story** is a vector of **paragraphs**. The body, the four header and
footer stories and every footnote are stories of their own.

A paragraph holds UTF-8 text without a terminator, a list of **runs**
(start offset and `CharFormat`; runs cover the text, the last one also gives
the format of the paragraph mark) and a `ParaFormat`. Formats are stored by
value; adjacent runs with equal formats are merged after every change.

Positions are `Pos {paragraph, byte offset}`. Special characters inside the
text carry structure, like Word's own text stream does:

| character | meaning |
|---|---|
| U+000B | new line inside a paragraph (Shift-Return) |
| U+000C | manual page break |
| U+E00C | section break (starts a new page) |
| U+E000 to U+E003 | page number, date, time, page count fields |
| U+E004 | footnote reference; `CharFormat::footnote` names the footnote |
| U+FFFC | picture; `CharFormat::picture` names the picture |

Footnotes are numbered by the order of their references in the body. A
footnote story starts with its own reference character, which Word also
does, so the number is drawn by the same layout code.

**Tables** use Word's internal representation: each cell is a paragraph of
kind `kParaCell` and every row ends with a paragraph of kind `kParaRowEnd`
that carries the `RowFormat` (cell edges, gap, indent, height, borders). The
editor never merges cells: deleting across cells clears their text, Return
in a cell inserts a new line, and whole rows can be pasted at the start of a
paragraph.

Every change gives a paragraph a new `revision`. The layout cache is keyed by
revision, so paragraphs that only move (after a Return above them, or when
undo puts old copies back) are not laid out again.

## Editing and undo (`Editor`)

All changes go through a few primitives (`ReplaceRange`, `ApplyCharFormat`,
`ApplyParaFormat`, ...). Before a change the editor copies the paragraphs it
will touch into an **undo step**; undo swaps the saved copies with the
current ones, and the same swap redoes. Typing extends the current step while
it stays inside the paragraphs the step covers, which makes continuous typing
one undo step as in Word. Commands that change many paragraphs or the
document settings and styles (Replace All, Sort, style definitions, Document
dialog) use `BeginBulk`/`EndBulk`, which snapshot the whole story and the
settings once.

When paragraphs are joined, the merged paragraph takes the format of the
paragraph whose mark survives, like Word, except that joining with an empty
paragraph keeps the format of the first one. Applying a style resets the
paragraph format and changes only those character attributes that matched
the old style, so direct formatting survives. Redefining a style updates the
paragraphs and the styles based on it the same way.

## Layout (`Layout`)

`LayoutParagraph` measures characters through the `FontMetrics` interface
(the Haiku implementation caches advance widths per font face and character
in em units), breaks lines greedily after spaces, hyphens, optional hyphens
and dashes, handles left, centred, right, decimal and bar tabs (with the
hanging indent acting as a tab stop and default stops after the last custom
one), aligns and justifies lines, and computes line heights for auto,
at-least and exact spacing with the extra space above the text, as Word
does. The result gives every character an x position, so drawing, hit
testing and caret placement agree exactly. Text is drawn with
`BView::DrawString()` at these explicit positions.

`DocLayout` keeps the paragraph layouts of a story and places every line
twice: once in the continuous **Normal** view and once on a **page**
(page, column, position). Pagination honours page break before, keep lines
together, keep with next, widow and orphan control, manual page and section
breaks, columns, footnote space at the bottom of the page (or footnotes at
the end of the document), table rows (which move as a whole unless taller
than a page, in which case their cells continue on the next page) and mirror
margins and gutters. Geometry functions turn positions into caret
rectangles and selection rectangles and points back into positions, for both
views.

**Outline view** is a layout option: paragraph formatting is replaced by an
indent per heading level, collapsed or filtered paragraphs get zero-height
lines that are skipped by drawing, hit testing and the arrow keys, and the
story is not paginated.

## RTF (`RTF`)

The writer produces RTF 1 with a font table, colour table, style sheet,
document information, page and section properties, headers and footers,
footnotes (`\chftn` and `\footnote`), fields (`\chpgn`, `\chdate`,
`\chtime`), pictures (`\pngblip`, `\jpegblip`), tables (`\trowd`, `\cellx`,
`\cell`, `\row`) and Unicode text as `\uN?`. Fragments for the clipboard
leave the last paragraph open so that pasting part of a paragraph merges
it, like pasting in Word.

The reader is a state machine with a group stack. It understands the code
pages Word used (`\mac`, `\ansi`, `\pc`, `\ansicpg`), `\uN` with `\uc`
fallbacks and surrogate pairs, `\'hh`, symbol fonts, style sheets (mapped to
Quill's style indices by number, with Normal first and Quill's standard
styles added when missing), `\pntext` and `\listtext` (numbering text of
later Word versions), fields, and skips the destinations it does not know.
`ImportStory` copies a story from one document to another, matching styles
by name and giving pictures and footnotes new identifiers; paste, glossary
entries and Insert File use it.

## The application

`DocView` draws a story into an off-screen bitmap and copies the update
rectangle to the screen, so typing does not flicker. Normal and Outline
view draw lines from the continuous layout; Page Layout view, Print Preview
and printing all use `PageRenderer`, which draws the paper, headers and
footers (laid out per page so page number fields are right), table grids,
the body lines of the page and its footnotes. The caret is drawn with
`B_OP_INVERT` after the copy, so blinking only redraws a few lines.

`DocWindow` owns the document and builds Word's menus. Every dialog is a
window of its own that receives copies of the formats and sends the result
back as a message, so the document is only touched from the document
window's thread. Headers, footers and footnotes are edited in a pane at the
bottom of the same window for the same reason. Printing makes a copy of the
document and prints it from another thread, because the print system's
dialogs block the calling thread.

The window understands a `Qcmd` scripting message (`hey Quill Qcmd of
Window 0 with cmd="..."`) for the tests: typing, synthetic key presses
(posted to the preferred handler so that shortcuts work as for real keys),
selections, information about formats and pages, posting menu commands and
saving.

## Icons

The toolbar, ribbon and ruler pictures are drawn from small pixel-art and
shape definitions in `Icons.cpp` (scaled up for large UI fonts). The
application icon is generated as HVIF by `tools/make_icon.py`.

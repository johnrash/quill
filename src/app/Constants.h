/*
 * Quill - message codes and shared constants.
 */
#ifndef QUILL_CONSTANTS_H
#define QUILL_CONSTANTS_H

#include <SupportDefs.h>

#define APP_SIGNATURE "application/x-vnd.Quill"
#define APP_NAME "Quill"

enum {
	// File
	kMsgNew					= 'Fnew',
	kMsgOpen				= 'Fopn',
	kMsgClose				= 'Fcls',
	kMsgSave				= 'Fsav',
	kMsgSaveAs				= 'Fsva',
	kMsgSummaryInfo			= 'Fsum',
	kMsgPrintPreview		= 'Fprv',
	kMsgPageSetup			= 'Fpgs',
	kMsgPrint				= 'Fprt',
	kMsgOpenRecent			= 'Frec',
	kMsgSaveFormat			= 'Fsfm',

	// Edit
	kMsgUndo				= 'Eund',
	kMsgRedo				= 'Ered',
	kMsgRepeat				= 'Erep',
	kMsgClear				= 'Eclr',
	kMsgPasteSpecial		= 'Epsp',
	kMsgFind				= 'Efnd',
	kMsgFindAgain			= 'Efag',
	kMsgReplace				= 'Erpl',
	kMsgGoTo				= 'Egto',
	kMsgGlossary			= 'Eglo',

	// View
	kMsgViewNormal			= 'Vnrm',
	kMsgViewOutline			= 'Voln',
	kMsgViewPage			= 'Vpag',
	kMsgToggleToolbar		= 'Vtlb',
	kMsgToggleRibbon		= 'Vrib',
	kMsgToggleRuler			= 'Vrul',
	kMsgViewHeader			= 'Vhdr',
	kMsgViewFooter			= 'Vftr',
	kMsgViewFirstHeader		= 'Vfhd',
	kMsgViewFirstFooter		= 'Vfft',
	kMsgViewFootnotes		= 'Vfnt',
	kMsgShowAll				= 'Vshw',
	kMsgZoom				= 'Vzom',
	kMsgZoomTo				= 'Vzto',
	kMsgClosePane			= 'Vcpn',
	kMsgOutline				= 'Votl',	// "op", "level"

	// Insert
	kMsgInsertPageBreak		= 'Ipgb',
	kMsgInsertSectionBreak	= 'Isec',
	kMsgInsertTable			= 'Itbl',
	kMsgInsertFootnote		= 'Ifnt',
	kMsgInsertDate			= 'Idat',
	kMsgInsertTime			= 'Itim',
	kMsgInsertPageNumber	= 'Ipgn',
	kMsgInsertSymbol		= 'Isym',
	kMsgInsertPicture		= 'Ipic',
	kMsgInsertFile			= 'Ifil',
	kMsgInsertTOC			= 'Itoc',
	kMsgInsertChar			= 'Ichr',

	// Format
	kMsgCharacter			= 'Ochr',
	kMsgParagraph			= 'Opar',
	kMsgSection				= 'Osec',
	kMsgDocument			= 'Odoc',
	kMsgBorder				= 'Obrd',
	kMsgTabs				= 'Otab',
	kMsgTableLayout			= 'Otly',
	kMsgStyle				= 'Osty',
	kMsgRevertToStyle		= 'Orvt',
	kMsgChangeCase			= 'Ocas',
	kMsgPlainText			= 'Opln',
	kMsgToggleChar			= 'Otgl',	// "field": CharField
	kMsgUnderlineKind		= 'Oulk',	// "kind"
	kMsgSuperscript			= 'Osup',
	kMsgSubscript			= 'Osub',
	kMsgAlign				= 'Oaln',	// "align"
	kMsgLineSpacing			= 'Olsp',	// "value"
	kMsgParaSpacing			= 'Opsp',	// "before"
	kMsgColumns				= 'Ocol',	// "columns"
	kMsgColor				= 'Oclr',	// "color"
	kMsgNest				= 'Onst',	// "in"
	kMsgHanging				= 'Ohng',

	// Font
	kMsgFontSize			= 'Tsiz',	// "size"
	kMsgFontUp				= 'Tup ',
	kMsgFontDown			= 'Tdwn',
	kMsgFontFamily			= 'Tfam',	// "family"

	// Tools
	kMsgSpelling			= 'Tspl',
	kMsgWordCount			= 'Twrd',
	kMsgRenumber			= 'Tren',
	kMsgBullets				= 'Tbul',
	kMsgNumbers				= 'Tnum',
	kMsgSort				= 'Tsrt',
	kMsgSortDescending		= 'Tsrd',
	kMsgCalculate			= 'Tcal',
	kMsgRepaginate			= 'Trpg',
	kMsgPreferences			= 'Tprf',
	kMsgHyphenate			= 'Thyp',

	// Window and Help
	kMsgShowClipboard		= 'Wclp',
	kMsgArrange				= 'Warr',
	kMsgActivateWindow		= 'Wact',
	kMsgNewWindow			= 'Wnew',
	kMsgAbout				= 'Habt',
	kMsgHelp				= 'Hhlp',
	kMsgKeyboardHelp		= 'Hkey',

	// Internal
	kMsgApplyCharacter		= 'Achr',	// "format" (flattened), "mask"
	kMsgApplyParagraph		= 'Apar',
	kMsgApplyStyle			= 'Asty',	// "style"
	kMsgApplyDocument		= 'Adoc',
	kMsgApplyTabs			= 'Atab',
	kMsgApplyBorder			= 'Abrd',
	kMsgApplyIndents		= 'Aind',	// from the ruler
	kMsgApplyRow			= 'Arow',
	kMsgApplyMargins		= 'Amrg',
	kMsgDocumentChanged		= 'Dchg',
	kMsgSelectionChanged	= 'Dsel',
	kMsgAutoScroll			= 'Dasc',
	kMsgTabType				= 'Rtab',	// ruler tab button
	kMsgRulerScale			= 'Rscl',
	kMsgSaveChoice			= 'Dsvc',
	kMsgFindNext			= 'Dfnx',	// from the Find dialog
	kMsgReplaceOne			= 'Drp1',
	kMsgReplaceAll			= 'Drpa',
	kMsgSpellCommand		= 'Dspc',
	kMsgGlossaryCommand		= 'Dglc',
	kMsgDialogClosed		= 'Ddcl',
	kMsgPreviewCommand		= 'Dprv',
	kMsgTitleChanged		= 'Dttl',
	kMsgWindowsChanged		= 'Dwch',
	kMsgPrefsChanged		= 'Dpch',
	kMsgSetStatus			= 'Dsts',
	kMsgPrintDone			= 'Dprd',
	kMsgScript				= 'Qcmd'	// test scripting: "cmd" string
};

enum ViewMode {
	kModeNormal = 0,
	kModeOutline,
	kModePage,
	kModePreview
};

enum SaveFormat {
	kFormatRTF = 0,
	kFormatText,
	kFormatTextLineBreaks
};

#endif	// QUILL_CONSTANTS_H

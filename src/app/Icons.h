/*
 * Quill - bitmaps for the Toolbar, Ribbon and Ruler buttons.
 */
#ifndef QUILL_ICONS_H
#define QUILL_ICONS_H

#include <Bitmap.h>

enum IconID {
	kIconNew = 0,
	kIconOpen,
	kIconSave,
	kIconPrint,
	kIconCut,
	kIconCopy,
	kIconPaste,
	kIconUndo,
	kIconBullets,
	kIconNumbers,
	kIconUnindent,
	kIconIndent,
	kIconTable,
	kIconColumns,
	kIconPicture,
	kIconSpelling,
	kIconShowAll,
	kIconNormalView,
	kIconPageView,
	kIconPreview,
	kIconZoom,
	kIconFootnote,
	kIconAlignLeft,
	kIconAlignCenter,
	kIconAlignRight,
	kIconAlignJustify,
	kIconSpaceSingle,
	kIconSpaceOneHalf,
	kIconSpaceDouble,
	kIconParaClosed,
	kIconParaOpen,
	kIconTabLeft,
	kIconTabCenter,
	kIconTabRight,
	kIconTabDecimal,
	kIconTabBar,
	kIconScale,
	kIconColumns1,
	kIconColumns2,
	kIconColumns3,
	kIconSuperscript,
	kIconSubscript,
	kIconPageNumber,
	kIconDate,
	kIconTime,
	kIconMargins,
	kIconOnePage,
	kIconTwoPages,
	kIconClose,
	kIconPromote,
	kIconDemote,
	kIconToBody,
	kIconMoveUp,
	kIconMoveDown,
	kIconExpand,
	kIconCollapse,
	kIconFirstLine,
	kIconFormatting,
	kIconCount
};

BBitmap* GetIcon(int id);
int IconScale();

#endif	// QUILL_ICONS_H

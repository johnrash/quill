/*
 * Quill - editing operations on a story with undo.
 *
 * Every change goes through ReplaceParagraphs-style snapshots: before an
 * operation the affected paragraphs are copied into an undo step, and undo
 * swaps them back. The same step then holds what redo needs.
 */
#ifndef QUILL_EDITOR_H
#define QUILL_EDITOR_H

#include "Document.h"

#include <functional>
#include <string>
#include <vector>

namespace quill {

enum UndoKind {
	kUndoOther = 0,
	kUndoTyping
};

struct UndoStep {
	std::string	name;
	int			kind = kUndoOther;
	int32_t		first = 0;
	int32_t		count = 0;			// paragraphs currently in the story
	std::vector<Paragraph> saved;	// paragraphs to swap in
	Pos			beforeAnchor, beforeCaret;
	Pos			afterAnchor, afterCaret;
	bool		hasDocument = false;
	DocSettings	settings;
	StyleSheet	styles;
	DocInfo		info;
};

enum CaseChange {
	kCaseUpper = 0,
	kCaseLower,
	kCaseTitle,
	kCaseSentence,
	kCaseToggle
};

class Editor {
public:
							Editor(Document* document, Story* story);

			Document*		GetDocument() const { return fDocument; }
			Story&			GetStory() const { return *fStory; }
			bool			IsBody() const
								{ return fStory == &fDocument->body; }

	// Selection
			Pos				Anchor() const { return fAnchor; }
			Pos				Caret() const { return fCaret; }
			Pos				SelStart() const
								{ return fAnchor < fCaret ? fAnchor : fCaret; }
			Pos				SelEnd() const
								{ return fAnchor < fCaret ? fCaret : fAnchor; }
			bool			HasSelection() const { return fAnchor != fCaret; }
			void			Select(Pos anchor, Pos caret);
			void			SetCaret(Pos pos) { Select(pos, pos); }
			void			SelectAll();
			Pos				Clamp(Pos pos) const;
			Pos				End() const;
			int32_t			FirstSelectedPara() const;
			int32_t			LastSelectedPara() const;

	// Insertion format (what typing will produce)
			CharFormat		InsertionFormat() const;
			void			SetTypingFormat(const CharFormat& format);
			bool			HasTypingFormat() const { return fTypingSet; }

	// Current formats for menus, ribbon and ruler. sameMask gets the
	// fields that are identical across the whole selection.
			CharFormat		SelectionCharFormat(uint32_t* sameMask) const;
			ParaFormat		SelectionParaFormat(uint32_t* sameMask) const;

	// Editing
			void			InsertText(const std::string& text);
			void			InsertParagraphBreak(bool keepStyle = false);
			void			InsertSpecial(uint32_t c,
								const CharFormat* format = nullptr,
								const char* undoName = "Typing");
			void			Backspace(bool word);
			void			DeleteForward(bool word);
			void			DeleteSelection(const char* undoName);
			Story			CopyRange(Pos start, Pos end) const;
			Story			CopySelection() const;
			Pos				ReplaceRange(Pos start, Pos end,
								const Story& fragment, const char* undoName,
								bool selectResult = false);
			void			ReplaceSelection(const Story& fragment,
								const char* undoName,
								bool selectResult = false);
			std::string		SelectedText() const;

	// Formatting
			void			ApplyCharFormat(
								const std::function<void(CharFormat&)>& modify,
								const char* undoName = "Formatting");
			void			ApplyCharFields(const CharFormat& values,
								uint32_t mask,
								const char* undoName = "Formatting");
			void			ToggleCharField(uint32_t field);
			void			ApplyParaFormat(
								const std::function<void(ParaFormat&)>& modify,
								const char* undoName = "Formatting");
			void			ApplyStyle(int32_t style);
			void			RevertToStyle();
			void			PlainText();
			void			ChangeCase(int mode);
			void			RedefineStyle(int32_t index, const Style& style,
								const char* undoName = "Style");
			int32_t			AddStyle(const Style& style);
			void			DeleteStyle(int32_t index);

	// Compound operations (Replace All, Sort, ...): one undo step for a
	// series of changes anywhere in the story or the document settings.
			void			BeginBulk(const char* undoName,
								bool includeDocument = false);
			void			EndBulk();
			void			DocumentChanged(const char* undoName,
								const DocSettings& oldSettings);

	// Undo
			bool			CanUndo() const { return !fUndo.empty(); }
			bool			CanRedo() const { return !fRedo.empty(); }
			std::string		UndoName() const;
			std::string		RedoName() const;
			void			Undo();
			void			Redo();
			void			ClearUndo();
			void			BreakTypingSequence() { fCoalesce = false; }
			const std::string& LastTyped() const { return fLastTyped; }

	// Navigation that does not need the layout
			Pos				NextChar(Pos pos) const;
			Pos				PrevChar(Pos pos) const;
			Pos				NextWord(Pos pos) const;
			Pos				PrevWord(Pos pos) const;
			Pos				NextParagraph(Pos pos) const;
			Pos				PrevParagraph(Pos pos) const;
			Pos				NextSentence(Pos pos) const;
			Pos				PrevSentence(Pos pos) const;
			void			WordAt(Pos pos, Pos* start, Pos* end) const;
			void			SentenceAt(Pos pos, Pos* start, Pos* end) const;
			void			ParagraphAt(Pos pos, Pos* start, Pos* end) const;

	// Change tracking for views
			uint64_t		ChangeCount() const { return fChangeCount; }
			void			MarkChanged();

private:
			void			_BeginStep(const char* name, int kind,
								int32_t first, int32_t last);
			void			_EndStep(int32_t newLast);
			void			_Swap(UndoStep& step);
			Pos				_ReplaceRaw(Pos start, Pos end,
								const Story& fragment);
			Pos				_ReplaceInTable(Pos start, Pos end,
								const Story& fragment);
			Story			_Sanitize(const Story& fragment, Pos at) const;
			void			_ApplyStyleToParagraph(Paragraph& paragraph,
								int32_t style);
			void			_UpdateStyleUsers(Story& story, int32_t index,
								const Style& oldStyle, const Style& newStyle);
			bool			_InBulk() const { return fBulkDepth > 0; }

			Document*		fDocument;
			Story*			fStory;
			Pos				fAnchor;
			Pos				fCaret;
			CharFormat		fTypingFormat;
			bool			fTypingSet;

			std::vector<UndoStep> fUndo;
			std::vector<UndoStep> fRedo;
			bool			fCoalesce;
			int32_t			fPendingFirst;
			int32_t			fPendingCount;
			int32_t			fPendingNewLast;
			bool			fPendingCoalesced;
			int				fBulkDepth;
			std::string		fLastTyped;
			uint64_t		fChangeCount;
};

}	// namespace quill

#endif	// QUILL_EDITOR_H

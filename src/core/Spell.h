/*
 * Quill - spelling checker.
 *
 * Uses word lists in the ispell format Haiku ships in
 * /boot/system/data/spell_check/word_dictionary ("word/FLAGS", with the
 * standard English affix flags), plus a user dictionary.
 */
#ifndef QUILL_SPELL_H
#define QUILL_SPELL_H

#include "Document.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace quill {

class SpellChecker {
public:
							SpellChecker();

			bool			LoadWordList(const std::string& path);
			void			AddRoot(const std::string& entry);	// "word/FLAGS"
			void			AddWord(const std::string& word);
			void			RemoveWord(const std::string& word);
			size_t			WordCount() const { return fExact.size(); }
			bool			IsLoaded() const { return !fExact.empty(); }

			bool			IsCorrect(const std::string& word) const;
			std::vector<std::string> Suggest(const std::string& word,
								size_t maximum = 12) const;

			void			IgnoreAll(const std::string& word);
			void			ClearIgnored() { fIgnored.clear(); }
			bool			IsIgnored(const std::string& word) const;

			bool			ignoreUppercase;
			bool			ignoreWithNumbers;

	// Finds the next word that is not in the dictionary at or after from.
			bool			NextMisspelling(const Story& story, Pos from,
								Pos limit, Pos* start, Pos* end,
								std::string* word) const;

private:
			void			_Insert(const std::string& word);
			bool			_Known(const std::string& word) const;

			std::unordered_set<std::string> fExact;
			std::unordered_set<std::string> fLower;	// all-lowercase entries
			std::unordered_set<std::string> fUpper;	// uppercased entries
			std::unordered_set<std::string> fIgnored;
};

}	// namespace quill

#endif	// QUILL_SPELL_H

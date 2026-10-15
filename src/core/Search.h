/*
 * Quill - Find and Replace.
 *
 * Patterns use Word's special characters: ^t tab, ^p paragraph mark,
 * ^n new line, ^d page or section break, ^s nonbreaking space, ^- optional
 * hyphen, ^~ nonbreaking hyphen, ^w white space, ^? any character,
 * ^# any digit, ^$ any letter, ^^ caret, ^nnn character number nnn.
 */
#ifndef QUILL_SEARCH_H
#define QUILL_SEARCH_H

#include "Document.h"

#include <string>
#include <vector>

namespace quill {

struct SearchOptions {
	bool	matchCase = false;
	bool	wholeWord = false;
};

class SearchPattern {
public:
							SearchPattern(const std::string& pattern,
								const SearchOptions& options);
			bool			IsValid() const { return !fTokens.empty(); }

	// Finds the next (forward) or previous match starting at from.
			bool			Find(const Story& story, Pos from, bool forward,
								Pos limit, Pos* start, Pos* end) const;

	struct Token {
		int			kind;
		uint32_t	c;
	};

private:
			int				_MatchAt(const std::vector<uint32_t>& text,
								size_t index, size_t token) const;

			std::vector<Token> fTokens;
			SearchOptions	fOptions;
};

// Expands the special characters of a replacement string; "^p" becomes a
// paragraph mark ("\n"), "^n" a line break.
std::string ExpandReplacement(const std::string& text);

}	// namespace quill

#endif	// QUILL_SEARCH_H

/*
 * Quill - Find and Replace.
 */
#include "Search.h"
#include "Text.h"

#include <cstdlib>

namespace quill {

enum TokenKind {
	kTokenChar = 0,
	kTokenAny,
	kTokenDigit,
	kTokenLetter,
	kTokenWhite,
	kTokenBreak		// page or section break
};

static const uint32_t kParagraphMark = '\n';


static bool
ParseSpecial(const std::string& pattern, size_t* index, uint32_t* c,
	int* kind)
{
	// pattern[*index] == '^'
	size_t i = *index + 1;
	if (i >= pattern.size()) {
		*c = '^';
		*kind = kTokenChar;
		*index = i;
		return true;
	}
	char code = pattern[i];
	*kind = kTokenChar;
	switch (code) {
		case 't': *c = '\t'; break;
		case 'p': *c = kParagraphMark; break;
		case 'n': *c = kLineBreakChar; break;
		case 'd': *kind = kTokenBreak; *c = kPageBreakChar; break;
		case 's': *c = kNbspChar; break;
		case '-': *c = kSoftHyphenChar; break;
		case '~': *c = kNbHyphenChar; break;
		case 'w': *kind = kTokenWhite; *c = ' '; break;
		case '?': *kind = kTokenAny; *c = 0; break;
		case '#': *kind = kTokenDigit; *c = 0; break;
		case '$': *kind = kTokenLetter; *c = 0; break;
		case '^': *c = '^'; break;
		default:
			if (code >= '0' && code <= '9') {
				size_t end = i;
				while (end < pattern.size() && end < i + 5
					&& pattern[end] >= '0' && pattern[end] <= '9')
					end++;
				*c = (uint32_t)atoi(pattern.substr(i, end - i).c_str());
				if (*c >= 0x80 && *c < 0x100)
					*c = MacRomanToUnicode((unsigned char)*c);
				*index = end;
				return true;
			}
			*c = '^';
			*index = i;
			return true;
	}
	*index = i + 1;
	return true;
}


SearchPattern::SearchPattern(const std::string& pattern,
	const SearchOptions& options)
	:
	fOptions(options)
{
	size_t i = 0;
	while (i < pattern.size()) {
		Token token;
		if (pattern[i] == '^') {
			ParseSpecial(pattern, &i, &token.c, &token.kind);
		} else {
			int32_t consumed;
			token.c = DecodeUtf8(pattern.c_str() + i,
				(int32_t)(pattern.size() - i), &consumed);
			token.kind = kTokenChar;
			i += std::max<int32_t>(1, consumed);
		}
		if (token.kind == kTokenChar && !options.matchCase)
			token.c = ToLower(token.c);
		fTokens.push_back(token);
	}
}


static bool
IsWhite(uint32_t c)
{
	return c == ' ' || c == '\t' || c == kNbspChar;
}


int
SearchPattern::_MatchAt(const std::vector<uint32_t>& text, size_t index,
	size_t token) const
{
	// Returns the length of the match or -1.
	size_t position = index;
	for (size_t t = token; t < fTokens.size(); t++) {
		const Token& expected = fTokens[t];
		if (expected.kind == kTokenWhite) {
			size_t end = position;
			while (end < text.size() && IsWhite(text[end]))
				end++;
			if (end == position)
				return -1;
			// greedy with backtracking
			for (size_t stop = end; stop > position; stop--) {
				int rest = _MatchAt(text, stop, t + 1);
				if (rest >= 0)
					return (int)(stop - index) + rest;
			}
			return -1;
		}
		if (position >= text.size())
			return -1;
		uint32_t c = text[position];
		switch (expected.kind) {
			case kTokenAny:
				if (c == kParagraphMark)
					return -1;
				break;
			case kTokenDigit:
				if (!IsDigit(c))
					return -1;
				break;
			case kTokenLetter:
				if (!IsLetter(c))
					return -1;
				break;
			case kTokenBreak:
				if (c != kPageBreakChar && c != kSectionBreakChar)
					return -1;
				break;
			default: {
				uint32_t compare = fOptions.matchCase ? c : ToLower(c);
				if (compare != expected.c)
					return -1;
				break;
			}
		}
		position++;
	}
	return (int)(position - index);
}


bool
SearchPattern::Find(const Story& story, Pos from, bool forward, Pos limit,
	Pos* start, Pos* end) const
{
	if (fTokens.empty() || story.empty())
		return false;

	// Flatten the story into characters with paragraph marks.
	std::vector<uint32_t> text;
	std::vector<Pos> positions;
	for (int32_t p = 0; p < (int32_t)story.size(); p++) {
		const std::string& paragraph = story[p].text;
		int32_t length = (int32_t)paragraph.size();
		for (int32_t offset = 0; offset < length;) {
			int32_t consumed;
			uint32_t c = DecodeUtf8(paragraph.c_str() + offset,
				length - offset, &consumed);
			text.push_back(c);
			positions.push_back(Pos(p, offset));
			offset += std::max<int32_t>(1, consumed);
		}
		text.push_back(kParagraphMark);
		positions.push_back(Pos(p, length));
	}
	positions.push_back(Document::EndOf(story));

	auto indexOf = [&](Pos pos) -> size_t {
		size_t low = 0;
		size_t high = positions.size() - 1;
		while (low < high) {
			size_t middle = (low + high) / 2;
			if (positions[middle] < pos)
				low = middle + 1;
			else
				high = middle;
		}
		return low;
	};

	size_t first = indexOf(from);
	size_t stop = indexOf(limit);
	auto wordBoundary = [&](size_t index, size_t length) {
		if (!fOptions.wholeWord)
			return true;
		if (index > 0 && IsWordChar(text[index - 1]))
			return false;
		size_t after = index + length;
		if (after < text.size() && IsWordChar(text[after]))
			return false;
		return true;
	};

	if (forward) {
		for (size_t i = first; i < text.size() && i < stop; i++) {
			int length = _MatchAt(text, i, 0);
			if (length > 0 && i + (size_t)length <= stop
				&& wordBoundary(i, length)) {
				*start = positions[i];
				*end = positions[i + length];
				return true;
			}
		}
	} else {
		size_t i = first;
		while (i > stop) {
			i--;
			int length = _MatchAt(text, i, 0);
			if (length > 0 && i + length <= first && wordBoundary(i, length)) {
				*start = positions[i];
				*end = positions[i + length];
				return true;
			}
		}
	}
	return false;
}


std::string
ExpandReplacement(const std::string& text)
{
	std::string result;
	size_t i = 0;
	while (i < text.size()) {
		if (text[i] != '^') {
			result += text[i++];
			continue;
		}
		uint32_t c;
		int kind;
		ParseSpecial(text, &i, &c, &kind);
		if (kind != kTokenChar && kind != kTokenBreak)
			continue;	// wildcards make no sense in a replacement
		AppendUtf8(result, c);
	}
	return result;
}

}	// namespace quill

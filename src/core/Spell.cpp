/*
 * Quill - spelling checker with ispell affix expansion.
 */
#include "Spell.h"
#include "Text.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>

namespace quill {

namespace {

struct SuffixRule {
	const char*	condition;
	const char*	strip;
	const char*	add;
};

struct SuffixFlag {
	char		flag;
	bool		cross;
	SuffixRule	rules[4];
};

// The standard English affix rules of ispell (english.aff).
const SuffixFlag kSuffixes[] = {
	{ 'V', false, { { "E", "E", "IVE" }, { "[^E]", "", "IVE" } } },
	{ 'N', true, { { "E", "E", "ION" }, { "Y", "Y", "ICATION" },
		{ "[^EY]", "", "EN" } } },
	{ 'X', true, { { "E", "E", "IONS" }, { "Y", "Y", "ICATIONS" },
		{ "[^EY]", "", "ENS" } } },
	{ 'H', false, { { "Y", "Y", "IETH" }, { "[^Y]", "", "TH" } } },
	{ 'Y', true, { { ".", "", "LY" } } },
	{ 'G', true, { { "E", "E", "ING" }, { "[^E]", "", "ING" } } },
	{ 'J', true, { { "E", "E", "INGS" }, { "[^E]", "", "INGS" } } },
	{ 'D', true, { { "E", "", "D" }, { "[^AEIOU]Y", "Y", "IED" },
		{ "[AEIOU]Y", "", "ED" }, { "[^EY]", "", "ED" } } },
	{ 'T', false, { { "E", "", "ST" }, { "[^AEIOU]Y", "Y", "IEST" },
		{ "[AEIOU]Y", "", "EST" }, { "[^EY]", "", "EST" } } },
	{ 'R', true, { { "E", "", "R" }, { "[^AEIOU]Y", "Y", "IER" },
		{ "[AEIOU]Y", "", "ER" }, { "[^EY]", "", "ER" } } },
	{ 'Z', true, { { "E", "", "RS" }, { "[^AEIOU]Y", "Y", "IERS" },
		{ "[AEIOU]Y", "", "ERS" }, { "[^EY]", "", "ERS" } } },
	{ 'S', true, { { "[^AEIOU]Y", "Y", "IES" }, { "[AEIOU]Y", "", "S" },
		{ "[SXZH]", "", "ES" }, { "[^SXZHY]", "", "S" } } },
	{ 'P', true, { { "[^AEIOU]Y", "Y", "INESS" }, { "[AEIOU]Y", "", "NESS" },
		{ "[^Y]", "", "NESS" } } },
	{ 'M', true, { { ".", "", "'S" } } },
};

struct PrefixFlag {
	char		flag;
	const char*	add;
};

const PrefixFlag kPrefixes[] = {
	{ 'A', "RE" }, { 'I', "IN" }, { 'U', "UN" }, { 'C', "DE" },
	{ 'E', "DIS" }, { 'F', "CON" }, { 'K', "PRO" }
};


// Matches an ispell condition like "[^AEIOU]Y" against the end of word.
bool
ConditionMatches(const char* condition, const std::string& upper)
{
	std::vector<std::pair<std::string, bool> > classes;
	for (const char* c = condition; *c;) {
		if (*c == '[') {
			bool negate = c[1] == '^';
			const char* start = c + (negate ? 2 : 1);
			const char* end = strchr(start, ']');
			if (end == nullptr)
				return false;
			classes.push_back(std::make_pair(std::string(start, end - start),
				negate));
			c = end + 1;
		} else if (*c == '.') {
			classes.push_back(std::make_pair(std::string(), true));
			c++;
		} else {
			classes.push_back(std::make_pair(std::string(1, *c), false));
			c++;
		}
	}
	if (classes.size() > upper.size())
		return false;
	size_t base = upper.size() - classes.size();
	for (size_t i = 0; i < classes.size(); i++) {
		char letter = upper[base + i];
		bool inSet = classes[i].first.find(letter) != std::string::npos;
		if (classes[i].second ? inSet : !inSet)
			return false;
	}
	return true;
}


bool
IsAllUpper(const std::string& word)
{
	bool letters = false;
	int32_t length = (int32_t)word.size();
	for (int32_t offset = 0; offset < length;) {
		int32_t consumed;
		uint32_t c = DecodeUtf8(word.c_str() + offset, length - offset,
			&consumed);
		offset += std::max<int32_t>(1, consumed);
		if (IsLower(c))
			return false;
		if (IsUpper(c))
			letters = true;
	}
	return letters;
}


bool
IsAllLower(const std::string& word)
{
	int32_t length = (int32_t)word.size();
	for (int32_t offset = 0; offset < length;) {
		int32_t consumed;
		uint32_t c = DecodeUtf8(word.c_str() + offset, length - offset,
			&consumed);
		offset += std::max<int32_t>(1, consumed);
		if (IsUpper(c))
			return false;
	}
	return true;
}


std::string
AsciiUpper(const std::string& text)
{
	std::string result = text;
	for (char& c : result)
		c = (char)toupper((unsigned char)c);
	return result;
}


std::string
MatchCase(const std::string& affix, bool upper)
{
	if (upper)
		return affix;
	std::string result = affix;
	for (char& c : result)
		c = (char)tolower((unsigned char)c);
	return result;
}

}	// namespace


SpellChecker::SpellChecker()
	:
	ignoreUppercase(true),
	ignoreWithNumbers(true)
{
}


void
SpellChecker::_Insert(const std::string& word)
{
	if (word.empty())
		return;
	fExact.insert(word);
	if (IsAllLower(word))
		fLower.insert(word);
	fUpper.insert(ToUpperString(word));
}


void
SpellChecker::AddRoot(const std::string& entry)
{
	size_t slash = entry.find('/');
	std::string root = entry.substr(0, slash);
	while (!root.empty() && (root.back() == '\r' || root.back() == ' '))
		root.pop_back();
	if (root.empty())
		return;
	_Insert(root);
	if (slash == std::string::npos)
		return;
	std::string flags = entry.substr(slash + 1);
	bool upper = root.size() > 1 && IsAllUpper(root);
	bool capitalized = !IsAllLower(root);
	std::string rootUpper = AsciiUpper(root);

	std::vector<std::string> suffixed;	// forms that may take prefixes
	for (char flag : flags) {
		for (const SuffixFlag& suffix : kSuffixes) {
			if (suffix.flag != flag)
				continue;
			for (const SuffixRule& rule : suffix.rules) {
				if (rule.condition == nullptr)
					break;
				if (!ConditionMatches(rule.condition, rootUpper))
					continue;
				std::string form = root;
				size_t strip = strlen(rule.strip);
				if (strip > 0)
					form.resize(form.size() - std::min(strip, form.size()));
				form += MatchCase(rule.add, upper);
				_Insert(form);
				if (suffix.cross)
					suffixed.push_back(form);
				break;
			}
		}
	}
	if (capitalized)
		return;
	for (char flag : flags) {
		for (const PrefixFlag& prefix : kPrefixes) {
			if (prefix.flag != flag)
				continue;
			std::string add = MatchCase(prefix.add, upper);
			_Insert(add + root);
			for (const std::string& form : suffixed)
				_Insert(add + form);
		}
	}
}


bool
SpellChecker::LoadWordList(const std::string& path)
{
	std::ifstream file(path.c_str());
	if (!file)
		return false;
	std::string line;
	while (std::getline(file, line)) {
		if (line.empty() || line[0] == '#')
			continue;
		AddRoot(line);
	}
	return true;
}


void
SpellChecker::AddWord(const std::string& word)
{
	_Insert(word);
}


void
SpellChecker::RemoveWord(const std::string& word)
{
	fExact.erase(word);
	fLower.erase(word);
}


void
SpellChecker::IgnoreAll(const std::string& word)
{
	fIgnored.insert(word);
}


bool
SpellChecker::IsIgnored(const std::string& word) const
{
	return fIgnored.find(word) != fIgnored.end();
}


bool
SpellChecker::_Known(const std::string& word) const
{
	if (fExact.count(word))
		return true;
	if (fLower.count(ToLowerString(word)))
		return true;
	if (word.size() > 1 && IsAllUpper(word) && fUpper.count(word))
		return true;
	return false;
}


static std::string
NormalizeApostrophes(const std::string& word)
{
	std::string result;
	int32_t length = (int32_t)word.size();
	for (int32_t offset = 0; offset < length;) {
		int32_t consumed;
		uint32_t c = DecodeUtf8(word.c_str() + offset, length - offset,
			&consumed);
		offset += std::max<int32_t>(1, consumed);
		AppendUtf8(result, c == 0x2019 || c == 0x2018 ? '\'' : c);
	}
	while (!result.empty() && result.back() == '\'')
		result.pop_back();
	while (!result.empty() && result.front() == '\'')
		result.erase(0, 1);
	return result;
}


bool
SpellChecker::IsCorrect(const std::string& original) const
{
	std::string word = NormalizeApostrophes(original);
	if (word.empty() || CountChars(word) <= 1)
		return true;
	if (IsIgnored(word) || IsIgnored(original))
		return true;
	bool digits = false;
	for (char c : word) {
		if (isdigit((unsigned char)c))
			digits = true;
	}
	if (digits)
		return ignoreWithNumbers;
	if (ignoreUppercase && IsAllUpper(word))
		return true;
	if (fExact.empty())
		return true;	// no dictionary: do not complain about everything
	if (_Known(word))
		return true;
	// possessives of known words
	if (word.size() > 2 && (word.compare(word.size() - 2, 2, "'s") == 0
		|| word.compare(word.size() - 2, 2, "'S") == 0)) {
		if (_Known(word.substr(0, word.size() - 2)))
			return true;
	}
	return false;
}


std::vector<std::string>
SpellChecker::Suggest(const std::string& original, size_t maximum) const
{
	std::string word = NormalizeApostrophes(original);
	std::vector<uint32_t> letters;
	{
		std::string lower = ToLowerString(word);
		int32_t length = (int32_t)lower.size();
		for (int32_t offset = 0; offset < length;) {
			int32_t consumed;
			letters.push_back(DecodeUtf8(lower.c_str() + offset,
				length - offset, &consumed));
			offset += std::max<int32_t>(1, consumed);
		}
	}
	auto encode = [](const std::vector<uint32_t>& cps) {
		std::string text;
		for (uint32_t c : cps)
			AppendUtf8(text, c);
		return text;
	};
	static const char kAlphabet[] = "abcdefghijklmnopqrstuvwxyz'";

	auto edits = [&](const std::vector<uint32_t>& source,
			std::vector<std::vector<uint32_t> >& out) {
		size_t n = source.size();
		for (size_t i = 0; i < n; i++) {
			std::vector<uint32_t> e = source;
			e.erase(e.begin() + i);
			out.push_back(e);
		}
		for (size_t i = 0; i + 1 < n; i++) {
			std::vector<uint32_t> e = source;
			std::swap(e[i], e[i + 1]);
			out.push_back(e);
		}
		for (size_t i = 0; i < n; i++) {
			for (const char* a = kAlphabet; *a; a++) {
				if ((uint32_t)*a == source[i])
					continue;
				std::vector<uint32_t> e = source;
				e[i] = (uint32_t)*a;
				out.push_back(e);
			}
		}
		for (size_t i = 0; i <= n; i++) {
			for (const char* a = kAlphabet; *a; a++) {
				std::vector<uint32_t> e = source;
				e.insert(e.begin() + i, (uint32_t)*a);
				out.push_back(e);
			}
		}
	};

	std::map<std::string, int> scores;
	auto consider = [&](const std::string& candidate, int score) {
		if (candidate.empty())
			return;
		std::string found;
		if (fExact.count(candidate))
			found = candidate;
		else if (fLower.count(candidate))
			found = candidate;
		else {
			// a proper noun: find the capitalized entry
			std::string capital = candidate;
			int32_t consumed;
			uint32_t first = DecodeUtf8(capital.c_str(),
				(int32_t)capital.size(), &consumed);
			capital = Utf8(ToUpper(first)) + capital.substr(consumed);
			if (fExact.count(capital))
				found = capital;
		}
		if (found.empty())
			return;
		// prefer suggestions that keep the first letter
		if (!candidate.empty() && !letters.empty()
			&& (uint32_t)(unsigned char)candidate[0] != letters[0])
			score += 1;
		auto it = scores.find(found);
		if (it == scores.end() || it->second > score)
			scores[found] = score;
	};

	std::vector<std::vector<uint32_t> > first;
	edits(letters, first);
	for (const auto& e : first)
		consider(encode(e), 2);
	if (scores.size() < maximum && letters.size() < 16) {
		size_t budget = 400000;
		for (const auto& e : first) {
			std::vector<std::vector<uint32_t> > second;
			edits(e, second);
			for (const auto& e2 : second) {
				consider(encode(e2), 5);
				if (--budget == 0)
					break;
			}
			if (budget == 0)
				break;
		}
	}

	std::vector<std::pair<int, std::string> > ranked;
	for (const auto& entry : scores)
		ranked.push_back(std::make_pair(entry.second, entry.first));
	std::sort(ranked.begin(), ranked.end(),
		[&](const std::pair<int, std::string>& a,
			const std::pair<int, std::string>& b) {
			if (a.first != b.first)
				return a.first < b.first;
			// closer length first
			long da = std::labs((long)a.second.size() - (long)word.size());
			long db = std::labs((long)b.second.size() - (long)word.size());
			if (da != db)
				return da < db;
			return a.second < b.second;
		});

	bool upper = word.size() > 1 && IsAllUpper(word);
	bool capital = false;
	if (!word.empty()) {
		int32_t consumed;
		capital = IsUpper(DecodeUtf8(word.c_str(), (int32_t)word.size(),
			&consumed));
	}
	std::vector<std::string> result;
	for (const auto& entry : ranked) {
		std::string suggestion = entry.second;
		if (upper)
			suggestion = ToUpperString(suggestion);
		else if (capital) {
			int32_t consumed;
			uint32_t c = DecodeUtf8(suggestion.c_str(),
				(int32_t)suggestion.size(), &consumed);
			suggestion = Utf8(ToUpper(c)) + suggestion.substr(consumed);
		}
		if (std::find(result.begin(), result.end(), suggestion) == result.end())
			result.push_back(suggestion);
		if (result.size() >= maximum)
			break;
	}
	return result;
}


bool
SpellChecker::NextMisspelling(const Story& story, Pos from, Pos limit,
	Pos* start, Pos* end, std::string* word) const
{
	auto isWordPart = [](uint32_t c) {
		return IsLetter(c) || IsDigit(c) || c == '\'' || c == 0x2019;
	};
	for (int32_t p = from.para; p < (int32_t)story.size() && p <= limit.para;
			p++) {
		const std::string& text = story[p].text;
		int32_t length = (int32_t)text.size();
		int32_t offset = p == from.para ? from.offset : 0;
		int32_t stop = p == limit.para ? limit.offset : length;
		// do not start in the middle of a word
		if (offset > 0 && offset < length
			&& isWordPart(CharAt(text, PrevOffset(text, offset)))) {
			while (offset < length && isWordPart(CharAt(text, offset)))
				offset = NextOffset(text, offset);
		}
		while (offset < stop) {
			uint32_t c = CharAt(text, offset);
			if (!IsLetter(c) && !IsDigit(c)) {
				offset = NextOffset(text, offset);
				continue;
			}
			int32_t wordStart = offset;
			while (offset < length && isWordPart(CharAt(text, offset)))
				offset = NextOffset(text, offset);
			int32_t wordEnd = offset;
			// trailing apostrophes are punctuation
			while (wordEnd > wordStart) {
				uint32_t last = CharAt(text, PrevOffset(text, wordEnd));
				if (last != '\'' && last != 0x2019)
					break;
				wordEnd = PrevOffset(text, wordEnd);
			}
			std::string candidate = text.substr(wordStart,
				wordEnd - wordStart);
			if (!IsCorrect(candidate)) {
				*start = Pos(p, wordStart);
				*end = Pos(p, wordEnd);
				*word = candidate;
				return true;
			}
		}
	}
	return false;
}

}	// namespace quill

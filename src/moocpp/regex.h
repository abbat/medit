/*
 *   regex.h
 *
 *   Copyright (C) 2004-2016 by Yevgen Muntyan <emuntyan@users.sourceforge.net>
 *
 *   This file is part of medit.  medit is free software; you can
 *   redistribute it and/or modify it under the terms of the
 *   GNU Lesser General Public License as published by the
 *   Free Software Foundation; either version 2.1 of the License,
 *   or (at your option) any later version.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with medit.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "moocpp/gstr.h"
#include "mooutils/mooutils-cpp.h"

namespace g
{

class MatchInfo;

class Regex
{
public:
    enum CompileFlags
    {
        COMPILE_FLAGS_NONE = 0,
        CASELESS = G_REGEX_CASELESS,
        MULTILINE = G_REGEX_MULTILINE,
        DOTALL = G_REGEX_DOTALL,
        EXTENDED = G_REGEX_EXTENDED,
        ANCHORED = G_REGEX_ANCHORED,
        DOLLAR_ENDONLY = G_REGEX_DOLLAR_ENDONLY,
        UNGREEDY = G_REGEX_UNGREEDY,
        RAW = G_REGEX_RAW,
        NO_AUTO_CAPTURE = G_REGEX_NO_AUTO_CAPTURE,
        OPTIMIZE = G_REGEX_OPTIMIZE,
        FIRSTLINE = G_REGEX_FIRSTLINE,
        DUPNAMES = G_REGEX_DUPNAMES,
        NEWLINE_CR = G_REGEX_NEWLINE_CR,
        NEWLINE_LF = G_REGEX_NEWLINE_LF,
        NEWLINE_CRLF = G_REGEX_NEWLINE_CRLF,
        NEWLINE_ANYCRLF = G_REGEX_NEWLINE_ANYCRLF,
        BSR_ANYCRLF = G_REGEX_BSR_ANYCRLF,
        JAVASCRIPT_COMPAT = G_REGEX_JAVASCRIPT_COMPAT,
    };

    enum MatchFlags
    {
        MATCH_FLAGS_NONE = 0,
        MATCH_ANCHORED = G_REGEX_MATCH_ANCHORED,
        MATCH_NOTBOL = G_REGEX_MATCH_NOTBOL,
        MATCH_NOTEOL = G_REGEX_MATCH_NOTEOL,
        MATCH_NOTEMPTY = G_REGEX_MATCH_NOTEMPTY,
        MATCH_PARTIAL = G_REGEX_MATCH_PARTIAL,
        MATCH_NEWLINE_CR = G_REGEX_MATCH_NEWLINE_CR,
        MATCH_NEWLINE_LF = G_REGEX_MATCH_NEWLINE_LF,
        MATCH_NEWLINE_CRLF = G_REGEX_MATCH_NEWLINE_CRLF,
        MATCH_NEWLINE_ANY = G_REGEX_MATCH_NEWLINE_ANY,
        MATCH_NEWLINE_ANYCRLF = G_REGEX_MATCH_NEWLINE_ANYCRLF,
        MATCH_BSR_ANYCRLF  = G_REGEX_MATCH_BSR_ANYCRLF,
        MATCH_BSR_ANY = G_REGEX_MATCH_BSR_ANY,
        MATCH_PARTIAL_SOFT = G_REGEX_MATCH_PARTIAL_SOFT,
        MATCH_PARTIAL_HARD = G_REGEX_MATCH_PARTIAL_HARD,
        MATCH_NOTEMPTY_ATSTART = G_REGEX_MATCH_NOTEMPTY_ATSTART,
    };

    Regex(GRegex*);
    ~Regex();
    Regex(const Regex&) = delete;
    Regex& operator=(const Regex&) = delete;
    Regex(Regex&&);
    Regex& operator=(Regex&&);

    bool is_valid() const;
    operator bool() const;

    static Regex compile(const char* pattern, CompileFlags compile_options = COMPILE_FLAGS_NONE, MatchFlags match_options = MATCH_FLAGS_NONE, GError** error = nullptr);

    static bool match(const char *pattern, const char *string, CompileFlags compile_options = COMPILE_FLAGS_NONE, MatchFlags match_options = MATCH_FLAGS_NONE);

    MatchInfo match(const char* string, MatchFlags match_options = MATCH_FLAGS_NONE) const;
    MatchInfo match(const gstr& string, MatchFlags match_options = MATCH_FLAGS_NONE) const;
    MatchInfo match(const char* string, ssize_t string_len, int start_position, MatchFlags match_options, GError** error) const;
    static std::vector<gstr> split(const char* pattern, const char* string, CompileFlags compile_options = COMPILE_FLAGS_NONE, MatchFlags match_options = MATCH_FLAGS_NONE);
    std::vector<gstr> split(const char* string, MatchFlags match_options = MATCH_FLAGS_NONE) const;
    std::vector<gstr> split(const char* string, ssize_t string_len, int start_position, MatchFlags match_options, int max_tokens, GError** error) const;

    gstr replace(const char* string, const char* replacement, MatchFlags match_options = MATCH_FLAGS_NONE) const;
    gstr replace(const char* string, ssize_t string_len, int start_position, const char* replacement, MatchFlags match_options, GError** error) const;
    gstr replace_literal(const char* string, const char* replacement, MatchFlags match_options = MATCH_FLAGS_NONE) const;
    gstr replace_literal(const char* string, ssize_t string_len, int start_position, const char* replacement, MatchFlags match_options, GError** error) const;

private:
    GRegex* m_p;
};

class MatchInfo
{
public:
    MatchInfo(const Regex& regex);
    MatchInfo(const Regex& regex, GMatchInfo* p, bool take_ownership);
    ~MatchInfo();
    MatchInfo(const MatchInfo&) = delete;
    MatchInfo& operator=(const MatchInfo&) = delete;
    MatchInfo(MatchInfo&&);
    MatchInfo& operator=(MatchInfo&&);

    bool is_match() const;
    operator bool() const;

    const Regex& get_regex() const;
    const char* get_string() const;

    bool next(GError** error);
    bool matches() const;
    int get_match_count() const;
    bool is_partial_match() const;
    gstr expand_references(const char* string_to_expand, GError** error) const;
    gstr fetch(int match_num) const;
    bool fetch_pos(int match_num, int& start_pos, int& end_pos) const;
    gstr fetch_named(const char* name) const;
    bool fetch_named_pos(const char* name, int& start_pos, int& end_pos) const;
private:
    std::reference_wrapper<const Regex> m_regex;
    GMatchInfo* m_p;
    bool m_own;
};

} // namespace g

MOO_DEFINE_FLAGS(g::Regex::CompileFlags)
MOO_DEFINE_FLAGS(g::Regex::MatchFlags)

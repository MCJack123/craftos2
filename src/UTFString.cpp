/*
 * UTFString.cpp
 * CraftOS-PC 2
 * 
 * This file implements the UTFString type and library.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2021 JackMacWindows.
 */

#include <codecvt>
#include <locale>
#include <sstream>
#include <Poco/Unicode.h>
#include "UTFString.hpp"

// TODO: maybe optimize concatenation and substrings?

static std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> characterConverter;

std::u32string ansiToUnicode(const std::string& str) {
    std::u32string retval;
    for (unsigned char c : str) retval += c;
    return retval;
}

std::string unicodeToAnsi(const std::u32string& str) {
    std::string retval;
    for (char32_t c : str) {
        if (c > 255) retval += '?';
        else retval += (unsigned char)c;
    }
    return retval;
}

std::u32string UTF8ToUnicode(const std::string& str) {
    try {
        return characterConverter.from_bytes(str);
    } catch (std::range_error &e) {
        return ansiToUnicode(str);
    }
}

std::string unicodeToUTF8(const std::u32string& str) {
    try {
        return characterConverter.to_bytes(str);
    } catch (std::range_error &e) {
        return unicodeToAnsi(str);
    }
}

std::u32string& createUTFString(lua_State *L, const std::u32string &str) {
    std::u32string& retval = *(*(std::u32string**)lua_newuserdata(L, sizeof(std::u32string*)) = new std::u32string(str));
    lua_getfield(L, LUA_REGISTRYINDEX, "UTFString.mt");
    lua_setmetatable(L, -2);
    return retval;
}

bool isUTFString(lua_State *L, int idx) {
    if (!lua_isuserdata(L, idx) || !luaL_getmetafield(L, idx, "__name")) return false;
    bool ok = lua_isstring(L, -1) && strcmp(lua_tostring(L, -1), "UTFString") == 0;
    lua_pop(L, 1);
    return ok;
}

struct {
    lua_CFunction byte;
    lua_CFunction Char;
    lua_CFunction dump;
    lua_CFunction find;
    lua_CFunction format;
    lua_CFunction gmatch;
    lua_CFunction gsub;
    lua_CFunction len;
    lua_CFunction lower;
    lua_CFunction match;
    lua_CFunction pack;
    lua_CFunction packsize;
    lua_CFunction rep;
    lua_CFunction reverse;
    lua_CFunction sub;
    lua_CFunction tostring;
    lua_CFunction unpack;
    lua_CFunction upper;
} libstring;

// String library functions

// UTFString new(): Creates an empty string.
// UTFString new(str: string, isUTF8: boolean?): Creates a string from a binary string.
//   If isUTF8 is true, the input is treated as UTF-8; otherwise it's treated as 8-bit ANSI.
// UTFString new(str: UTFString): Copies the selected string.
static int UTFString_new(lua_State *L) {
    if (lua_isstring(L, 2)) // construct from string
        createUTFString(L, (lua_toboolean(L, 3) ? UTF8ToUnicode : ansiToUnicode)(tostring(L, 2)));
    else if (isUTFString(L, 2))
        createUTFString(L, toUTFString(L, 2));
    else if (lua_isnoneornil(L, 2))
        createUTFString(L);
    else return luaL_typerror(L, 2, "string or UTFString");
    return 1;
}

static int UTFString_byte(lua_State *L) {
    if (lua_isstring(L, 1)) return libstring.byte(L);
    if (!isUTFString(L, 1)) luaL_typerror(L, 1, "UTFString");
    std::u32string& str = toUTFString(L, 1);
    long start = luaL_optinteger(L, 2, 1);
    long end = luaL_optinteger(L, 3, 1);
    if (start < 0) start += str.size();
    if (end < 0) end += str.size();
    int size = end - start + 1;
    luaL_checkstack(L, size + 3, "could not allocate space for bytes");
    while (start <= end && start <= str.size()) {
        if (start < 0) start++;
        else lua_pushinteger(L, str[start++-1]);
    }
    return size;
}

static int UTFString_char(lua_State *L) {
    std::u32string& str = createUTFString(L);
    for (int i = 1; i < lua_gettop(L); i++) {
        int code = luaL_checkinteger(L, i);
        str += (char32_t)code;
    }
    return 1;
}

// don't know why you'd want this :P
static int UTFString_dump(lua_State *L) {
    libstring.dump(L);
    createUTFString(L, ansiToUnicode(tostring(L, -1)));
    return 1;
}

static int UTFString_isUTFString(lua_State *L) {
    lua_pushboolean(L, isUTFString(L, 1));
    return 1;
}

static int UTFString_len(lua_State *L) {
    if (lua_isstring(L, 1)) return libstring.len(L);
    if (!isUTFString(L, 1)) luaL_typerror(L, 1, "UTFString");
    std::u32string& str = toUTFString(L, 1);
    lua_pushinteger(L, str.size());
    return 1;
}

static int UTFString_lower(lua_State *L) {
    if (lua_isstring(L, 1)) return libstring.lower(L);
    if (!isUTFString(L, 1)) luaL_typerror(L, 1, "UTFString");
    std::u32string& str = toUTFString(L, 1);
    std::u32string& retval = createUTFString(L);
    retval.reserve(str.size());
    std::transform(str.begin(), str.end(), retval.begin(), [](char32_t c)->char32_t {return Poco::Unicode::toLower(c);});
    return 1;
}

static int UTFString_pack(lua_State *L) {
    std::string pattern;
    if (isUTFString(L, 1)) pattern = unicodeToAnsi(toUTFString(L, 1));
    else pattern = checkstring(L, 1);
    int p = 2;
    for (int i = 0; i < pattern.size() && p < lua_gettop(L); i++) {
        switch (pattern[i]) {
        case 'u': {
            pattern[i] = 'z';
            if (!isUTFString(L, p)) luaL_typerror(L, p, "UTFString");
            std::string utf8 = unicodeToAnsi(toUTFString(L, p));
            lua_pushlstring(L, utf8.c_str(), utf8.size());
            lua_replace(L, p++);
            break;
        } case 'U': {
            pattern[i] = 'z';
            if (!isUTFString(L, p)) luaL_typerror(L, p, "UTFString");
            std::string utf8 = unicodeToUTF8(toUTFString(L, p));
            lua_pushlstring(L, utf8.c_str(), utf8.size());
            lua_replace(L, p++);
            break;
        } case 'b': case 'B': case 'h': case 'H': case 'l': case 'L': case 'j': case 'J': case 'T':
          case 'i': case 'I': case 'f': case 'd': case 'c': case 'z': case 's': p++; break;
        }
    }
    lua_pushlstring(L, pattern.c_str(), pattern.size());
    lua_replace(L, 1);
    libstring.pack(L);
    createUTFString(L, ansiToUnicode(tostring(L, -1)));
    return 1;
}

static int UTFString_packsize(lua_State *L) {
    std::string pattern;
    if (isUTFString(L, 1)) pattern = unicodeToAnsi(toUTFString(L, 1));
    else pattern = checkstring(L, 1);
    for (char& c : pattern) if (c == 'u' || c == 'U') c = 'z';
    lua_pushlstring(L, pattern.c_str(), pattern.size());
    lua_replace(L, 1);
    return libstring.packsize(L);
}

static int UTFString_rep(lua_State *L) {
    if (lua_isstring(L, 1)) return libstring.rep(L);
    if (!isUTFString(L, 1)) luaL_typerror(L, 1, "UTFString");
    std::u32string& str = toUTFString(L, 1);
    int count = luaL_checkinteger(L, 2);
    std::u32string sep;
    if (lua_isstring(L, 3)) sep = ansiToUnicode(tostring(L, 3));
    else if (isUTFString(L, 3)) sep = toUTFString(L, 3);
    else if (!lua_isnoneornil(L, 3)) luaL_typerror(L, 3, "string or UTFString");
    if (count < 1) {
        createUTFString(L);
        return 1;
    }
    std::u32string& retval = createUTFString(L, str);
    while (--count > 0) retval += sep + str;
    return 1;
}

static int UTFString_reverse(lua_State *L) {
    if (lua_isstring(L, 1)) return libstring.reverse(L);
    if (!isUTFString(L, 1)) luaL_typerror(L, 1, "UTFString");
    std::u32string& str = toUTFString(L, 1);
    std::u32string& retval = createUTFString(L);
    retval.reserve(str.size());
    std::reverse_copy(str.begin(), str.end(), retval.begin());
    return 1;
}

static int UTFString_sub(lua_State *L) {
    if (lua_isstring(L, 1)) return libstring.sub(L);
    if (!isUTFString(L, 1)) luaL_typerror(L, 1, "UTFString");
    std::u32string& str = toUTFString(L, 1);
    long start = luaL_optinteger(L, 2, 1);
    long end = luaL_optinteger(L, 3, -1);
    if (start < 0) start += str.size();
    if (start == 0) start++;
    if (end < 0) end += str.size();
    if (end == 0) end++;
    if (end > str.size()) end = str.size();
    long size = end - start + 1;
    createUTFString(L, str.substr(start - 1, size));
    return 1;
}

static int UTFString_tostring(lua_State *L) {
    if (!isUTFString(L, 1)) {
        luaL_tostring(L, 1);
        return 1;
    }
    std::u32string& str = toUTFString(L, 1);
    std::string retval = unicodeToAnsi(str);
    lua_pushlstring(L, retval.c_str(), retval.size());
    return 1;
}

static int UTFString_unpack(lua_State *L) {
    std::string pattern, oldpattern;
    if (isUTFString(L, 1)) pattern = unicodeToAnsi(toUTFString(L, 1));
    else pattern = checkstring(L, 1);
    oldpattern = pattern;
    for (char& c : pattern) if (c == 'u' || c == 'U') c = 'z';
    lua_pushlstring(L, pattern.c_str(), pattern.size());
    lua_replace(L, 1);
    if (isUTFString(L, 2)) {
        std::string str = unicodeToAnsi(toUTFString(L, 2));
        lua_pushlstring(L, str.c_str(), str.size());
        lua_replace(L, 2);
    }
    int n = libstring.unpack(L);
    int p = lua_gettop(L) - n + 1;
    for (int i = 0; i < oldpattern.size() && p < lua_gettop(L); i++) {
        switch (oldpattern[i]) {
        case 'u': {
            createUTFString(L, ansiToUnicode(tostring(L, p)));
            lua_replace(L, p++);
            break;
        } case 'U': {
            createUTFString(L, UTF8ToUnicode(tostring(L, p)));
            lua_replace(L, p++);
            break;
        } case 'b': case 'B': case 'h': case 'H': case 'l': case 'L': case 'j': case 'J': case 'T':
          case 'i': case 'I': case 'f': case 'd': case 'c': case 'z': case 's': p++; break;
        }
    }
    return n;
}

static int UTFString_upper(lua_State *L) {
    if (lua_isstring(L, 1)) return libstring.upper(L);
    if (!isUTFString(L, 1)) luaL_typerror(L, 1, "UTFString");
    std::u32string& str = toUTFString(L, 1);
    std::u32string& retval = createUTFString(L);
    retval.reserve(str.size());
    std::transform(str.begin(), str.end(), retval.begin(), [](char32_t c)->char32_t {return Poco::Unicode::toUpper(c);});
    return 1;
}

static int UTFString_utf8(lua_State *L) {
    if (!isUTFString(L, 1)) luaL_typerror(L, 1, "UTFString");
    std::u32string& str = toUTFString(L, 1);
    std::string retval = unicodeToUTF8(str);
    lua_pushlstring(L, retval.c_str(), retval.size());
    return 1;
}

/*
** {======================================================
** PATTERN MATCHING
** =======================================================
*/

static ptrdiff_t posrelat (ptrdiff_t pos, size_t len) {
  /* relative string position: negative means back from end */
  if (pos < 0) pos += (ptrdiff_t)len + 1;
  return (pos >= 0) ? pos : 0;
}


#define CAP_UNFINISHED	(-1)
#define CAP_POSITION	(-2)
#define uchar(c) (c)

typedef struct MatchState {
  const char32_t *src_init;  /* init of source string */
  const char32_t *src_end;  /* end (`\0') of source string */
  lua_State *L;
  int level;  /* total number of captures (finished or unfinished) */
  struct {
    const char32_t *init;
    ptrdiff_t len;
  } capture[LUA_MAXCAPTURES];
} MatchState;


#define L_ESC		'%'
#define SPECIALS	U"^$*+?.([%-"


static int check_capture (MatchState *ms, int l) {
  l -= '1';
  if (l < 0 || l >= ms->level || ms->capture[l].len == CAP_UNFINISHED)
    return luaL_error(ms->L, "invalid capture index");
  return l;
}


static int capture_to_close (MatchState *ms) {
  int level = ms->level;
  for (level--; level>=0; level--)
    if (ms->capture[level].len == CAP_UNFINISHED) return level;
  return luaL_error(ms->L, "invalid pattern capture");
}


static const char32_t *classend (MatchState *ms, const char32_t *p, const char32_t *pend) {
  switch (*p++) {
    case L_ESC: {
      if (p >= pend)
        luaL_error(ms->L, "malformed pattern (ends with " LUA_QL("%%") ")");
      return p+1;
    }
    case '[': {
      if (*p == '^') p++;
      do {  /* look for a `]' */
        if (p >= pend)
          luaL_error(ms->L, "malformed pattern (missing " LUA_QL("]") ")");
        if (*(p++) == L_ESC && p < pend)
          p++;  /* skip escapes (e.g. `%]') */
      } while (*p != ']');
      return p+1;
    }
    default: {
      return p;
    }
  }
}


static int match_class (int c, int cl) {
  int res;
  switch (tolower(cl)) {
    case 'a' : res = isalpha(c); break;
    case 'c' : res = iscntrl(c); break;
    case 'd' : res = isdigit(c); break;
    case 'l' : res = islower(c); break;
    case 'p' : res = ispunct(c); break;
    case 's' : res = isspace(c); break;
    case 'u' : res = isupper(c); break;
    case 'w' : res = isalnum(c); break;
    case 'x' : res = isxdigit(c); break;
    case 'z' : res = (c == 0); break;
    default: return (cl == c);
  }
  return (islower(cl) ? res : !res);
}


static int matchbracketclass (int c, const char32_t *p, const char32_t *ec) {
  int sig = 1;
  if (*(p+1) == '^') {
    sig = 0;
    p++;  /* skip the `^' */
  }
  while (++p < ec) {
    if (*p == L_ESC) {
      p++;
      if (match_class(c, uchar(*p)))
        return sig;
    }
    else if ((*(p+1) == '-') && (p+2 < ec)) {
      p+=2;
      if (uchar(*(p-2)) <= c && c <= uchar(*p))
        return sig;
    }
    else if (uchar(*p) == c) return sig;
  }
  return !sig;
}


static int singlematch (int c, const char32_t *p, const char32_t *ep) {
  switch (*p) {
    case '.': return 1;  /* matches any char */
    case L_ESC: return match_class(c, uchar(*(p+1)));
    case '[': return matchbracketclass(c, p, ep-1);
    default:  return (uchar(*p) == c);
  }
}


static const char32_t *match (MatchState *ms, const char32_t *s, const char32_t *p, const char32_t *pend);


static const char32_t *matchbalance (MatchState *ms, const char32_t *s,
                                   const char32_t *p, const char32_t *pend) {
  if (p + 1 >= pend)
    luaL_error(ms->L, "unbalanced pattern");
  if (*s != *p) return NULL;
  else {
    int b = *p;
    int e = *(p+1);
    int cont = 1;
    while (++s < ms->src_end) {
      if (*s == e) {
        if (--cont == 0) return s+1;
      }
      else if (*s == b) cont++;
    }
  }
  return NULL;  /* string ends out of balance */
}


static const char32_t *max_expand (MatchState *ms, const char32_t *s,
                                 const char32_t *p, const char32_t *ep, const char32_t *pend) {
  ptrdiff_t i = 0;  /* counts maximum expand for item */
  while ((s+i)<ms->src_end && singlematch(uchar(*(s+i)), p, ep))
    i++;
  /* keeps trying to match with the maximum repetitions */
  while (i>=0) {
    const char32_t *res = match(ms, (s+i), ep+1, pend);
    if (res) return res;
    i--;  /* else didn't match; reduce 1 repetition to try again */
  }
  return NULL;
}


static const char32_t *min_expand (MatchState *ms, const char32_t *s,
                                 const char32_t *p, const char32_t *ep, const char32_t *pend) {
  for (;;) {
    const char32_t *res = match(ms, s, ep+1, pend);
    if (res != NULL)
      return res;
    else if (s<ms->src_end && singlematch(uchar(*s), p, ep))
      s++;  /* try with one more repetition */
    else return NULL;
  }
}


static const char32_t *start_capture (MatchState *ms, const char32_t *s,
                                    const char32_t *p, const char32_t *pend, int what) {
  const char32_t *res;
  int level = ms->level;
  if (level >= LUA_MAXCAPTURES) { luaL_error(ms->L, "too many captures"); return NULL; }
  ms->capture[level].init = s;
  ms->capture[level].len = what;
  ms->level = level+1;
  if ((res=match(ms, s, p, pend)) == NULL)  /* match failed? */
    ms->level--;  /* undo capture */
  return res;
}


static const char32_t *end_capture (MatchState *ms, const char32_t *s,
                                  const char32_t *p, const char32_t *pend) {
  int l = capture_to_close(ms);
  const char32_t *res;
  ms->capture[l].len = s - ms->capture[l].init;  /* close capture */
  if ((res = match(ms, s, p, pend)) == NULL)  /* match failed? */
    ms->capture[l].len = CAP_UNFINISHED;  /* undo capture */
  return res;
}


static const char32_t *match_capture (MatchState *ms, const char32_t *s, int l) {
  size_t len;
  l = check_capture(ms, l);
  len = ms->capture[l].len;
  if ((size_t)(ms->src_end-s) >= len &&
      memcmp(ms->capture[l].init, s, len) == 0)
    return s+len;
  else return NULL;
}


static const char32_t *match (MatchState *ms, const char32_t *s, const char32_t *p, const char32_t *pend) {
  init: /* using goto's to optimize tail recursion */
  if (p >= pend) return s;
  switch (*p) {
    case '(': {  /* start capture */
      if (*(p+1) == ')')  /* position capture? */
        return start_capture(ms, s, p+2, pend, CAP_POSITION);
      else
        return start_capture(ms, s, p+1, pend, CAP_UNFINISHED);
    }
    case ')': {  /* end capture */
      return end_capture(ms, s, p+1, pend);
    }
    case L_ESC: {
      switch (*(p+1)) {
        case 'b': {  /* balanced string? */
          s = matchbalance(ms, s, p+2, pend);
          if (s == NULL) return NULL;
          p+=4; goto init;  /* else return match(ms, s, p+4); */
        }
        case 'f': {  /* frontier? */
          const char32_t *ep; char previous;
          p += 2;
          if (*p != '[')
            luaL_error(ms->L, "missing " LUA_QL("[") " after "
                               LUA_QL("%%f") " in pattern");
          ep = classend(ms, p, pend);  /* points to what is next */
          previous = (s == ms->src_init) ? '\0' : *(s-1);
          if (matchbracketclass(uchar(previous), p, ep-1) ||
             !matchbracketclass(uchar(*s), p, ep-1)) return NULL;
          p=ep; goto init;  /* else return match(ms, s, ep); */
        }
        default: {
          if (isdigit(uchar(*(p+1)))) {  /* capture results (%0-%9)? */
            s = match_capture(ms, s, uchar(*(p+1)));
            if (s == NULL) return NULL;
            p+=2; goto init;  /* else return match(ms, s, p+2) */
          }
          goto dflt;  /* case default */
        }
      }
    }
    case '$': {
      if (p+1 >= pend)  /* is the `$' the last char in pattern? */
        return (s == ms->src_end) ? s : NULL;  /* check end of string */
      else goto dflt;
    }
    default: dflt: {  /* it is a pattern item */
      const char32_t *ep = classend(ms, p, pend);  /* points to what is next */
      int m = s<ms->src_end && singlematch(uchar(*s), p, ep);
      switch (*ep) {
        case '?': {  /* optional */
          const char32_t *res;
          if (m && ((res=match(ms, s+1, ep+1, pend)) != NULL))
            return res;
          p=ep+1; goto init;  /* else return match(ms, s, ep+1); */
        }
        case '*': {  /* 0 or more repetitions */
          return max_expand(ms, s, p, ep, pend);
        }
        case '+': {  /* 1 or more repetitions */
          return (m ? max_expand(ms, s+1, p, ep, pend) : NULL);
        }
        case '-': {  /* 0 or more repetitions (minimum) */
          return min_expand(ms, s, p, ep, pend);
        }
        default: {
          if (!m) return NULL;
          s++; p=ep; goto init;  /* else return match(ms, s+1, ep); */
        }
      }
    }
  }
}



static const char32_t *lmemfind (const char32_t *s1, size_t l1,
                               const char32_t *s2, size_t l2) {
  if (l2 == 0) return s1;  /* empty strings are everywhere */
  else if (l2 > l1) return NULL;  /* avoids a negative `l1' */
  else {
    const char32_t *init;  /* to search for a `*s2' inside `s1' */
    l2--;  /* 1st char will be checked by `memchr' */
    l1 = l1-l2;  /* `s2' cannot be found after that */
    while (l1 > 0 && (init = (const char32_t *)memchr(s1, *s2, l1)) != NULL) {
      init++;   /* 1st char is already checked */
      if (memcmp(init, s2+1, l2) == 0)
        return init-1;
      else {  /* correct `l1' and `s1' to try again */
        l1 -= init-s1;
        s1 = init;
      }
    }
    return NULL;  /* not found */
  }
}


static void push_onecapture (MatchState *ms, int i, const char32_t *s,
                                                    const char32_t *e) {
  if (i >= ms->level) {
    if (i == 0)  /* ms->level == 0, too */
      createUTFString(ms->L, std::u32string(s, e - s));  /* add whole match */
    else
      luaL_error(ms->L, "invalid capture index");
  }
  else {
    ptrdiff_t l = ms->capture[i].len;
    if (l == CAP_UNFINISHED) luaL_error(ms->L, "unfinished capture");
    if (l == CAP_POSITION)
      lua_pushinteger(ms->L, ms->capture[i].init - ms->src_init + 1);
    else
      createUTFString(ms->L, std::u32string(ms->capture[i].init, l));
  }
}


static int push_captures (MatchState *ms, const char32_t *s, const char32_t *e) {
  int i;
  int nlevels = (ms->level == 0 && s) ? 1 : ms->level;
  luaL_checkstack(ms->L, nlevels, "too many captures");
  for (i = 0; i < nlevels; i++)
    push_onecapture(ms, i, s, e);
  return nlevels;  /* number of strings pushed */
}


static int str_find_aux (lua_State *L, int find) {
  std::u32string us, up;
  if (isUTFString(L, 1)) us = toUTFString(L, 1);
  else us = ansiToUnicode(checkstring(L, 1));
  if (isUTFString(L, 2)) up = toUTFString(L, 2);
  else up = ansiToUnicode(checkstring(L, 2));
  size_t l1 = us.size(), l2 = up.size();
  const char32_t *s = us.c_str();
  const char32_t *p = up.c_str();
  ptrdiff_t init = posrelat(luaL_optinteger(L, 3, 1), l1) - 1;
  if (init < 0) init = 0;
  else if ((size_t)(init) > l1) init = (ptrdiff_t)l1;
  if (find && (lua_toboolean(L, 4) ||  /* explicit request? */
      up.find_first_of(SPECIALS) != std::u32string::npos)) {  /* or no special characters? */
    /* do a plain search */
    const char32_t *s2 = lmemfind(s+init, l1-init, p, l2);
    if (s2) {
      lua_pushinteger(L, s2-s+1);
      lua_pushinteger(L, s2-s+l2);
      return 2;
    }
  }
  else {
    MatchState ms;
    int anchor = (*p == '^') ? (p++, l2--, 1) : 0;
    const char32_t *s1=s+init;
    ms.L = L;
    ms.src_init = s;
    ms.src_end = s+l1;
    do {
      const char32_t *res;
      ms.level = 0;
      if ((res=match(&ms, s1, p, p+l2)) != NULL) {
        if (find) {
          lua_pushinteger(L, s1-s+1);  /* start */
          lua_pushinteger(L, res-s);   /* end */
          return push_captures(&ms, NULL, 0) + 2;
        }
        else
          return push_captures(&ms, s1, res);
      }
    } while (s1++ < ms.src_end && !anchor);
  }
  lua_pushnil(L);  /* not found */
  return 1;
}


static int UTFString_find (lua_State *L) {
  return str_find_aux(L, 1);
}


static int UTFString_match (lua_State *L) {
  return str_find_aux(L, 0);
}


static int gmatch_aux (lua_State *L) {
  MatchState ms;
  std::u32string us, up;
  if (isUTFString(L, lua_upvalueindex(1))) us = toUTFString(L, lua_upvalueindex(1));
  else us = ansiToUnicode(checkstring(L, lua_upvalueindex(1)));
  if (isUTFString(L, lua_upvalueindex(2))) up = toUTFString(L, lua_upvalueindex(2));
  else up = ansiToUnicode(checkstring(L, lua_upvalueindex(2)));
  size_t ls = us.size(), lp = up.size();
  const char32_t *s = us.c_str();
  const char32_t *p = up.c_str();
  const char32_t *src;
  ms.L = L;
  ms.src_init = s;
  ms.src_end = s+ls;
  for (src = s + (size_t)lua_tointeger(L, lua_upvalueindex(3));
       src <= ms.src_end;
       src++) {
    const char32_t *e;
    ms.level = 0;
    if ((e = match(&ms, src, p, p+lp)) != NULL) {
      lua_Integer newstart = e-s;
      if (e == src) newstart++;  /* empty match? go at least one position */
      lua_pushinteger(L, newstart);
      lua_replace(L, lua_upvalueindex(3));
      return push_captures(&ms, src, e);
    }
  }
  return 0;  /* not found */
}


static int UTFString_gmatch (lua_State *L) {
  luaL_checkstring(L, 1);
  luaL_checkstring(L, 2);
  lua_settop(L, 2);
  lua_pushinteger(L, 0);
  lua_pushcclosure(L, gmatch_aux, 3);
  return 1;
}


static int gfind_nodef (lua_State *L) {
  return luaL_error(L, LUA_QL("string.gfind") " was renamed to "
                       LUA_QL("string.gmatch"));
}


static void add_s (MatchState *ms, std::basic_stringstream<char32_t> *b,
                                   const char32_t *s, const char32_t *e) {
  std::u32string unews;
  if (isUTFString(ms->L, 3)) unews = toUTFString(ms->L, 3);
  else unews = ansiToUnicode(checkstring(ms->L, 3));
  size_t l = unews.size(), i;
  const char32_t *news = unews.c_str();
  for (i = 0; i < l; i++) {
    if (news[i] != L_ESC)
      *b << news[i];
    else {
      i++;  /* skip ESC */
      if (!isdigit(uchar(news[i])))
        *b << news[i];
      else if (news[i] == '0')
          *b << std::u32string(s, e - s);
      else {
        push_onecapture(ms, news[i] - '1', s, e);
        /* add capture to accumulated result */
        if (isUTFString(ms->L, -1)) *b << toUTFString(ms->L, -1);
        else if (lua_isnumber(ms->L, -1)) *b << lua_tonumber(ms->L, -1);
        else *b << ansiToUnicode(tostring(ms->L, -1));
        lua_pop(ms->L, 1);
      }
    }
  }
}


struct string_gsub_state {
  MatchState ms;
  std::basic_stringstream<char32_t> * b;
  int n;
  const char32_t * e;
  int max_s;
  int res;
};

static void add_value (struct string_gsub_state * st, const char32_t *s) {
  lua_State *L = st->ms.L;
  if (st->res) {
    st->res = 0;
    goto resume;
  }
  switch (lua_type(L, 3)) {
    case LUA_TNUMBER:
    case LUA_TSTRING: {
      add_s(&st->ms, st->b, s, st->e);
      return;
    }
    case LUA_TFUNCTION: {
      int n;
      lua_pushvalue(L, 3);
      n = push_captures(&st->ms, s, st->e);
      st->res = 1;
      lua_vcall(L, n, 1, st);
      st->res = 0;
      break;
    }
    case LUA_TTABLE: {
      push_onecapture(&st->ms, 0, s, st->e);
      lua_gettable(L, 3);
      break;
    }
  }
resume:
  if (!lua_toboolean(L, -1)) {  /* nil or false? */
    lua_pop(L, 1);
    createUTFString(L, std::u32string(s, st->e - s));  /* keep original text */
  }
  else if (!isUTFString(L, -1) && !lua_isstring(L, -1))
    luaL_error(L, "invalid replacement value (a %s)", luaL_typename(L, -1)); 
  /* add result to accumulator */
  if (isUTFString(L, -1)) *st->b << toUTFString(L, -1);
  else *st->b << ansiToUnicode(tostring(L, -1));
  lua_pop(L, 1);
}


static int UTFString_gsub (lua_State *L) {
  std::u32string usrc, up;
  if (isUTFString(L, 1)) usrc = toUTFString(L, 1);
  else usrc = ansiToUnicode(checkstring(L, 1));
  if (isUTFString(L, 2)) up = toUTFString(L, 2);
  else up = ansiToUnicode(checkstring(L, 2));
  size_t srcl = usrc.size(), pl = up.size();
  const char32_t *src = usrc.c_str();
  const char32_t *p = up.c_str();
  int tr = lua_type(L, 3);
  int anchor = (*p == '^') ? (p++, pl--, 1) : 0;
  void * ud = NULL;
  struct string_gsub_state * s;
  lua_Alloc alloc = lua_getallocf(L, &ud);
  if (lua_vcontext(L)) {
    s = (struct string_gsub_state*)lua_vcontext(L);
    goto resume;
  }
  s = (struct string_gsub_state*)alloc(ud, NULL, 0, sizeof(struct string_gsub_state));
  s->n = 0;
  s->res = 0;
  s->max_s = luaL_optint(L, 4, srcl + 1);
  luaL_argcheck(L, tr == LUA_TNUMBER || tr == LUA_TSTRING ||
                   tr == LUA_TFUNCTION || tr == LUA_TTABLE, 3,
                      "string/function/table expected");
  s->b = new std::basic_stringstream<char32_t>;
  s->ms.L = L;
  s->ms.src_init = src;
  s->ms.src_end = src+srcl;
  while (s->n < s->max_s) {
    s->ms.level = 0;
    s->e = match(&s->ms, src, p, p+pl);
    if (s->e) {
      s->n++;
resume:
      add_value(s, src);
    }
    if (s->e && s->e>src) /* non empty match? */
      src = s->e;  /* skip it */
    else if (src < s->ms.src_end)
      *s->b << *src++;
    else break;
    if (anchor) break;
  }
  *s->b << std::u32string(src, s->ms.src_end-src);
  createUTFString(L, s->b->str());
  lua_pushinteger(L, s->n);  /* number of substitutions */
  delete s->b;
  alloc(ud, s, sizeof(struct string_gsub_state), 0);
  return 2;
}

/* }====================================================== */


/* maximum size of each formatted item (> len(format('%99.99f', -1e308))) */
#define MAX_ITEM	512
/* valid flags in a format specification */
#define FLAGS	"-+ #0"
/*
** maximum size of each format specification (such as '%-099.99d')
** (+10 accounts for %99.99x plus margin of error)
*/
#define MAX_FORMAT	(sizeof(FLAGS) + sizeof(LUA_INTFRMLEN) + 10)


static void addquoted (lua_State *L, std::basic_stringstream<char32_t>& b, int arg) {
  if (lua_isnil(L, arg)) {
    b << U"nil";
    return;
  }
  std::u32string str;
  if (isUTFString(L, arg)) str = toUTFString(L, arg);
  else str = ansiToUnicode(checkstring(L, arg));
  b << U'"';
  for (int i = 0; i < str.size(); i++) {
    switch (str[i]) {
      case '"': case '\\': case '\n': {
        b << U'\\' << str[i];
        break;
      }
      case '\r': {
        b << U"\\r";
        break;
      }
      case '\0': {
        b << U"\\000";
        break;
      }
      default: {
        b << str[i];
        break;
      }
    }
  }
  b << U'"';
}

static size_t scanformat (lua_State *L, const std::u32string& strfrmt, size_t i, char* form) {
  size_t oldi = i;
  while (strfrmt[i] != '\0' && strchr(FLAGS, strfrmt[i]) != NULL) i++;  /* skip flags */
  if (i - oldi >= sizeof(FLAGS))
    luaL_error(L, "invalid format (repeated flags)");
  if (Poco::Unicode::isDigit(strfrmt[i])) i++;  /* skip width */
  if (Poco::Unicode::isDigit(strfrmt[i])) i++;  /* (2 digits at most) */
  if (strfrmt[i] == '.') {
    i++;
    if (Poco::Unicode::isDigit(strfrmt[i])) i++;  /* skip precision */
    if (Poco::Unicode::isDigit(strfrmt[i])) i++;  /* (2 digits at most) */
  }
  if (Poco::Unicode::isDigit(strfrmt[i]))
    luaL_error(L, "invalid format (width or precision too long)");
  *(form++) = '%';
  while (oldi <= i) *(form++) = strfrmt[oldi++];
  *form = '\0';
  return i;
}


static void addintlen (char *form) {
  size_t l = strlen(form);
  char spec = form[l - 1];
  strcpy(form + l - 1, LUA_INTFRMLEN);
  form[l + sizeof(LUA_INTFRMLEN) - 2] = spec;
  form[l + sizeof(LUA_INTFRMLEN) - 1] = '\0';
}


static int UTFString_format (lua_State *L) {
  int top = lua_gettop(L);
  int arg = 1;
  size_t sfl;
  std::u32string strfrmt;
  if (isUTFString(L, arg)) strfrmt = toUTFString(L, arg);
  else strfrmt = ansiToUnicode(checkstring(L, arg));
  std::basic_stringstream<char32_t> ss;
  size_t i = 0;
  while (i < strfrmt.size()) {
    if (strfrmt[i] != L_ESC)
      ss << strfrmt[i++];
    else if (strfrmt[++i] == L_ESC)
      ss << strfrmt[i++];  /* %% */
    else { /* format item */
      char form[MAX_FORMAT];  /* to store the format (`%...') */
      char buff[MAX_ITEM];  /* to store the formatted item */
      if (++arg > top)
        luaL_argerror(L, arg, "no value");
      i = scanformat(L, strfrmt, i, form);
      switch (strfrmt[i++]) {
        case 'c': {
          sprintf(buff, form, (int)luaL_checknumber(L, arg));
          break;
        }
        case 'd':  case 'i': {
          addintlen(form);
          sprintf(buff, form, (LUA_INTFRM_T)luaL_checknumber(L, arg));
          break;
        }
        case 'o':  case 'u':  case 'x':  case 'X': {
          addintlen(form);
          sprintf(buff, form, (unsigned LUA_INTFRM_T)luaL_checknumber(L, arg));
          break;
        }
        case 'e':  case 'E': case 'f':
        case 'g': case 'G': {
          sprintf(buff, form, (double)luaL_checknumber(L, arg));
          break;
        }
        case 'q': {
          addquoted(L, ss, arg);
          continue;  /* skip the 'addsize' at the end */
        }
        case 's': {
          // We format the string manually to allow for proper UTF insertion
          std::u32string str;
          if (isUTFString(L, arg)) str = toUTFString(L, arg);
          else if (lua_isnil(L, arg)) str = U"nil";
          else str = ansiToUnicode(checkstring(L, arg));
          bool ljust = false;
          int width = 0;
          int precision = str.size();
          int stage = 0;
          for (int j = 1; stage < 3 && form[j]; j++) {
            switch (stage) {
            case 0:
              switch (form[j]) {
              case '-': ljust = true;
              case '+': case ' ': case '#': case '0': break;
              }
              stage = 1;
            case 1:
              if (isdigit(form[j])) {width = width * 10 + (form[j] - '0'); break;}
              else if (form[j] != '.') {stage = 3; break;}
              stage = 2;
              precision = 0;
            case 2:
              if (isdigit(form[j])) {precision = precision * 10 + (form[j] - '0'); break;}
              stage = 3;
            }
          }
          if (str.size() > precision) str = str.substr(0, precision);
          if (width > str.size()) {
            if (!ljust) ss << std::u32string(width - str.size(), ' ');
            ss << str;
            if (ljust) ss << std::u32string(width - str.size(), ' ');
          } else ss << str;
          continue;
        }
        default: {  /* also treat cases `pnLlh' */
          return luaL_error(L, "invalid option " LUA_QL("%%%c") " to "
                               LUA_QL("format"), strfrmt[i-1]);
        }
      }
      ss << ansiToUnicode(buff);
    }
  }
  createUTFString(L, ss.str());
  return 1;
}

// Metamethods

static int UTFString__concat(lua_State *L) {
    std::u32string* str;
    int idx;
    if (isUTFString(L, 1)) {
        idx = 2;
        str = *(std::u32string**)lua_touserdata(L, 1);
    } else if (isUTFString(L, 2)) {
        idx = 1;
        str = *(std::u32string**)lua_touserdata(L, 2);
    } else luaL_error(L, "bad arguments (no UTFString)");
    if (isUTFString(L, idx)) { // idx must be 2 because the second path wasn't executed
        std::u32string& retval = createUTFString(L, *str);
        retval += toUTFString(L, idx);
        return 1;
    }
    std::u32string other = ansiToUnicode(tostring(L, idx));
    if (idx == 1) createUTFString(L, other + *str);
    else createUTFString(L, *str + other);
    return 1;
}

static int UTFString__eq(lua_State *L) {
    if (!isUTFString(L, 1) || !isUTFString(L, 2)) luaL_typerror(L, 1, "UTFString");
    std::u32string& a = toUTFString(L, 1);
    std::u32string& b = toUTFString(L, 1);
    lua_pushboolean(L, a == b);
    return 1;
}

static int UTFString__lt(lua_State *L) {
    if (!isUTFString(L, 1) || !isUTFString(L, 2)) luaL_typerror(L, 1, "UTFString");
    std::u32string& a = toUTFString(L, 1);
    std::u32string& b = toUTFString(L, 1);
    lua_pushboolean(L, a < b);
    return 1;
}

static int UTFString__le(lua_State *L) {
    if (!isUTFString(L, 1) || !isUTFString(L, 2)) luaL_typerror(L, 1, "UTFString");
    std::u32string& a = toUTFString(L, 1);
    std::u32string& b = toUTFString(L, 1);
    lua_pushboolean(L, a <= b);
    return 1;
}

static int UTFString__streq(lua_State *L) {
    std::u32string* str;
    int idx;
    if (isUTFString(L, 1)) {
        idx = 2;
        str = *(std::u32string**)lua_touserdata(L, 1);
    } else if (isUTFString(L, 2)) {
        idx = 1;
        str = *(std::u32string**)lua_touserdata(L, 2);
    } else luaL_error(L, "bad arguments (no UTFString)");
    if (isUTFString(L, idx)) { // idx must be 2 because the second path wasn't executed
        std::u32string& retval = createUTFString(L, *str);
        retval += toUTFString(L, idx);
        return 1;
    }
    std::u32string other = ansiToUnicode(tostring(L, idx));
    lua_pushboolean(L, *str == other);
    return 1;
}

static int UTFString__gc(lua_State *L) {
    delete *(std::u32string**)lua_touserdata(L, 1);
    return 0;
}

// Register

static luaL_Reg UTFString[] = {
    {"new", UTFString_new},
    {"byte", UTFString_byte},
    {"char", UTFString_char},
    {"dump", UTFString_dump},
    {"find", UTFString_find},
    {"format", UTFString_format},
    {"gmatch", UTFString_gmatch},
    {"gsub", UTFString_gsub},
    {"isUTFString", UTFString_isUTFString},
    {"len", UTFString_len},
    {"lower", UTFString_lower},
    {"match", UTFString_match},
    {"pack", UTFString_pack},
    {"packsize", UTFString_packsize},
    {"rep", UTFString_rep},
    {"reverse", UTFString_reverse},
    {"sub", UTFString_sub},
    {"tostring", UTFString_tostring},
    {"unpack", UTFString_unpack},
    {"upper", UTFString_upper},
    {"utf8", UTFString_utf8},
    {NULL, NULL}
};

#define getstrfunc(name) lua_getfield(L, -1, #name); libstring.name = lua_tocfunction(L, -1); lua_pop(L, 1)

int luaopen_UTFString(lua_State *L) {
    luaL_register(L, "UTFString", UTFString);
    lua_createtable(L, 0, 1);
    lua_pushcfunction(L, UTFString_new);
    lua_setfield(L, -2, "__call");
    lua_setmetatable(L, -2);

    lua_createtable(L, 0, 10);
    lua_pushcfunction(L, UTFString_len);
    lua_setfield(L, -2, "__len");
    lua_pushcfunction(L, UTFString__concat);
    lua_setfield(L, -2, "__concat");
    lua_pushcfunction(L, UTFString__eq);
    lua_setfield(L, -2, "__eq");
    lua_pushcfunction(L, UTFString__lt);
    lua_setfield(L, -2, "__lt");
    lua_pushcfunction(L, UTFString__le);
    lua_setfield(L, -2, "__le");
    lua_pushcfunction(L, UTFString_tostring);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, UTFString__streq);
    lua_setfield(L, -2, "__streq");
    lua_pushcfunction(L, UTFString__gc);
    lua_setfield(L, -2, "__gc");
    lua_pushvalue(L, -2);
    lua_setfield(L, -2, "__index");
    lua_pushliteral(L, "UTFString");
    lua_setfield(L, -2, "__name");
    lua_setfield(L, LUA_REGISTRYINDEX, "UTFString.mt");

    lua_getglobal(L, "string");
    getstrfunc(byte);
    lua_getfield(L, -1, "char"); libstring.Char = lua_tocfunction(L, -1); lua_pop(L, 1);
    getstrfunc(dump);
    getstrfunc(find);
    getstrfunc(format);
    getstrfunc(gmatch);
    getstrfunc(gsub);
    getstrfunc(len);
    getstrfunc(lower);
    getstrfunc(match);
    getstrfunc(pack);
    getstrfunc(packsize);
    getstrfunc(rep);
    getstrfunc(reverse);
    getstrfunc(sub);
    getstrfunc(tostring);
    getstrfunc(unpack);
    getstrfunc(upper);
    lua_pop(L, 1);

    return 1;
}
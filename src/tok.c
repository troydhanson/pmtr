#include <string.h>
#include "cfg.h"

static struct {
  char *str;
  size_t len;
  int id;
} kw[] = 
{
 {"job",     3, TOK_JOB},
 {"name",    4, TOK_NAME},
 {"user",    4, TOK_USER},
 {"cmd",     3, TOK_CMD},
 {"env",     3, TOK_ENV},
 {"dir",     3, TOK_DIR},
 {"out",     3, TOK_OUT},
 {"err",     3, TOK_ERR},
 {"in",      2, TOK_IN},
 {"order",   5, TOK_ORDER},
 {"disable", 7, TOK_DISABLED},
 {"wait",    4, TOK_WAIT},
 {"once",    4, TOK_ONCE},
 {"{",       1, TOK_LCURLY},
 {"}",       1, TOK_RCURLY},
 {"listen",  6, TOK_LISTEN},
 {"on",      2, TOK_ON},
 {"report",  6, TOK_REPORT},
 {"to",      2, TOK_TO},
 {"bounce",  6, TOK_BOUNCE},
 {"every",   5, TOK_EVERY},
 {"depends", 7, TOK_DEPENDS},
 {"ulimit",  6, TOK_ULIMIT},
 {"nice",    4, TOK_NICE},
 {"cpu",     3, TOK_CPUSET},
};
static const int ws[256] = { ['\r']=1, ['\n']=1, ['\t']=1, [' ']=1 };

int get_tok(char *c_orig, char **c, size_t *bsz, size_t *toksz, int *line) {
  char *e;
  int leading_nl=0;

 again:
  /* skip leading whitespace */
  while(*bsz && ws[(int)**c]) {
    if (**c=='\n') { 
      leading_nl=1;
      (*line)++;
    }
    (*bsz)--; (*c)++;
  }
  if (*bsz == 0) return 0; // end of input

  /* disregard comments til end of line */
  if (**c=='#') {
    while (*bsz) {
      (*c)++; (*bsz)--;
      if (**c == '\n') goto again;
    }
    return 0; /* eob while looking for trailing newline */
  }

  /* identify literal keywords */
  int i;
  for(i=0; i < sizeof(kw)/sizeof(*kw); i++) {
    if (*bsz < kw[i].len) continue;
    if (memcmp(*c,kw[i].str,kw[i].len) ) continue;
    /* keywords except "{ on to every" must be preceded by start-of-buf or newline */
    if (kw[i].id != TOK_LCURLY && 
        kw[i].id != TOK_ON     && 
        kw[i].id != TOK_TO     && 
        kw[i].id != TOK_EVERY) {
      for(e=(*c)-1; e > c_orig; e--) {
        if (*e == '\n') break;
        if (ws[(int)*e]) continue;
        /* has non-newline chars preceding, so do not consider as a keyword */
        else goto quoted; 
      }
    }
    /* require keywords to end with space or eob */
    e = (*bsz > kw[i].len) ? (*c + kw[i].len) : " ";
    if (!ws[(int)(*e)]) continue;
    *toksz = kw[i].len;
    return kw[i].id;
  }

 quoted:
  /* identify quoted string that ends with the closing quote on same line */
  if (**c=='"') {
    *toksz=1;
    while (*toksz < *bsz) {
      e = *c + *toksz;
      (*toksz)++;
      if (*e == '"') return TOK_QUOTEDSTR;
      if (*e == '\n') return -1;
    }
    return -1; /* eob without terminating quote */
  }

  /* otherwise its a string ending with end-of-buffer or whitespace */
  *toksz=0; 
  while ((*toksz < *bsz) && (!ws[(int)*(*c+*toksz)])) (*toksz)++;
  if (*toksz) return TOK_STR; 

  return -1;
}

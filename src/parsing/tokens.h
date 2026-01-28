#ifndef QGUM_TOK
#define QGUM_TOK
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef enum QGUM_TOKEN_KIND
{
  E_O_F = 0,
  OPARAN,
  CPARAN,
  COMMA,
  SEMICOL,
  IDENTIFIER,
  ASTERIX,
  EQUALS,
  STRING,
  NUMBER,
  FLOAT,
  ESCAPE,
  WHITESPACE,
  VALUES,
  INSERT,
  INTO,
  CREATE,
  TABLE,
  CONNECTION,
  WITH,
  SHOW

} TokenKind;

typedef struct
{
  int str_length;
  int str_max;
  char* str;
  char str_type;

} token_state;

extern token_state yylex_state;

static inline void
resize_str (int length, bool append)
{

  if (append)
  {
    length = length + yylex_state.str_length;
  }

  if (length > yylex_state.str_max)
  {

    yylex_state.str_max = length * 2;
    yylex_state.str = realloc (yylex_state.str, yylex_state.str_max);
  }
}

static inline void
set_str (char* str, int length)
{
  resize_str (length, false);
  strcpy (yylex_state.str, str);
  yylex_state.str_length = length;
}

static inline void
append_str (char* str, int length)
{
  // printf ("%d length\n", length);
  // printf ("%s + str: %s length: %d \n",
  // str,
  // yylex_state.str,
  // yylex_state.str_length);

  resize_str (length, true);

  strcpy (yylex_state.str + yylex_state.str_length, str);

  yylex_state.str_length += length;
  // printf ("%s + str: %s\n", str, yylex_state.str);
}

#endif

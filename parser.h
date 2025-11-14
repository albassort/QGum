#include <stdbool.h>
#include <jansson.h>
#include <stdint.h>
#include <m-dict.h>
#include "deps/clex/clex.h"
#define VARNAME_MAX_LENGTH 1024
#define NUMBER_OF_VERBS 3
#define FBSIZE 1024
#define NUMBER_OF_DATABASES 1
#define NUMBER_OF_CREATES 1

typedef enum
{
  QGUM_AST_TYPE_INVALID = 0,
  QGUM_AST_TYPE_INT = 1,
  QGUM_AST_TYPE_UINT = 2,
  QGUM_AST_TYPE_STRING = 3,
  QGUM_AST_TYPE_FLOAT = 4,
  QGUM_AST_TYPE_VARIABLE = 5,
  QGUM_AST_TYPE_OTHER = 6,
  QGUM_AST_VERB_CONNECT = 7,
  QGUM_AST_VERB_CREATE = 8,
  QGUM_AST_VERB_INSERT = 9,
} qgum_key_types;

typedef enum
{
  QGUM_DATABASE_UNKNOWN,
  QGUM_DATABASE_POSTGRES
} db_connection_type;

typedef enum
{
  QGUM_CREATE_PLOT

} qgum_create_types;

typedef enum
{
  QGUM_AST_VERB_READING,
  QGUM_AST_VERB_CONNECT_READING,
  QGUM_COMPLETE,
} state_context;

typedef enum
{
  DEFAULT,
  QGUM_VARNAME_READ,
  QGUM_PAREN_OPEN_READ,
  QGUM_KV_READ,
  QGUM_PAREN_CLOSE_READ,
} state_lower;

DICT_DEF2 (k_v,
           const char*,
           M_CSTR_OPLIST,
           const char*,
           M_CSTR_OPLIST)

DICT_DEF2 (k_type,
           const char*,
           M_CSTR_OPLIST,
           qgum_key_types,
           M_BASIC_OPLIST)

typedef struct
{
  char varname[VARNAME_MAX_LENGTH];
  bool has_var_name;
  k_v_t params;
  union
  {
    struct
    {
      db_connection_type db;

    } qgum_connection_ast;

    struct
    {
      qgum_create_types create_type;
      json_t* create_data;
    } qgum_create_ast;

    struct
    {
      struct q_gum_ast* ast;
      size_t num_of_cols;
      char** cols;
    } qgum_insert_ast;

    struct
    {
      uint8_t data;
      uint64_t length;
    } raw_data;
  };
  qgum_key_types type;
} q_gum_ast;

DICT_DEF2 (lex_lookup,
           const char*,
           M_CSTR_OPLIST,
           q_gum_ast*,
           M_PTR_OPLIST)

typedef enum QGUM_TOKEN_KIND
{
  E_O_F = -1,
  OPARAN,
  CPARAN,
  COMMA,
  SEMICOL,
  IDENTIFIER,
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
  START_COMMENT,
  END_COMMENT,

} TokenKind;

void
init_lexer (clexLexer** lexer)
{

  clexRegisterKind (*lexer, "/\\*", START_COMMENT);
  clexRegisterKind (*lexer, "\\*/", END_COMMENT);
  clexRegisterKind (*lexer, "\\\\+", ESCAPE);
  clexRegisterKind (*lexer, ",", COMMA);
  clexRegisterKind (*lexer, "[iI][nN][tT][oO]", INTO);
  clexRegisterKind (*lexer, "[wW][iI][tT][hH]", WITH);
  clexRegisterKind (*lexer, "[cC][rR][eE][aA][tT][eE]", CREATE);
  clexRegisterKind (*lexer, "[tT][aA][bB][lL][eE]", TABLE);
  clexRegisterKind (
    *lexer, "[cC][oO][nN][nN][eE][cC][tT]", CONNECTION);
  clexRegisterKind (*lexer, "[iI][nN][sS][eE][rR][tT]", INSERT);
  clexRegisterKind (*lexer, "[vV][aA][lL][uU][eE][sS]", VALUES);

  clexRegisterKind (*lexer, "\\(", OPARAN);
  clexRegisterKind (*lexer, "\\)", CPARAN);
  clexRegisterKind (*lexer, ";", SEMICOL);
  clexRegisterKind (*lexer, "=", EQUALS);

  clexRegisterKind (*lexer, "'", STRING);
  clexRegisterKind (*lexer, "`", STRING);
  clexRegisterKind (
    *lexer, "[a-zA-Z_]([a-zA-Z_]|[0-9])*", IDENTIFIER);
  clexRegisterKind (*lexer, "[+-]?[0-9]", NUMBER);
  clexRegisterKind (*lexer, "[+-]?([0-9]*[.])?[0-9]+", FLOAT);

  // clexReset (*lexer, "insert INTO TABLE\nTEST(A = 'aq\\c') ");
  // clexToken token;
  // while ((token = clex (*lexer)).kind != -1)
  // {
  //   printf ("%s{%d, %ld, %ld} ",
  //           token.lexeme,
  //           token.kind,
  //           token.linen,
  //           token.linepos);
  // }
}

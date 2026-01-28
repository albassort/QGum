#include "./tokens.h"
#include <stdbool.h>
#include <jansson.h>
#include <stdint.h>
#include <m-dict.h>

#ifndef QGUM_PARSER
#define QGUM_PARSER
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
  QGUM_AST_VERB_SHOW = 10
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
      char* insert_statement;
    } qgum_insert_ast;

    struct
    {
      uint8_t data;
      uint64_t length;
    } raw_data;

    struct
    {
      struct q_gum_ast* plot_to_show;
    } show_data;
  };
  qgum_key_types type;
} q_gum_ast;

DICT_DEF2 (lex_lookup,
           const char*,
           M_CSTR_OPLIST,
           q_gum_ast*,
           M_PTR_OPLIST)

void
init_data (void);

q_gum_ast*
read_qgum (char*, int*);

struct test
{
  int i;
};

#endif

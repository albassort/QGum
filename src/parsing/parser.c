#include <ctype.h>
#include <m-dict.h>
#include <m-string.h>
#include <clog.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <jansson.h>
#include <unistd.h>
#include "parser.h"
#include "./valid_keys.h"
#include "./lexer.h"
#include "tokens.h"
#include <stdio.h>

static json_t* valid_keys;
static k_type_t type_lookup;
static lex_lookup_t lex_lookup;

void
init_data (void)
{

  lex_lookup_init (lex_lookup);
  k_type_init (type_lookup);

  k_type_set_at (type_lookup, "string", QGUM_AST_TYPE_STRING);
  k_type_set_at (type_lookup, "uint", QGUM_AST_TYPE_UINT);
  k_type_set_at (type_lookup, "int", QGUM_AST_TYPE_INT);
  k_type_set_at (type_lookup, "float", QGUM_AST_TYPE_FLOAT);

  valid_keys =
    json_loadb ((char*) valid_keys_json, valid_keys_json_len, 0, 0);
}

q_gum_ast*
read_qgum (char*, int*);

static inline void
free_kv (k_v_t* map)
{
  int i = 0;
  int size = k_v_size (*map);

  k_v_it_t it;
  for (k_v_it (it, *map); size > i; k_v_next (it))
  {
    const k_v_itref_t* got = k_v_cref (it);
    if (got->key == NULL && got->value == NULL)
    {
      continue;
    }
    printf ("%.*s\n", 10, got->key);
    printf ("%s\n", got->value);
    free ((char*) got->key);
    free ((char*) got->value);
    printf ("!!\n");
    i++;
  }

  k_v_reset (*map);
  k_v_clear (*map);
}

int
valid_var_char (int c)
{
  return ((c >= '0' && '9' >= c) || (c >= 'A' && 'Z' >= c) ||
          (c >= '_' && 'z' >= c) || (c == '\\'));
}

/**
 *
 * This takes in a group, with a value, and evaluates its type.
 * The type is in a "type": "${type}" within the json. E.g
 * CONNECT_POSTGRESS.
 *
 * @param group The Json group
 * @param value The value within the group
 * @return The type found.
 */
qgum_key_types
get_json_param_type (json_t* root, char* key)
{

  json_t* v = json_object_get (root, key);

  if (v == NULL)
  {
    return QGUM_AST_TYPE_INVALID;
  }

  const char* type_value =
    json_string_value (json_object_get (v, "type"));

  if (type_value == NULL)
  {

    ERROR ("'TYPE' VALUE NOT FOUND ON KEY: %s", key);
    exit (1);
  };

  return *k_type_get (type_lookup, type_value);
}

typedef enum
{
  A,
  B
} buffmode;

const static char* verb_to_enum_string[NUMBER_OF_VERBS] = {
  "CONNECT",
  "CREATE",
  "INSERT"
};

const static qgum_key_types verb_to_enum_enum[NUMBER_OF_VERBS] = {
  QGUM_AST_VERB_CONNECT,
  QGUM_AST_VERB_CREATE,
  QGUM_AST_VERB_INSERT
};

const static char* database_strings[NUMBER_OF_DATABASES] = {
  "POSTGRES"
};

const static db_connection_type
  database_enums[NUMBER_OF_DATABASES] = { QGUM_DATABASE_POSTGRES };

const static char* create_strings[NUMBER_OF_CREATES] = { "PLOT" };

const static qgum_create_types create_to_enum[NUMBER_OF_CREATES] = {
  QGUM_CREATE_PLOT
};

int
match_associated_array (char* verb_str,
                        const char* const* arr1,
                        const int* arr2,
                        int length)
{
  for (int i = 0; i != length; i++)
  {
    // printf ("ASSOCIATE INT %d\n", i);
    const char* str = arr1[i];

    // printf ("%s %d\n", str, i);
    // printf ("vs %s %d\n", verb_str, i);

    if (strcmp (str, verb_str) == 0)
    {
      return arr2[i];
    }
  }
  return -1;
}

// checks if the string an odd or even escape.
static inline bool
is_escape (char* escape)
{
  int count = 0;
  // Assumes all are equal to \, from the lexer
  for (char* p = escape; *p != 0; p++)
  {
    count++;
  }

  FIXME ("Escape count: %d", count);
  return (count % 2) == 1;
}

void
parse_kv (q_gum_ast* AST, int* total_read, json_t* group)
{

  int token;

  static const int default_max_length = 1024;
  while (true)
  {
    char* key = malloc (default_max_length);
    char* value = malloc (default_max_length);
    token = yylex ();
    FIXME ("read key: %s", yylex_state.str);
    if (token != IDENTIFIER)
    {
    }
    strcpy (key, yylex_state.str);

    token = yylex ();
    FIXME ("reading equals: %s", yylex_state.str);

    if (token != EQUALS)
    {
    }

    TokenKind value_type;
    token = yylex ();
    // FIXME ("reading value: %s, %d", token.lexeme, token.);

    switch (token)
    {
      case STRING:
      {

        TRACE ("copied string of length %d, %s",
               yylex_state.str_length,
               yylex_state.str);
        strcpy (value, yylex_state.str);
        break;
      }
      case FLOAT:
      case NUMBER:
      {
        strcpy (value, yylex_state.str);
        break;
      }
      default:
      {
        ERROR ("[%ld] Expected string, number, float, got %s",
               yylineno,
               yylex_state.str);
        exit (1);
      }
    }

    TRACE ("read value: %s\n", value);
    value_type = token;

    token = yylex ();
    FIXME ("reading comma: %s", yytext);

    if (token != COMMA && token != CPARAN)
    {
      ERROR ("[%ld Expected comma, got %s", yylineno, yytext);
      exit (1);
    }

    bool end = token == CPARAN;

    for (char* p = key; *p != 0; p++)
    {
      *p = toupper (*p);
    }

    k_v_set_at (AST->params, key, value);
    // normalize key
    qgum_key_types correct_type = get_json_param_type (group, key);

    if (correct_type == QGUM_AST_TYPE_INVALID)
    {
      ERROR ("[%ld] Unknown key: %s", yylineno, yylex_state.str);
      exit (1);
    }
    // type checking
    switch (correct_type)
    {
      case QGUM_AST_TYPE_INT:
      case QGUM_AST_TYPE_FLOAT:
      {
        if (value_type == STRING)
        {
          ERROR ("[%ld expected INT or FLOAT for %s but got string",
                 yylineno,
                 key);
          exit (1);
        }
        break;
      }
      case QGUM_AST_TYPE_UINT:
      {
        if (value_type == STRING)
        {
          ERROR ("[%ld] expected INT or FLOAT for %s but got string",
                 yylineno,
                 key);
          exit (1);
        }
        char c = value[0];
        if (c == '-')
        {
          ERROR ("[%ld] only positive integers are allowed for %s!",
                 yylineno,
                 key);
          exit (1);
        }
        break;
      }

      case QGUM_AST_TYPE_STRING:

      {

        if (value_type != STRING)
        {
          ERROR ("[%ld] Expected string for %s got %s",
                 yylineno,
                 key,
                 value);
        }
        break;
      }

      case QGUM_AST_TYPE_INVALID:
      case QGUM_AST_TYPE_VARIABLE:
      case QGUM_AST_TYPE_OTHER:
      case QGUM_AST_VERB_CONNECT:
      case QGUM_AST_VERB_CREATE:
      case QGUM_AST_VERB_INSERT:
      default:
      {
        ERROR ("unreachable");
        exit (1);
      }
    }

    k_v_set_at (AST->params, key, value);

    (*total_read)++;
    if (end)
      break;
  }

  json_t* mandatory_args = json_object_get (group, "mandatory_args");

  size_t i = 0;
  json_t* v;
  json_array_foreach (mandatory_args, i, v)
  {
    const char* str = json_string_value (v);
    const char** got = k_v_safe_get (AST->params, str);
    if (*got == NULL)
    {
      ERROR ("MANDATORY ARG: %s MISSING", str);
      exit (1);
    }
  }
}
// forward decs
void
switch_read_stmt (void);
void
end_read_stmt (void);

void
read_insert (q_gum_ast* ast)
{
  switch_read_stmt ();
  yylex ();
  end_read_stmt ();
}

int
read_tuple_list (json_t* group, char*** output_array)
{
  int max_size = 1024;
  int max_columns = 32;
  char* output = malloc (max_size);
  *output_array = malloc (sizeof (char*) * max_columns);

  char* write_pos = output;
  int count = 0;
  TokenKind token;
  char* lexeme = yylex_state.str;
  while (true)
  {
    TRACE ("Tuple reading %c", count);
    token = yylex ();
    if (token == CPARAN)
    {
      ERROR ("[%ld] Expected columns for insert...", yylineno);
      exit (1);
    }
    else if (token != IDENTIFIER)
    {
      ERROR ("[%ld]%s Expected identifier got: %s", yylineno, lexeme);
      exit (1);
    }
    size_t length = yylex_state.str_length + 1;
    if (write_pos - output >= max_size)
    {
      output = realloc (output, max_size *= 2);
    }

    strcpy (write_pos, lexeme);
    if (count >= max_columns)
    {
      output = realloc (output_array, max_columns *= 2);
    }
    (*output_array)[count++] = write_pos;
    write_pos += length;

    token = yylex ();

    if (token == CPARAN)
    {
      break;
    }
    else if (token != COMMA)
    {
      ERROR ("[%ld] Expected comma or cparen for insert..., "
             "got '%s'",
             yylineno,
             lexeme);
    }
  }

  size_t i = 0;
  json_t* v;

  json_t* mandatory_cols =
    json_object_get (group, "mandatory_insert");

  json_array_foreach (mandatory_cols, i, v)
  {
    bool found = false;

    const char* refstr = json_string_value (v);
    for (int i = 0; count > i; i++)
    {
      TRACE ("%s", (*output_array)[i]);

      char* str = (*output_array)[i];
      found = strcmp (str, refstr) == 0;
      if (found)
        break;
    }
    if (!found)
    {
      ERROR ("MISSING MANDATORY INSERT COLUMN %s", refstr);
      exit (1);
    }
  };

  return count;
}

void
parse (TokenKind kind, q_gum_ast* ast)
{
  TokenKind current;

  char* lexeme = yylex_state.str;
  switch (kind)
  {
    case CONNECTION:
    {

      ast->type = QGUM_AST_VERB_CONNECT;
      printf ("ENTER\n");
      current = yylex ();

      if (current != IDENTIFIER)
      {
        ERROR ("[%ld]Expected Identifier", yylineno);
      }

      json_t* databse_objects =
        json_object_get (valid_keys, "DATABASE_CONNECTIONS");

      if (databse_objects == NULL)
      {
        ERROR ("Json data header corrupted!");
        exit (1);
      }

      // normalize string
      for (char* p = lexeme; *p != 0; p++)
      {
        *p = toupper (*p);
      }

      json_t* valid_params =
        json_object_get (databse_objects, lexeme);

      if (valid_params == NULL)
      {

        ERROR ("[%ld]%s is not a valid database.", yylineno, lexeme);
        exit (1);
      }

      db_connection_type database =
        match_associated_array (lexeme,
                                database_strings,
                                (const int*) database_enums,
                                NUMBER_OF_DATABASES);

      ast->qgum_connection_ast.db = database;

      current = yylex ();
      if (current != IDENTIFIER)
      {
        if (current == E_O_F)
          goto UNEXPEcTED_EOF;
        ERROR ("[%ld]Expected Identifier", yylineno);
        exit (1);
      }
      ast->has_var_name = true;
      strcpy (ast->varname, lexeme);

      q_gum_ast** ast_Loopup =
        lex_lookup_safe_get (lex_lookup, ast->varname);
      if (*ast_Loopup != NULL)
      {
        if (current == E_O_F)
          goto UNEXPEcTED_EOF;
        ERROR (
          "[%ld]Identifier %s already taken", yylineno, ast->varname);
        exit (1);
      }

      current = yylex ();
      if (current != OPARAN)
      {
        if (current == E_O_F)
          goto UNEXPEcTED_EOF;

        ERROR ("[%ld]expected '(' got  %s", yylineno, ast->varname);
      }

      int total_read = 0;
      parse_kv (ast, &total_read, valid_params);
      current = yylex ();
      if (current != SEMICOL)
      {
        ERROR ("[%ld]expected ';' got  %s", yylineno, ast->varname);

        exit (1);
      }
      lex_lookup_set_at (lex_lookup, ast->varname, ast);
      break;
    }
    case CREATE:
    {

      ast->type = QGUM_AST_VERB_CREATE;
      current = yylex ();

      if (current != IDENTIFIER)
      {
        if (current == E_O_F)
          goto UNEXPEcTED_EOF;
        ERROR ("[%ld]Expected Identifier", yylineno);
        exit (1);
      }

      for (char* p = lexeme; *p != 0; p++)
      {
        *p = toupper (*p);
      }

      json_t* creates = json_object_get (valid_keys, "VALID_CREATE");

      json_t* create_obj = json_object_get (creates, lexeme);

      if (current != IDENTIFIER)
      {
        if (current == E_O_F)
          goto UNEXPEcTED_EOF;
        ERROR ("[%ld]Expected Identifier", yylineno);
        exit (1);
      }

      if (create_obj == NULL)
      {
        ERROR ("[%ld]Unknown create type: %s", yylineno, lexeme);
        exit (1);
      }

      qgum_create_types type =
        match_associated_array (lexeme,
                                create_strings,
                                (int*) create_to_enum,
                                NUMBER_OF_CREATES);

      ast->qgum_create_ast.create_type = type;

      current = yylex ();

      if (current != IDENTIFIER)
      {
        if (current == E_O_F)
          goto UNEXPEcTED_EOF;
        ERROR ("[%ld]Expected Identifier", yylineno);
        exit (1);
      }
      TRACE ("varname: %s", lexeme);

      ast->has_var_name = true;
      strcpy (ast->varname, lexeme);

      q_gum_ast** ast_Loopup =
        lex_lookup_safe_get (lex_lookup, ast->varname);

      if (*ast_Loopup != NULL)
      {
        ERROR (
          "[%ld]Identifier %s already taken", yylineno, ast->varname);
        exit (1);
      }

      current = yylex ();

      int total_read = 0;
      parse_kv (ast, &total_read, create_obj);

      current = yylex ();
      if (current != SEMICOL)
      {
        ERROR ("[%ld]expected ';' got  %s", yylineno, ast->varname);

        exit (1);
      }
      TRACE ("REGISTERING: %s", ast->varname);
      ast->qgum_create_ast.create_data = create_obj;
      lex_lookup_set_at (lex_lookup, ast->varname, ast);
      break;
    }
    case INSERT:
    {
      ast->type = QGUM_AST_VERB_INSERT;
      current = yylex ();

      if (current != INTO)
      {
        if (current == E_O_F)
          goto UNEXPEcTED_EOF;
        ERROR ("[%ld]Expected Identifier", yylineno);
        exit (1);
      }

      current = yylex ();
      if (current != IDENTIFIER)
      {
        if (current == E_O_F)
          goto UNEXPEcTED_EOF;
        ERROR ("[%ld]Expected Identifier", yylineno);
        exit (1);
      }

      q_gum_ast** lexical = lex_lookup_safe_get (lex_lookup, lexeme);

      if ((*lexical) == NULL)
      {
        ERROR ("[%ld]Identifier %s doesn't exist", yylineno, lexeme);
        exit (1);
      }

      ast->qgum_insert_ast.ast = (struct q_gum_ast*) *lexical;

      current = yylex ();
      if (current != OPARAN)
      {
        ERROR (
          "[%ld]Expected open parentheses got %s", yylineno, lexeme);
        exit (1);
      }

      char** outstirs;
      int total = read_tuple_list (
        (*lexical)->qgum_create_ast.create_data, &outstirs);
      ast->qgum_insert_ast.cols = outstirs;
      ast->qgum_insert_ast.num_of_cols = total;

      current = yylex ();
      if (current != VALUES)
      {
        ERROR ("[%ld] expected values after insert, got '%s'",
               yylineno,
               lexeme);
        exit (1);
      }

      current = yylex ();
      if (current != WITH)
      {
        ERROR ("[%ld] expected with after values, got '%s'",
               yylineno,
               lexeme);
        exit (1);
      }

      current = yylex ();
      if (current != IDENTIFIER)
      {
        ERROR ("[%ld] expected identifier after with got '%s'",
               yylineno,
               lexeme);
        exit (1);
      }

      q_gum_ast** connection =
        lex_lookup_safe_get (lex_lookup, lexeme);

      if ((*connection) == NULL)
      {
        ERROR ("[%ld]Identifier %s doesn't exist", yylineno, lexeme);
        exit (1);
      }

      read_insert (ast);
      break;
    }
    case SHOW:
    {
      current = yylex ();
      if (current != IDENTIFIER)
      {
        ERROR (
          "[%ld] Identifer expected %s\n", yylineno, ast->varname);
        exit (1);
      }

      q_gum_ast** ast_Loopup =
        lex_lookup_safe_get (lex_lookup, lexeme);

      if (*ast_Loopup == NULL)
      {
        ERROR ("[%ld]Lookup failed for %s, the identifer after must "
               "be a varaible.",
               yylineno,
               ast->varname);
        exit (1);
      };

      ast->show_data.plot_to_show = (struct q_gum_ast*) *ast_Loopup;

      current = yylex ();

      if (current != SEMICOL)
      {
        ERROR (
          "[%ld] Semicol expected got %s\n", yylineno, ast->varname);
        exit (1);
      }
      ast->type = QGUM_AST_VERB_SHOW;
      break;
    }

    case E_O_F:
    {
      // Technically unreachable but we use this for unexpected EOF
    UNEXPEcTED_EOF:
    {
      ERROR ("[%ld]Unexpected EOF", yylineno);
      exit (1);
    }
    break;
    }

    default:
    {
      // unreachable
      exit (1);
      break;
    }
  }
}

void
free_ast (q_gum_ast* asts, int count)
{
  for (int i = 0; count > i; i++)
  {
    q_gum_ast ast = asts[i];

    // k_v_clear (ast.params);

    free_kv (&ast.params);
    if (ast.type == QGUM_AST_VERB_INSERT)
    {
      free (ast.qgum_insert_ast.insert_statement);

      char** cols = ast.qgum_insert_ast.cols;
      free (*cols);
      free (cols);
    }
    else if (ast.type == QGUM_AST_VERB_CONNECT)
    {
    }
    else if (ast.type == QGUM_AST_VERB_CREATE)
    {
    }
  }
}
//
q_gum_ast*
read_qgum (char* path, int* count)
{
  yyin = stdin;
  char* file_buf;

  TokenKind token;
  int max_ast = 32;
  int cur_ast = 0;
  char* lexeme = yylex_state.str;

  q_gum_ast* asts = calloc (sizeof (q_gum_ast), max_ast);

  while ((token = yylex ()) != E_O_F)
  {

    printf ("\nkind: %d, text : %d, str: %s\n",
            token,
            yylex_state.str_length,
            lexeme);
    if (cur_ast == max_ast)
      asts = realloc (asts, max_ast *= 2);

    for (char* p = lexeme; *p != 0; p++)
    {
      *p = toupper (*p);
    }

    printf ("%d<- conn,  we have %d\n", CONNECTION, token);
    switch (token)
    {
      case CREATE:
      case INSERT:
      case CONNECTION:
      case SHOW:
      {
        k_v_init (asts[cur_ast].params);
        k_v_reserve (asts[cur_ast].params, 1024);
        parse (token, &asts[cur_ast++]);

        break;
      }
      default:
      {
        ERROR (
          "Highest level must be CREATE, INSERT, CONNECT, got %s\n",
          *lexeme);
        exit (1);
      }
    }
  }

  TRACE ("EOF reached.");

  *count = cur_ast;
  return asts;
}

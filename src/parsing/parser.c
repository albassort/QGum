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
#include "../../deps/clex/clex.h"
#include "./valid_keys.h"

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

clexToken
clex_sc (clexLexer* lexer)
{
  clexToken result;
  result = clex (lexer);
  if (result.kind == START_COMMENT)
  {

    while ((result = clex (lexer)).kind != END_COMMENT)
    {
      if (result.kind == E_O_F)
      {
        ERROR ("[%ld:%ld]Comment unterminated",
               result.linen,
               result.linepos);
        exit (1);
      }
    }

    result = clex (lexer);
    return result;
  };

  return result;
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

static inline void
agregate_value (clexLexer* lexer,
                char** str,
                int* max_length,
                int* length_written,
                char start_char)
{
  clexToken token;
  bool escaped = false;
  *length_written = 0;
  while (true)
  {

    token = clex_sc (lexer);
    TRACE ("in agregate_value string, got %s kind: %d",
           token.lexeme,
           token.kind);

    if (token.kind == E_O_F)
    {
      ERROR ("[%ld:%ld]Unexpected EOF", token.linen, token.linepos);
      exit (1);
    }

    if (token.kind == ESCAPE && is_escape (token.lexeme))
    {
      TRACE ("ESCAPED!");
      escaped = true;
      continue;
    }
    else if (token.kind == STRING && !escaped &&
             token.lexeme[0] == start_char)
    {

      (*str)[(*length_written) + 1] = 0;
      return;
    }

    else
    {
      TRACE ("Copying");
      // TODO: implement length in clex;
      int length = strlen (token.lexeme);
      if (length + 1 + *length_written > *max_length)
      {
        *str = realloc (*str, (*max_length) *= 2);
      }

      printf ("copying: %s\n", token.lexeme);

      strcpy ((*str) + *length_written, token.lexeme);

      (*length_written) += length + 1;
      (*str)[*length_written - 1] = ' ';
    }

    if (escaped)
      escaped = false;
  }
}

void
parse_kv (clexLexer* lexer,
          q_gum_ast* AST,
          int* total_read,
          json_t* group)
{

  clexToken token;

  static const int default_max_length = 1024;
  while (true)
  {
    char* key = malloc (default_max_length);
    char* value = malloc (default_max_length);
    token = clex_sc (lexer);
    FIXME ("read key: %s", token.lexeme);
    if (token.kind != IDENTIFIER)
    {
    }
    strcpy (key, token.lexeme);

    token = clex_sc (lexer);
    FIXME ("reading equals: %s", token.lexeme);

    if (token.kind != EQUALS)
    {
    }

    TokenKind value_type;
    token = clex_sc (lexer);
    // FIXME ("reading value: %s, %d", token.lexeme, token.);

    switch (token.kind)
    {
      case STRING:
      {
        int max_length = default_max_length;
        int length_written = 0;
        agregate_value (lexer,
                        &value,
                        &max_length,
                        &length_written,
                        token.lexeme[0]);

        TRACE ("copied string of length %d, %.*s",
               length_written,
               length_written,
               value);
        printf ("%s\n", value);
        break;
      }
      case FLOAT:
      case NUMBER:
      {
        strcpy (value, token.lexeme);
        break;
      }
      default:
      {
        ERROR ("[%ld,%ld] Expected string, number, float, got %s",
               token.linen,
               token.linepos,
               token.lexeme);
        exit (1);
      }
    }

    value_type = token.kind;

    token = clex_sc (lexer);
    FIXME ("reading comma: %s", token.lexeme);

    if (token.kind != COMMA && token.kind != CPARAN)
    {
      ERROR ("[%ld,%ld] Expected comma, got %s",
             token.linen,
             token.linepos,
             token.lexeme);
      exit (1);
    }

    bool end = token.kind == CPARAN;

    for (char* p = key; *p != 0; p++)
    {
      *p = toupper (*p);
    }

    k_v_set_at (AST->params, key, value);
    // normalize key
    qgum_key_types correct_type = get_json_param_type (group, key);

    if (correct_type == QGUM_AST_TYPE_INVALID)
    {
      ERROR (
        "[%ld,%ld] Unknown key: %s", token.linen, token.linepos, key);
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
          ERROR (
            "[%ld,%ld] expected INT or FLOAT for %s but got string",
            token.linen,
            token.linepos,
            key);
          exit (1);
        }
        break;
      }
      case QGUM_AST_TYPE_UINT:
      {
        if (value_type == STRING)
        {
          ERROR (
            "[%ld,%ld] expected INT or FLOAT for %s but got string",
            token.linen,
            token.linepos,
            key);
          exit (1);
        }
        char c = value[0];
        if (c == '-')
        {
          ERROR (
            "[%ld,%ld] only positive integers are allowed for %s!",
            token.linen,
            token.linepos,
            key);
          exit (1);
        }
        break;
      }

      case QGUM_AST_TYPE_STRING:

      {

        if (value_type != STRING)
        {
          ERROR ("[%ld,%ld] Expected string for %s got %s",
                 token.linen,
                 token.linepos,
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

void
read_insert (clexLexer* lexer, q_gum_ast* ast)
{
  size_t max_size = 1024;
  ast->qgum_insert_ast.insert_statement = malloc (max_size);
  char* out = ast->qgum_insert_ast.insert_statement;
  char* cur_pos = out;
  bool in_string = false;
  bool escaped = false;
  clexToken token;
  char start_char = 0;
  while (true)
  {
    // NOTE: as of right now I intend to interpret the executed SQL
    // literally; thus, we dont use clex_sc.
    //
    token = clex (lexer);

    if (token.kind == STRING && in_string && !escaped &&
        token.lexeme[0] == start_char)
    {
      in_string = false;
    }
    else if (token.kind == STRING && !in_string)
    {
      start_char = token.lexeme[0];

      in_string = true;
    }

    else if (token.kind == ESCAPE && in_string &&
             is_escape (token.lexeme))
    {
      escaped = true;
      goto write;
    }
    else if (token.kind == SEMICOL && !in_string)
    {
      cur_pos[0] = ';';
      cur_pos[1] = 0;
      TRACE ("read command: %s", out);
      return;
    }
    else if (token.kind == E_O_F)
    {
      ERROR ("UNEXPEcTED EOF");
      exit (1);
    }

    if (escaped)
    {
      escaped = false;
    }
  write:
  {

    if (token.lexeme == NULL)
    {
      continue;
    };

    int lexeme_size = strlen (token.lexeme);
    size_t pos = cur_pos - out;
    if (pos + lexeme_size + 2 >= max_size)
    {
      out = realloc (out, max_size *= 2);
    }
    strcpy (cur_pos, token.lexeme);
    cur_pos += lexeme_size;
    cur_pos[0] = ' ';
    cur_pos++;
    TRACE ("Copying: %s", token.lexeme);
  }
  };
}

int
read_tuple_list (clexLexer* lex, json_t* group, char*** output_array)
{
  int max_size = 1024;
  int max_columns = 32;
  char* output = malloc (max_size);
  *output_array = malloc (sizeof (char*) * max_columns);

  char* write_pos = output;
  int count = 0;
  clexToken token;
  while (true)
  {
    TRACE ("Tuple reading %c", count);
    token = clex_sc (lex);
    if (token.kind == CPARAN)
    {
      ERROR ("[%ld:%ld]%s Expected columns for insert...",
             token.linen,
             token.linepos);
      exit (1);
    }
    else if (token.kind != IDENTIFIER)
    {
      ERROR ("[%ld:%ld]%s Expected identifier got: %s",
             token.linen,
             token.linepos,
             token.lexeme);
      exit (1);
    }

    size_t length = strlen (token.lexeme) + 1;
    if (write_pos - output >= max_size)
    {
      output = realloc (output, max_size *= 2);
    }

    strcpy (write_pos, token.lexeme);
    if (count >= max_columns)
    {
      output = realloc (output_array, max_columns *= 2);
    }
    (*output_array)[count++] = write_pos;
    write_pos += length;

    token = clex_sc (lex);

    if (token.kind == CPARAN)
    {
      break;
    }
    else if (token.kind != COMMA)
    {
      ERROR ("[%ld:%ld] Expected comma or cparen for insert..., "
             "got '%s'",
             token.linen,
             token.linepos,
             token.lexeme);
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
parse (clexLexer* lexer, TokenKind kind, q_gum_ast* ast)
{
  clexToken current;
  switch (kind)
  {
    case CONNECTION:
    {

      ast->type = QGUM_AST_VERB_CONNECT;
      printf ("ENTER\n");
      current = clex_sc (lexer);
      if (current.kind != IDENTIFIER)
      {

        ERROR ("[%ld:%ld]Expected Identifier",
               current.linen,
               current.linepos);
      }

      json_t* databse_objects =
        json_object_get (valid_keys, "DATABASE_CONNECTIONS");

      if (databse_objects == NULL)
      {
        ERROR ("Json data header corrupted!");
        exit (1);
      }

      // normalize string
      for (char* p = current.lexeme; *p != 0; p++)
      {
        *p = toupper (*p);
      }

      json_t* valid_params =
        json_object_get (databse_objects, current.lexeme);

      if (valid_params == NULL)
      {

        ERROR ("[%ld:%ld]%s is not a valid database.",
               current.linen,
               current.linepos,
               current.lexeme);
        exit (1);
      }

      db_connection_type database =
        match_associated_array (current.lexeme,
                                database_strings,
                                (const int*) database_enums,
                                NUMBER_OF_DATABASES);

      ast->qgum_connection_ast.db = database;

      current = clex_sc (lexer);
      if (current.kind != IDENTIFIER)
      {
        if (current.kind == E_O_F)
          goto UNEXPEcTED_EOF;
        ERROR ("[%ld:%ld]Expected Identifier",
               current.linen,
               current.linepos);
        exit (1);
      }
      ast->has_var_name = true;
      strcpy (ast->varname, current.lexeme);

      q_gum_ast** ast_Loopup =
        lex_lookup_safe_get (lex_lookup, ast->varname);
      if (*ast_Loopup != NULL)
      {
        if (current.kind == E_O_F)
          goto UNEXPEcTED_EOF;
        ERROR ("[%ld:%ld]Identifier %s already taken",
               current.linen,
               current.linepos,
               ast->varname);
        exit (1);
      }

      current = clex_sc (lexer);
      if (current.kind != OPARAN)
      {
        if (current.kind == E_O_F)
          goto UNEXPEcTED_EOF;

        ERROR ("[%ld:%ld]expected '(' got  %s",
               current.linen,
               current.linepos,
               ast->varname);
      }

      int total_read = 0;
      parse_kv (lexer, ast, &total_read, valid_params);
      current = clex_sc (lexer);
      if (current.kind != SEMICOL)
      {
        ERROR ("[%ld:%ld]expected ';' got  %s",
               current.linen,
               current.linepos,
               ast->varname);

        exit (1);
      }
      lex_lookup_set_at (lex_lookup, ast->varname, ast);
      break;
    }
    case CREATE:
    {

      ast->type = QGUM_AST_VERB_CREATE;
      current = clex_sc (lexer);

      if (current.kind != IDENTIFIER)
      {
        if (current.kind == E_O_F)
          goto UNEXPEcTED_EOF;
        ERROR ("[%ld:%ld]Expected Identifier",
               current.linen,
               current.linepos);
        exit (1);
      }

      for (char* p = current.lexeme; *p != 0; p++)
      {
        *p = toupper (*p);
      }

      json_t* creates = json_object_get (valid_keys, "VALID_CREATE");

      json_t* create_obj = json_object_get (creates, current.lexeme);

      if (current.kind != IDENTIFIER)
      {
        if (current.kind == E_O_F)
          goto UNEXPEcTED_EOF;
        ERROR ("[%ld:%ld]Expected Identifier",
               current.linen,
               current.linepos);
        exit (1);
      }

      if (create_obj == NULL)
      {
        ERROR ("[%ld:%ld]Unknown create type: %s",
               current.linen,
               current.linepos,
               current.lexeme);
        exit (1);
      }

      qgum_create_types type =
        match_associated_array (current.lexeme,
                                create_strings,
                                (int*) create_to_enum,
                                NUMBER_OF_CREATES);

      ast->qgum_create_ast.create_type = type;

      current = clex_sc (lexer);

      if (current.kind != IDENTIFIER)
      {
        if (current.kind == E_O_F)
          goto UNEXPEcTED_EOF;
        ERROR ("[%ld:%ld]Expected Identifier",
               current.linen,
               current.linepos);
        exit (1);
      }
      TRACE ("varname: %s", current.lexeme);

      ast->has_var_name = true;
      strcpy (ast->varname, current.lexeme);

      q_gum_ast** ast_Loopup =
        lex_lookup_safe_get (lex_lookup, ast->varname);

      if (*ast_Loopup != NULL)
      {
        ERROR ("[%ld:%ld]Identifier %s already taken",
               current.linen,
               current.linepos,
               ast->varname);
        exit (1);
      }

      current = clex_sc (lexer);

      int total_read = 0;
      parse_kv (lexer, ast, &total_read, create_obj);

      current = clex_sc (lexer);
      if (current.kind != SEMICOL)
      {
        ERROR ("[%ld:%ld]expected ';' got  %s",
               current.linen,
               current.linepos,
               ast->varname);

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
      current = clex_sc (lexer);

      if (current.kind != INTO)
      {
        if (current.kind == E_O_F)
          goto UNEXPEcTED_EOF;
        ERROR ("[%ld:%ld]Expected Identifier",
               current.linen,
               current.linepos);
        exit (1);
      }

      current = clex_sc (lexer);
      if (current.kind != IDENTIFIER)
      {
        if (current.kind == E_O_F)
          goto UNEXPEcTED_EOF;
        ERROR ("[%ld:%ld]Expected Identifier",
               current.linen,
               current.linepos);
        exit (1);
      }

      q_gum_ast** lexical =
        lex_lookup_safe_get (lex_lookup, current.lexeme);

      if ((*lexical) == NULL)
      {
        ERROR ("[%ld:%ld]Identifier %s doesn't exist",
               current.linen,
               current.linepos,
               current.lexeme);
        exit (1);
      }

      ast->qgum_insert_ast.ast = (struct q_gum_ast*) *lexical;

      current = clex_sc (lexer);
      if (current.kind != OPARAN)
      {
        ERROR ("[%ld:%ld]Expected open parentheses got %s",
               current.linen,
               current.linepos,
               current.lexeme);
        exit (1);
      }

      char** outstirs;
      int total = read_tuple_list (
        lexer, (*lexical)->qgum_create_ast.create_data, &outstirs);
      ast->qgum_insert_ast.cols = outstirs;
      ast->qgum_insert_ast.num_of_cols = total;

      current = clex_sc (lexer);
      if (current.kind != VALUES)
      {
        ERROR ("[%ld:%ld] expected values after insert, got '%s'",
               current.linen,
               current.linepos,
               current.lexeme);
        exit (1);
      }

      current = clex_sc (lexer);
      if (current.kind != WITH)
      {
        ERROR ("[%ld:%ld] expected with after values, got '%s'",
               current.linen,
               current.linepos,
               current.lexeme);
        exit (1);
      }

      current = clex_sc (lexer);
      if (current.kind != IDENTIFIER)
      {
        ERROR ("[%ld:%ld] expected identifier after with got '%s'",
               current.linen,
               current.linepos,
               current.lexeme);
        exit (1);
      }

      q_gum_ast** connection =
        lex_lookup_safe_get (lex_lookup, current.lexeme);

      if ((*connection) == NULL)
      {
        ERROR ("[%ld:%ld]Identifier %s doesn't exist",
               current.linen,
               current.linepos,
               current.lexeme);
        exit (1);
      }

      read_insert (lexer, ast);
      break;
    }
    case E_O_F:
    {
      // Technically unreachable but we use this for unexpected EOF
    UNEXPEcTED_EOF:
    {
      ERROR (
        "[%ld:%ld]Unexpected EOF", current.linen, current.linepos);
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

//
q_gum_ast*
read_qgum (char* path, int* count)
{
  FILE* input_path;
  char* file_buf;
  int length = 0;
  if (path == NULL)
  {
    printf ("meow\n");
    input_path = stdin;
    char c = 0;
    int fbufmax = 1024;
    file_buf = malloc (fbufmax);
    while ((c = fgetc (stdin)) != EOF)
    {
      if (length >= fbufmax)
      {
        file_buf = realloc (file_buf, fbufmax *= 2);
      }
      file_buf[length++] = c;
    }
    length++;
  }
  else
  {
    input_path = fopen (path, "r");

    fseek (input_path, 0, SEEK_END);

    length = ftell (input_path);
    rewind (input_path);
    file_buf = malloc (length + 1);
    fread (file_buf, 1, length, input_path);
    file_buf[length] = 0;
  }

  clexLexer* lexer = clexInit ();
  init_lexer (&lexer);

  file_buf[length] = 0;

  printf ("%s\n", file_buf);
  clexReset (lexer, file_buf);
  clexToken token;

  int max_ast = 32;
  int cur_ast = 0;

  q_gum_ast* asts = calloc (sizeof (q_gum_ast), max_ast);

  while ((token = clex_sc (lexer)).kind != E_O_F)
  {
    if (cur_ast == max_ast)
      asts = realloc (asts, max_ast *= 2);

    for (char* p = token.lexeme; *p != 0; p++)
    {
      *p = toupper (*p);
    }

    printf ("kind: %d\n", token.kind);
    switch (token.kind)
    {
      case CREATE:
      case INSERT:
      case CONNECTION:
      {
        k_v_init (asts[cur_ast].params);
        k_v_reserve (asts[cur_ast].params, 1024);
        parse (lexer, token.kind, &asts[cur_ast++]);

        break;
      }
      default:
      {
        ERROR (
          "Highest level must be CREATE, INSERT, CONNECT\n, got %s",
          token.lexeme);
        exit (1);
      }
    }
  }

  for (int i = 0; cur_ast > i; i++)
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
  free (file_buf);
  clexLexerDestroy (lexer);
  TRACE ("EOF reached.");

  *count = cur_ast;
  return asts;
}

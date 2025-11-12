#include <ctype.h>
#include <m-dict.h>
#include <m-string.h>
#include <clog.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <jansson.h>
#include "valid_keys.h"
#include "parser.h"

static inline void
free_kv (k_v_t* map)
{
  int i = 0;
  int size = k_v_size (*map);

  k_v_it_t it;
  for (k_v_it (it, *map); i != size; k_v_next (it))
  {
    free ((char*) k_v_cref (it)->key);
    free ((char*) k_v_cref (it)->value);
    i++;
  }

  k_v_clear (*map);
}

json_t* valid_keys;
k_type_t type_lookup;
lex_lookup_t lex_lookup;

void

init_json (void)
{

  lex_lookup_init (lex_lookup);
  k_type_init (type_lookup);

  k_type_set_at (type_lookup, "string", QGUM_AST_TYPE_STRING);
  k_type_set_at (type_lookup, "uint", QGUM_AST_TYPE_UINT);
  k_type_set_at (type_lookup, "int", QGUM_AST_TYPE_INT);

  valid_keys =
    json_loadb ((char*) valid_keys_json, valid_keys_json_len, 0, 0);
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
get_json_param_type (json_t* root, const char* group, char** value)
{
  json_t* g = json_object_get (root, group);

  if (g == NULL)
  {
    return QGUM_AST_TYPE_INVALID;
  }

  json_t* v = json_object_get (g, *value);

  if (v == NULL)
  {
    return QGUM_AST_TYPE_INVALID;
  }

  const char* type_value =
    json_string_value (json_object_get (v, "type"));

  if (type_value == NULL)
  {

    ERROR ("'TYPE' VALUE NOT FOUND ON GROUP: %s, VALUE: %s",
           group,
           *value);
    exit (1);
  };

  return *k_type_get (type_lookup, type_value);
}

/**
 * Validates a string as a parameter
 * */
qgum_key_types
validate_string (char* str)
{
  bool escaped = false;

  int escape_counter = 0;
  // Because the str]0] == '\'' was done higher up;
  str++;

  for (char* p = str; *p != 0; p++)
  {
    char c = *p;

    if (escape_counter == 0)
    {
      escaped = false;
    }

    if (c == '\\')
    {
      escaped = true;
      escape_counter = 2;
    }
    else if (c == '\'' && !escaped)
    {

      if (p[1] == 0)
      {
        return QGUM_AST_TYPE_STRING;
      }
      else
      {
        return QGUM_AST_TYPE_INVALID;
      }
    }

    if (escape_counter > 0)
    {
      escape_counter--;
    }
  }

  return QGUM_AST_TYPE_STRING;
}

/**
 * Takes in a raw parameter, and identifies its type. E.g -512 -> int;
 * "Hello" -> string
 * */
qgum_key_types
identify_raw_key (char* str)
{
  char c = *str;
  if (c == '\'')
  {
    return validate_string (str);
  }
  else if (c == '-' || isdigit (c))
  {
    if (c == '-')
      str++;

    bool has_decimal = false;

    for (char* p = str; *p != 0; p++)
    {
      char c = *p;

      if (c == '.' && !has_decimal)
      {
        has_decimal = true;
        continue;
      }
      else if (c == '.' && has_decimal)
      {
        return QGUM_AST_TYPE_INVALID;
      }
      else if (isdigit (c))
      {
        continue;
      }
      else
      {
        return QGUM_AST_TYPE_INVALID;
      }
    }

    if (has_decimal)
    {
      return QGUM_AST_TYPE_FLOAT;
    }

    return QGUM_AST_TYPE_INT;
  }
  else
  {
    // Its not a string; it doesn't start with ` or an integer
    // Attempts to check if it is a variable
    for (char* p = str; *p != 0; p++)
    {
      char c = *p;
      bool valid = valid_var_char (c);
      if (!valid)
      {
        return QGUM_AST_TYPE_OTHER;
      }
    }

    return QGUM_AST_TYPE_VARIABLE;
  };

  return QGUM_AST_TYPE_OTHER;
}

typedef enum
{
  A,
  B
} buffmode;

static uint8_t abuff[FBSIZE];
static uint8_t bbuff[FBSIZE];

typedef struct
{
  FILE* file;
  uint64_t read_pos;

  bool at_end;
  bool read;

  uint64_t size_left;

  uint8_t* forward_buff;
  uint8_t* backwards_buff;

  buffmode mode;

} FileReader;

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

void
flip (FileReader* reader)
{
  if (reader->mode == A)
  {
    reader->forward_buff = bbuff;
    reader->backwards_buff = abuff;
    reader->mode = B;
  }
  else
  {
    reader->forward_buff = abuff;
    reader->backwards_buff = bbuff;
    reader->mode = B;
  }
}

bool
refill_filebuff (FileReader* reader)
{

  if (reader->at_end)
  {
    ERROR ("UNEXPECTED END OF FILE");
    exit (1);
    return true;
  }

  flip (reader);
  int read = fread (reader->forward_buff, 1, FBSIZE, reader->file);

  TRACE ("READ SIZE: %d\n", read);
  reader->at_end = read != FBSIZE;
  reader->read_pos = 0;
  reader->size_left = read;
  return false;
}

char
read_next_char (FileReader* reader, bool* done)
{
  if (reader->at_end && reader->read_pos == reader->size_left)
  {
    if (done == NULL)
    {
      ERROR ("UNEXPEcTED END OF FILE");
      exit (1);
    }
    else
    {
      TRACE ("End of file reached!");
      *done = true;
    }
  }
  else if (!reader->read || reader->read_pos == FBSIZE)
  {
    if (!reader->read)
    {
      reader->read = true;
    }

    refill_filebuff (reader);
  }

  return reader->forward_buff[reader->read_pos++];
}

char
stream_read_util_char (FileReader* reader, char chary)
{
  char c;
  while (true)
  {
    c = read_next_char (reader, 0);
    if (c == chary)
    {
      return c;
    }
  }
}

char
stream_filter_comments (FileReader* reader, char c)
{
  char one = 0;
  char two = 0;

  if (c == '-' || c == '/')
  {

    char next = read_next_char (reader, 0);

    if (c == '/' && next == '*')
    {
      one = '*';
      two = '/';
    }

    else if (next == '-' && c == '-')
    {

      char c = stream_read_util_char (reader, '\n');
      return c;
    }
    else
    {
      // NOTE: im not sure if this words if read_pos == 0...
      reader->read_pos -= 1;
      return c;
    }
  }
  else
  {
    return c;
  }

  while (true)
  {
    char next = read_next_char (reader, 0);
    if (next == one)
    {
      char nextnext = read_next_char (reader, 0);
      if (nextnext == two)
      {
        return read_next_char (reader, 0);
      }
    }
  }
}

char
stream_read_util_valid_ascii (FileReader* reader, bool* done)
{
  char c;
  while (true)
  {
    c = read_next_char (reader, done);
    char c_proper = stream_filter_comments (reader, c);

    // printf ("%c - %c\n", c, c_proper);

    if (isascii (c_proper))
    {
      return c_proper;
    }
  }
}

void
stream_read_statement (FileReader* reader,
                       char** buff,
                       int* max_length,
                       bool* end_of_file)
{
  int i = 0;
  bool at_end = false;
  bool read_char = false;
  bool in_quote = false;
  bool escape = false;
  while (true)
  {

    if (i >= *max_length)
    {
      *max_length *= 2;
      *buff = realloc (*buff, *max_length);
    }

    char c = stream_read_util_valid_ascii (reader, &at_end);
    // printf ("%c - %b - %b\n", c, in_quote, escape);
    if (in_quote && escape)
    {

      (*buff)[i++] = c;
      escape = false;
      continue;
    }

    if (c == '\\' && in_quote)
    {
      escape = true;
    }
    else if (c == '\'' && !escape)
    {
      in_quote = !in_quote;
    }
    else if (c == ';' && !in_quote)
    {
      (*buff)[i] = 0;
      return;
    }
    else if (!read_char && (!isspace (c) && c != 0))
    {
      read_char = true;
    }
    else if (at_end)
    {
      if (read_char)
      {

        (*buff)[i] = 0;
        ERROR ("Unexpected end of file.");
        exit (1);
      }
      else
      {
        FIXME ("Normal end of file");
        *end_of_file = true;
        (*buff)[i] = 0;
        return;
      }
    }

    (*buff)[i++] = c;
  }
}

// inline static int
// printable (int c)
// {
//   return ((c >= '!' && '<' >= c) || (c >= '>' && '~' >= c));
// }
//
//
int
read_word (char* str,
           char** out_buf,
           int maxlen,
           int (*validator) (int),
           int* written)
{
  bool reading = false;
  int i = 0;
  int total_read = 0;
  for (char* p = str; *p != 0; p++)
  {
    total_read++;
    bool is_valid = validator (*p);

    if (!is_valid && !reading)
      continue;

    char c = toupper (*p);

    if (is_valid && !reading)
    {
      reading = true;
      (*out_buf)[i++] = c;
    }
    else if (is_valid && reading)
    {
      (*out_buf)[i++] = c;
    }
    else if ((!is_valid && reading) || i == maxlen - 1)
    {
      break;
    }
  }

  (*out_buf)[i++] = 0;
  *written = i;
  return total_read;
}

// O(1)...
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

/**
 * Parses a raw value, after a '=', and reads its value into @param
 * value.
 * */
int
parse_value (char* str,
             char** value,
             int max_length,
             bool* at_end,
             int* written)
{
  int i = 0;
  int total_read = 0;

  bool pre_reading = false;
  bool in_string = false;
  for (char* p = str; *p != 0; p++)
  {
    total_read++;
    char c = *p;
    str = p;
    if (c == '=')
    {
      pre_reading = true;
      break;
    }
  }

  if (!pre_reading)
  {
    return 0;
    *written = -1;
  };

  str++;

  while (isspace (*str))
  {
    total_read++;
    str++;
  }

  // printf ("AAAAAA %c\n", *str);
  // " =    'meow'," ->  "meow,"

  bool escaped = false;
  int escape_counter = 0;
  in_string = *str == '\'';
  for (char* p = str; (*p != 0 && max_length > i); p++)
  {
    if (escape_counter == 0)
      escaped = false;

    char c = *p;
    if (!in_string)
    {
      if (c == ',')
      {
        break;
      }

      else if (c == ')')
      {
        *at_end = true;
        break;
      }
      else if (!valid_var_char (c))
      {
        continue;
      }
    }

    if (c == '\'' && in_string && !escaped)
    {
      in_string = false;
    }
    else if (c == '\\' && !escaped)
    {
      escaped = true;
      escape_counter = 2;
    }

    if (escape_counter > 0)
    {
      escape_counter--;
    }

    (*value)[i++] = c;
  }

  (*value)[i] = 0;
  *written = i + 1;
  return total_read;
}

void
parse_kv (char* str,
          q_gum_ast* AST,
          int* total_read,
          char* group,
          json_t* root)
{
  bool at_end = false;
  k_v_t* kv = &AST->qgum_connection_ast.params;

  k_v_init (*kv);
  int written = 0;

  while (!at_end)
  {
    char* key = malloc (128);
    char* value = malloc (128);

    // printf ("CURRENT: %s\n", str);

    int len = read_word (str, &key, 127, isalpha, &written);

    for (int i = 0; i != written; i++)
    {
      key[i] = toupper (key[i]);
    }

    FIXME ("Got key: %.*s, total written: %d", written, key, written);
    qgum_key_types set = get_json_param_type (root, group, &key);

    if (set == QGUM_AST_TYPE_INVALID)
    {
      ERROR ("INVALID KEY: %s!", key);
      exit (1);
    }

    str += len;
    *total_read += len;
    len = parse_value (str, &value, 127, &at_end, &written);
    str += len;
    *total_read += len;

    FIXME (
      "Got value: %.*s, total written: %d", written, value, written);

    qgum_key_types detected_type = identify_raw_key (value);

    if (detected_type == QGUM_AST_TYPE_VARIABLE)
    {

      q_gum_ast** ast = lex_lookup_safe_get (lex_lookup, value);

      if (*ast == NULL)
      {
        ERROR ("UNDEFINED SYMBOL: %s", value);
        exit (1);
      };
      detected_type = (*ast)->type;
    }
    if (detected_type != set)
    {

      ERROR ("INCORRECT TYPE: %s! %d;%d", key, detected_type, set);
      exit (1);
    }
    k_v_set_at (*kv, key, value);
  };

  json_t* g = json_object_get (root, group);
  json_t* mandatory = json_object_get (g, "mandatory_args");
  size_t index;
  json_t* value;

  json_array_foreach (mandatory, index, value)
  {
    const char* v = json_string_value (value);
    TRACE ("mandatory: %s\n", v);

    const char** k = k_v_safe_get (*kv, v);

    if (*k == NULL)
    {
      ERROR ("%s is a mandatory key, not found.\n", v);
      exit (1);
    }
  }
}

void
parse (char* buf, q_gum_ast* AST, int start_pos)
{
  int written = 0;
  int offset = start_pos;

  // state state = { .upper = QGUM_AST_VERB_READING, .lower = DEFAULT
  // };

  char* word = malloc (VARNAME_MAX_LENGTH);
  int len =
    read_word (buf, &word, VARNAME_MAX_LENGTH, isalpha, &written);

  int verb_type =
    match_associated_array (word,
                            verb_to_enum_string,
                            (const int*) verb_to_enum_enum,
                            NUMBER_OF_VERBS);

  if (0 >= verb_type)
  {
    ERROR ("Invalid command %s at %d\n", word, offset + start_pos);
    exit (1);
  }

  offset += len;
  buf += offset;

  AST->type = verb_type;

  switch (verb_type)
  {
    case QGUM_AST_VERB_CONNECT:
    {
      int len =
        read_word (buf, &word, VARNAME_MAX_LENGTH, isalpha, &written);

      db_connection_type database =
        match_associated_array (word,
                                database_strings,
                                (const int*) database_enums,
                                NUMBER_OF_DATABASES);
      if (0 >= database)
      {
        ERROR (
          "Unknown database %s, at %d", word, offset + start_pos);
        exit (1);
      }

      offset += len;
      buf += len;

      AST->qgum_connection_ast.db = database;

      len = read_word (
        buf, &word, VARNAME_MAX_LENGTH, valid_var_char, &written);

      AST->has_var_name = true;
      strcpy (AST->varname, word);

      q_gum_ast** taken_check =
        lex_lookup_safe_get (lex_lookup, AST->varname);

      if (*taken_check != NULL)
      {
        ERROR ("SYMBOL NAME TAKEN %s", word);
        exit (1);
      }
      else
      {
        lex_lookup_set_at (lex_lookup, AST->varname, AST);
      }

      int wait_length = 0;
      for (char* p = buf; *p != '('; p++)
      {
        wait_length += 1;
        if (*p == 0)
        {
          ERROR ("EXPECTED '(', but string ended at %d",
                 offset + wait_length);
          exit (1);
        }
        continue;
      }

      offset += wait_length + 1;
      buf += wait_length + 1;
      int total_read = 0;
      parse_kv (
        buf, AST, &total_read, "CONNECT_POSTGRESS", valid_keys);

      k_v_out_str (stdout, AST->qgum_connection_ast.params);
      break;
    }
    case QGUM_AST_VERB_INSERT:
    {
      int len =
        read_word (buf, &word, VARNAME_MAX_LENGTH, isalpha, &written);

      TRACE ("%s\n", word);
      if (strcmp (word, "INTO") != 0)
      {
        ERROR ("Expected `INTO` after `INSERT`");
        exit (1);
      };

      offset += len;
      buf += len;

      len = read_word (
        buf, &word, VARNAME_MAX_LENGTH, valid_var_char, &written);
      TRACE ("VARNAME = %s\n", word);

      q_gum_ast** ast = lex_lookup_safe_get (lex_lookup, word);

      if (*ast == NULL)
      {
        ERROR ("UNDEFINED SYMBOL: %s", word);
        exit (1);
      };

      offset += len;
      buf += len;

      len =
        read_word (buf, &word, VARNAME_MAX_LENGTH, isalpha, &written);

      if (strcmp (word, "WITH") != 0)
      {
        ERROR ("Expected WITH after varname in INSERT got %s\n",
               word);
        exit (1);
      }

      offset += len;
      buf += len;

      len = read_word (
        buf, &word, VARNAME_MAX_LENGTH, valid_var_char, &written);

      q_gum_ast** connection = lex_lookup_safe_get (lex_lookup, word);

      if (*connection == NULL)
      {
        ERROR ("UNDEFINED SYMBOL, EXPECTED CONNECTION: %s", word);
        exit (1);
      };

      if ((*connection)->type != QGUM_AST_VERB_CONNECT)
      {
        ERROR ("%s IS SUPPOSED TO BE OF TYPE `CONNECT`", word);
        exit (1);
      }

      offset += len;
      buf += len;
      printf ("ongoing\n");
      break;
    }
    case QGUM_AST_VERB_CREATE:
    {

      AST->type = QGUM_AST_VERB_CREATE;

      len =
        read_word (buf, &word, VARNAME_MAX_LENGTH, isalpha, &written);

      json_t* valid_create =
        json_object_get (valid_keys, "VALID_CREATE");

      json_t* group = json_object_get (valid_create, word);

      if (group == NULL)
      {
        ERROR ("INVALID TYPE FOR CREATE: %s", word);
        exit (1);
      }

      TRACE ("creating: %s", word);

      char create_type[128];
      strcpy (create_type, word);

      offset += len;
      buf += len;

      qgum_create_types create_enum =
        match_associated_array (word,
                                create_strings,
                                (int*) create_to_enum,
                                NUMBER_OF_CREATES);

      AST->qgum_create_ast.create_type = create_enum;

      if ((int) create_enum == -1)
      {
        ERROR ("CREATE ENUM LOOKUP FAILED\n");
        exit (1);
      }

      len = read_word (
        buf, &word, VARNAME_MAX_LENGTH, valid_var_char, &written);

      AST->has_var_name = true;
      strcpy (AST->varname, word);
      q_gum_ast** ast =
        lex_lookup_safe_get (lex_lookup, AST->varname);
      TRACE ("VARNAME: %s", word);

      if (*ast)
      {
        ERROR ("LEX LOOKUP TAKEN: %s", word);
        exit (1);
      }
      offset += len - 1;
      buf += len - 1;

      int wait_length = 0;
      for (char* p = buf; *p != '('; p++)
      {
        printf ("%c\n", *p);
        wait_length += 1;
        if (*p == 0)
        {
          ERROR ("EXPECTED '(', but string ended at %d",
                 offset + wait_length);
          exit (1);
        }
        continue;
      }

      offset += wait_length + 1;
      buf += wait_length + 1;

      printf ("%s\n", buf);
      int total_read = 0;
      parse_kv (buf, AST, &total_read, create_type, valid_create);

      lex_lookup_set_at (lex_lookup, AST->varname, AST);
      break;
    }
    default:
    {
      ERROR ("unreachable.");
      exit (1);
    }
  }

  free (word);
}

int
main (void)
{
  init_json ();
  FILE* file = fopen ("./toparse.qgum", "r");
  fseek (file, 0, SEEK_END);
  // int length = ftell (file);
  rewind (file);

  FileReader reader = { .file = file,
                        .at_end = false,
                        .read_pos = 0,
                        .read = false,
                        .forward_buff = abuff,
                        .backwards_buff = bbuff,
                        .mode = A };

  int max_size = 1024;

  int ast_max_count = 16;
  int ast_count = 0;
  q_gum_ast* parsed_ast = malloc (sizeof (q_gum_ast) * ast_max_count);

  char* current_buf = malloc (max_size);
  bool end_of_file = false;
  while (true)
  {
    if (ast_count >= ast_max_count)
    {
      ast_max_count *= 2;
      parsed_ast =
        realloc (parsed_ast, sizeof (q_gum_ast) * ast_max_count);
    }

    q_gum_ast* current_ast = &parsed_ast[ast_count++];

    stream_read_statement (
      &reader, &current_buf, &max_size, &end_of_file);

    if (end_of_file)
    {
      break;
    }
    parse (current_buf, current_ast, 0);
  }

  free (current_buf);

  return 1;
}

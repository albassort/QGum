#include <ctype.h>
#include <m-dict.h>
#include <m-string.h>
#include <clog.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <jansson.h>
#include "valid_keys.h"

typedef enum
{
  QGUM_KEY_TYPE_INVALID = 0,
  QGUM_KEY_TYPE_INT = 1,
  QGUM_KEY_TYPE_UINT = 2,
  QGUM_KEY_TYPE_STRING = 3,
  QGUM_KEY_TYPE_FLOAT = 4,
  QGUM_KEY_TYPE_OTHER = 5
} qgum_key_types;

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

json_t* valid_keys;
k_type_t type_lookup;

void

init_json ()
{

  k_type_init (type_lookup);

  k_type_set_at (type_lookup, "string", QGUM_KEY_TYPE_STRING);
  k_type_set_at (type_lookup, "uint", QGUM_KEY_TYPE_UINT);
  k_type_set_at (type_lookup, "int", QGUM_KEY_TYPE_INT);

  valid_keys =
    json_loadb ((char*) valid_keys_json, valid_keys_json_len, 0, 0);
}

qgum_key_types
test_set (const char* group, char** value)
{
  json_t* g = json_object_get (valid_keys, group);

  if (g == NULL)
  {
    return QGUM_KEY_TYPE_INVALID;
  }

  json_t* v = json_object_get (g, *value);

  const char* type_value =
    json_string_value (json_object_get (v, "type"));

  printf ("TYPE FOUND: %s - %d\n",
          type_value,
          *k_type_get (type_lookup, type_value));
  return *k_type_get (type_lookup, type_value);
};

qgum_key_types
validate_string (char* str)
{
  bool escaped = false;
  printf ("%s fuck\n", str);

  // Because the str]0] == '\'' was done higher up;
  str++;

  for (char* p = str; *p != 0; p++)
  {
    char c = *p;
    printf ("AA: %c\n", c);
    if (c == '\\')
    {
      escaped = true;
    }
    else if (c == '\'')
    {
      if (escaped)
      {
        escaped = false;
        continue;
      }
      else
      {
        if (p[1] == 0)
        {
          printf ("meow!!\n");
          return QGUM_KEY_TYPE_STRING;
        }
        else
        {
          return QGUM_KEY_TYPE_INVALID;
        }
      }
    }
  }

  return QGUM_KEY_TYPE_STRING;
}

qgum_key_types
identify_str (char* str)
{
  printf ("probed %s\n", str);
  char c = *str;
  if (c == '\'')
  {
    printf ("THIS\n");
    return validate_string (str);
  }
  else if (c == '-' || isdigit (c))
  {
    printf ("isdigit\n");
    if (c == '-')
      str++;

    bool has_decimal = false;

    for (char* p = str; *p != 0; p++)
    {
      char c = *p;

      printf ("meow %c\n", c);
      if (c == '.' && !has_decimal)
      {
        has_decimal = true;
        continue;
      }
      else if (c == '.' && has_decimal)
      {
        return QGUM_KEY_TYPE_INVALID;
      }
      else if (isdigit (c))
      {
        continue;
      }
      else
      {
        printf ("terminator %c\n", c);
        return QGUM_KEY_TYPE_INVALID;
      }
    }

    if (has_decimal)
    {
      return QGUM_KEY_TYPE_FLOAT;
    }

    return QGUM_KEY_TYPE_INT;
  }
  else
  {
    // Case not starting with ' or digit/-
    return QGUM_KEY_TYPE_OTHER;
  };

  return QGUM_KEY_TYPE_OTHER;
}

typedef struct
{
  union
  {
    int b;
  };
  int i;
} example;

typedef struct
{
  example* ptr;
} pointer_holder;

typedef enum
{
  A,
  B
} buffmode;

#define FBSIZE 1024

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

typedef enum
{
  QGUM_VERB_READING,
  QGUM_VERB_CONNECT_READING,
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

typedef enum
{
  QGUM_DATABASE_UNKNOWN,
  QGUM_DATABASE_POSTGRES
} db_connection_type;

typedef enum
{
  QGUM_INVALID = 0,
  QGUM_VERB_CONNECT = 1,
  QGUM_VERB_CREATE = 2,
  QGUM_VERB_INSERT = 3
} ast_type;

#define VARNAME_MAX_LENGTH 1024

typedef struct
{
  char varname[VARNAME_MAX_LENGTH];
  bool has_var_name;
  union
  {
    struct
    {
      db_connection_type db;
      k_v_t db_params;

    } qgum_connection_ast;
  };
  ast_type type;
} q_gum_ast;

typedef struct
{
  state_context upper;
  state_lower lower;
} state;

#define NUMBER_OF_VERBS 3

const static char* verb_to_enum_string[NUMBER_OF_VERBS] = {
  "CONNECT",
  "CREATE",
  "INSERT"
};

const static ast_type verb_to_enum_enum[NUMBER_OF_VERBS] = {
  QGUM_VERB_CONNECT,
  QGUM_VERB_CREATE,
  QGUM_VERB_INSERT
};

#define NUMBER_OF_DATABASES 1

const static char* database_strings[NUMBER_OF_DATABASES] = {
  "POSTGRES"
};

const static db_connection_type
  database_enums[NUMBER_OF_DATABASES] = { QGUM_DATABASE_POSTGRES };

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
read_next_char (FileReader* reader)
{
  if (reader->at_end && reader->read_pos == reader->size_left)
  {
    TRACE ("End of file reached!");
    exit (1);
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
};

char
stream_read_util_char (FileReader* reader, char chary)
{
  char c;
  while (true)
  {
    c = read_next_char (reader);
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

    char next = read_next_char (reader);

    // printf ("here: %c -> next: %c\n", c, next);
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
    char next = read_next_char (reader);
    if (next == one)
    {
      char nextnext = read_next_char (reader);
      if (nextnext == two)
      {
        return read_next_char (reader);
      }
    }
  }
}

char
stream_read_util_valid_ascii (FileReader* reader)
{
  char c;
  while (true)
  {
    c = read_next_char (reader);
    char cProper = stream_filter_comments (reader, c);

    printf ("%c - %c\n", c, cProper);

    if (isascii (cProper))
    {
      return cProper;
    }
  }
}

void
stream_read_util_char_valid (FileReader* reader,
                             char** buff,
                             int* max_length)
{
  int i = 0;
  while (true)
  {
    if (i >= *max_length)
    {
      *max_length *= 2;
      *buff = realloc (*buff, *max_length);
    }

    char c = stream_read_util_valid_ascii (reader);

    printf ("%c\n", c);
    if (c == ';')
    {
      printf ("\n");
      (*buff)[i] = 0;
      return;
    }

    (*buff)[i++] = c;
  }
}

int
valid_var_char (int c)
{

  return ((c >= '0' && '9' >= c) || (c >= 'A' && 'Z' >= c) ||
          (c >= '_' && 'z' >= c));
};

int
printable (int c)
{

  return ((c >= '!' && '<' >= c) || (c >= '>' && '~' >= c));
};

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
      (*out_buf)[i++] = 0;
      break;
    }
  }
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
  return 0;
};

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

  while ((*str == ' ') || (*str == '\n'))
  {
    total_read++;
    str++;
  }

  printf ("AAAAAA %c\n", *str);
  // " =    'meow'," ->  "meow,"

  for (char* p = str; *p != 0; p++)
  {
    total_read++;
    char c = *p;
    if (c == ',' && !in_string)
    {
      break;
    }
    else if (c == ')' && !in_string)
    {
      *at_end = true;
      break;
    }
    else if (c == '\n')
      continue;
    else if (c == '\'')
    {
      in_string = !in_string;
      (*value)[i++] = c;
    }
    else
    {
      if (i == max_length)
      {
        break;
      }

      (*value)[i++] = c;
    }
  }

  (*value)[i] = 0;
  *written = i + 1;
  return total_read;
};

void
parse_kv (char* str, q_gum_ast* AST, int* total_read)
{
  bool at_end = false;
  k_v_t* kv = &AST->qgum_connection_ast.db_params;

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

    printf ("written %d\n", written);
    qgum_key_types set = test_set ("CONNECT_POSTGRESS", &key);

    if (set == QGUM_KEY_TYPE_INVALID)
    {
      ERROR ("INVALID KEY: %s!", key);
      exit (1);
    }

    str += len;
    *total_read += len;
    len = parse_value (str, &value, 127, &at_end, &written);
    str += len;
    *total_read += len;

    qgum_key_types detected_type = identify_str (value);

    if (detected_type != set)
    {

      ERROR ("INCORRECT TYPE: %s! %d;%d", key, detected_type, set);
      exit (1);
    }

    k_v_set_at (*kv, key, value);
  };
};

void
parse (char* buf, q_gum_ast* AST, int start_pos)
{
  int written = 0;
  int offset = start_pos;

  // state state = { .upper = QGUM_VERB_READING, .lower = DEFAULT };

  char* word = malloc (24);
  int len = read_word (buf, &word, 24, isalpha, &written);

  int ast_type =
    match_associated_array (word,
                            verb_to_enum_string,
                            (const int*) verb_to_enum_enum,
                            NUMBER_OF_VERBS);

  if (0 >= ast_type)
  {
    ERROR ("Invalid command %s at %d\n", word, offset + start_pos);
    exit (1);
  }

  offset += len;
  buf += offset;

  AST->type = ast_type;

  switch (ast_type)
  {
    case QGUM_INVALID:
    {
      break;
    }
    case QGUM_VERB_CONNECT:
    {

      printf ("here\n");
      int len = read_word (buf, &word, 24, isalpha, &written);
      printf ("here\n");

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

      len = read_word (buf, &word, 24, valid_var_char, &written);
      // TODO: check len, etc

      printf ("var name %s\n", word);
      AST->has_var_name = true;
      strcpy (AST->varname, word);

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
      printf ("%s\n", buf);
      int total_read = 0;
      parse_kv (buf, AST, &total_read);

      k_v_out_str (stdout, AST->qgum_connection_ast.db_params);
      break;
    }
    case QGUM_VERB_INSERT:
    {
      break;
    }
    case QGUM_VERB_CREATE:
    {
      break;
    }
  }

  exit (1);
};

int
main ()
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
  char* current_buf = malloc (max_size);

  stream_read_util_char_valid (&reader, &current_buf, &max_size);

  q_gum_ast ast;
  parse (current_buf, &ast, 0);
  return 1;
}

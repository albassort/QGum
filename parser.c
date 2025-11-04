#include "m-core.h"
#include <ctype.h>
#include <m-dict.h>
#include <m-string.h>
#include <clog.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

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
  QGUM_VERB
} read_mode;

DICT_DEF2 (dict_string, string_t, M_STRING_OPLIST, int*, M_PTR_OPLIST)

const static char* action_words[2] = { "CONNECT", "CREATE" };

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
  char c = 'a';
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
read_word (char* str, char** out_buf, int maxlen)
{
  bool reading = false;
  int i = 0;
  for (char* p = str; *p != 0; p++)
  {
    bool alpha = isalpha (*p);

    if (!alpha && !reading)
      continue;

    char c = toupper (*p);

    if (alpha && !reading)
    {
      reading = true;
      (*out_buf)[i++] = c;
    }
    else if (alpha && reading)
    {
      (*out_buf)[i++] = c;
    }
    else if (!alpha && reading || i == maxlen - 1)
    {
      (*out_buf)[i++] = 0;
      break;
    }
  }
  return i;
}

int
main ()
{

  FILE* file = fopen ("./toparse.qgum", "r");
  fseek (file, 0, SEEK_END);
  int length = ftell (file);
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
  char* word = malloc (24);

  printf ("%s\n", current_buf);
  int len = read_word (current_buf, &word, 24);
  printf ("%s\n", word);
  len = read_word (current_buf + len, &word, 24);
  printf ("%s\n", word);
  return 1;
}

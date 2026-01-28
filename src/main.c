#include "parsing/parser.h"
#include "./parsing/lexer.h"
#include "./python/matplotlib.h"
int
main (void)
{

  init_data ();
  int count = 0;
  yylex_state.str = malloc (1024);
  yylex_state.str_max = 1024;
  q_gum_ast* ast = read_qgum (NULL, &count);
  test ();
}

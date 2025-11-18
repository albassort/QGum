#include "parsing/parser.h"
#include "./parsing/lexer.h"
int
main (void)
{
  init_data ();
  int count = 0;
  char* tet = "Hello The Word";
  yy_scan_string (tet);

  yylex ();
  q_gum_ast* ast = read_qgum (NULL, &count);
}

%option header-file="lexer.h"
%{
  #include  <stdio.h>
  #include  "./temp.h"
%}

%%
[a-zA-Z_]([a-zA-Z_]|[0-9])* { 
  return 1;
}

%%



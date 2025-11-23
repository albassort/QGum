%option   header-file="lexer.h"
%option   outfile="qgum-scanner.c"
%option   yylineno
%option noyywrap

%{
  #include  <stdio.h>
  #include  <stdbool.h>
  #include  "./tokens.h"
  int prev_state;
  token_state yylex_state;
%}

%x escape
%x in_string
%x line_comment
%x multi_line_comment
%x read_statement

%%

<INITIAL,read_statement>; {
  
  set_str(yytext, yyleng);
  return SEMICOL;
}
`
<INITIAL,read_statement>(\") {
  yylex_state.str_type = '"';

  yylex_state.str_length = 0;
  prev_state = YY_START;
  BEGIN(in_string);
}

<INITIAL,read_statement>(\') {
  yylex_state.str_type = '\'';
  yylex_state.str_length = 0;
  printf("STR START");
  prev_state = YY_START;
  BEGIN(in_string);
}

<INITIAL>, {
  set_str(yytext, yyleng);
  printf("\nCOMMA\n");
  return COMMA;
}

<INITIAL>(\() {
  set_str(yytext, yyleng);
  return OPARAN;
}


<INITIAL>(\)) {
  set_str(yytext, yyleng);
  return CPARAN;
}

<INITIAL>(=) {
  set_str(yytext, yyleng);
  return EQUALS;
}

<INITIAL>[iI][nN][tT][oO] {

  set_str(yytext, yyleng);
  return INTO;
}

<INITIAL>[wW][iI][tT][hH] {

  set_str(yytext, yyleng);
  return WITH;
}

<INITIAL>[cC][rR][eE][aA][tT][eE] {

  set_str(yytext, yyleng);
  return CREATE;
}


<INITIAL>[cC][oO][nN][nN][eE][cC][tT] {
  set_str(yytext, yyleng);
  return CONNECTION;

}

<INITIAL>[tT][aA][bB][lL][eE] {

  set_str(yytext, yyleng);
  return TABLE;
}

<INITIAL>[vV][aA][lL][uU][eE][sS] {

  set_str(yytext, yyleng);
  return VALUES;
}

<INITIAL>[iI][nN][sS][eE][rR][tT] { 

  set_str(yytext, yyleng);
  return INSERT; 
}

<INITIAL>[a-zA-Z_]([a-zA-Z_]|[0-9])* {
  printf("HERE: %s", yytext);
  set_str(yytext, yyleng);
  return IDENTIFIER; 
}

<INITIAL>[+-]?[0-9] {
    set_str(yytext, yyleng);
    return IDENTIFIER; 
}

<INITIAL>[+-]?([0-9]*[.])?[0-9]+ { 
    set_str(yytext, yyleng);
    return FLOAT; 
}

<in_string>(\") {
  if (yylex_state.str_type == '"'){
    BEGIN(prev_state);
    return STRING;
    yylex_state.str_length = 0;
  }
  else{
    append_str(yytext, yyleng);
  }
}

<in_string>' {
  yylex_state.str[yylex_state.str_length] = 0;
  if (yylex_state.str_type == '\''){
    BEGIN(prev_state);
    return STRING;
  }
  else{

    printf("ELSE!");
    append_str(yytext, yyleng);
  }
}

<in_string>(\\+) {

  bool is_odd = yyleng % 2 == 1;
  if (is_odd){
    BEGIN(escape);
  }
  if (yyleng > 1){
    if (is_odd){
      yytext[yyleng-1] = 0;      
      yyleng-=1;
    }
    append_str(yytext, yyleng);
  }
}



<in_string>(.) {
  printf("READING: %c\n", yytext[0]);
  append_str(yytext, yyleng);
}

<escape>(.|\n) {
  append_str(yytext, yyleng);
  BEGIN(in_string);
}


-- {

  prev_state = YY_START;

  if (YY_START != in_string){
    BEGIN(line_comment);
  }
}

(\/*) {
  prev_state = YY_START;
  BEGIN(multi_line_comment);
}

<multi_line_comment>(\*\/) {
  BEGIN(prev_state);
}


<line_comment>\n {
  BEGIN(prev_state);
}



%%

extern void switch_read_stmt() {
    BEGIN(read_statement);
};

extern void end_read_stmt() {
    BEGIN(INITIAL);
};


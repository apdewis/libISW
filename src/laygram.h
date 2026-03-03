/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_LAYYY_Y_TAB_H_INCLUDED
# define YY_LAYYY_Y_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int LayYYdebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    OC = 258,                      /* OC  */
    CC = 259,                      /* CC  */
    OA = 260,                      /* OA  */
    CA = 261,                      /* CA  */
    OP = 262,                      /* OP  */
    CP = 263,                      /* CP  */
    NAME = 264,                    /* NAME  */
    NUMBER = 265,                  /* NUMBER  */
    INFINITY = 266,                /* INFINITY  */
    VERTICAL = 267,                /* VERTICAL  */
    HORIZONTAL = 268,              /* HORIZONTAL  */
    EQUAL = 269,                   /* EQUAL  */
    DOLLAR = 270,                  /* DOLLAR  */
    PLUS = 271,                    /* PLUS  */
    MINUS = 272,                   /* MINUS  */
    TIMES = 273,                   /* TIMES  */
    DIVIDE = 274,                  /* DIVIDE  */
    PERCENTOF = 275,               /* PERCENTOF  */
    PERCENT = 276,                 /* PERCENT  */
    WIDTH = 277,                   /* WIDTH  */
    HEIGHT = 278,                  /* HEIGHT  */
    UMINUS = 279,                  /* UMINUS  */
    UPLUS = 280                    /* UPLUS  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif
/* Token kinds.  */
#define YYEMPTY -2
#define YYEOF 0
#define YYerror 256
#define YYUNDEF 257
#define OC 258
#define CC 259
#define OA 260
#define CA 261
#define OP 262
#define CP 263
#define NAME 264
#define NUMBER 265
#define INFINITY 266
#define VERTICAL 267
#define HORIZONTAL 268
#define EQUAL 269
#define DOLLAR 270
#define PLUS 271
#define MINUS 272
#define TIMES 273
#define DIVIDE 274
#define PERCENTOF 275
#define PERCENT 276
#define WIDTH 277
#define HEIGHT 278
#define UMINUS 279
#define UPLUS 280

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 20 "laygram.y"

    int		    ival;
    XrmQuark	    qval;
    BoxPtr	    bval;
    BoxParamsPtr    pval;
    GlueRec	    gval;
    LayoutDirection lval;
    ExprPtr	    eval;
    Operator	    oval;

#line 128 "y.tab.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE LayYYlval;


int LayYYparse (void);


#endif /* !YY_LAYYY_Y_TAB_H_INCLUDED  */

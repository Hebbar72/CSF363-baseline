%define api.value.type { ParserValue }

%code requires {
#include <iostream>
#include <vector>
#include <string>

#include "parser_util.hh"
#include "symbol.hh"

extern int yyerror(std::string msg);

}

%code {

#include <cstdlib>

extern int yylex();
extern int yyparse();

extern NodeStmts* final_values;

SymbolTable symbol_table;

int yyerror(std::string msg);

}

%token TPLUS TDASH TSTAR TSLASH
%token <lexeme> TINT_LIT TIDENT TTYPE
%token INT TLET TDBG
%token TSCOL TLPAREN TRPAREN TEQUAL
%token TCOLON 

%type <node> Expr Stmt
%type <stmts> Program StmtList

%left TPLUS TDASH
%left TSTAR TSLASH

%%

Program :                
        { final_values = nullptr; }
        | StmtList TSCOL 
        { final_values = $1; }
	    ;

StmtList : Stmt                
         { $$ = new NodeStmts(); $$->push_back($1); }
	     | StmtList TSCOL Stmt 
         { $$->push_back($3); }
	     ;

Stmt : TLET TIDENT TCOLON TTYPE TEQUAL Expr
     {
        if(symbol_table.contains($2)) {
            // tried to redeclare variable, so error
            yyerror("tried to redeclare variable.\n");
        } else {
            symbol_table.insert($2, $4);

            if($4 == "short" && $6->data_type > 1)
                yyerror("Type Mismatch");
            else if($4 == "int" && $6->data_type > 2)
                yyerror("Type Mismatch");
            else if($4 == "long" && $6->data_type > 3)
                yyerror("Type Mismatch");
            
            $$ = new NodeDecl($2, $4, $6);
        }
     }
     | TIDENT TEQUAL Expr
     {
        if(!symbol_table.contains($1)) {
            // tried to redeclare variable, so error
            yyerror("Undeclared variable.\n");
        } else {
            std::string temp = symbol_table.get_value($1);
            if(temp == "short" && $3->data_type > 1)
                yyerror("Type Mismatch");
            else if(temp == "int" && $3->data_type > 2)
                yyerror("Type Mismatch");
            else if(temp == "long" && $3->data_type > 3)
                yyerror("Type Mismatch");
            
            $$ = new NodeAssign($1, temp, $3);
        }
     }
     | TDBG Expr
     { 
        $$ = new NodeDebug($2);
     }
     ;

Expr : TINT_LIT               
     { $$ = new NodeInt(stoi($1)); }
     | TIDENT
     { 
        if(symbol_table.contains($1))
        {
            std::string x = symbol_table.get_value($1);
            if(x == "short")
                $$ = new NodeIdent($1, 1);
            else if(x == "int")
                $$ = new NodeIdent($1, 2);
            else 
                $$ = new NodeIdent($1, 3); 
        }
            
        else
            yyerror("using undeclared variable.\n");
     }
     | Expr TPLUS Expr
     { $$ = new NodeBinOp(NodeBinOp::PLUS, $1, $3); }
     | Expr TDASH Expr
     { $$ = new NodeBinOp(NodeBinOp::MINUS, $1, $3); }
     | Expr TSTAR Expr
     { $$ = new NodeBinOp(NodeBinOp::MULT, $1, $3); }
     | Expr TSLASH Expr
     { $$ = new NodeBinOp(NodeBinOp::DIV, $1, $3); }
     | TLPAREN Expr TRPAREN { $$ = $2; }
     ;

%%

int yyerror(std::string msg) {
    std::cerr << "Error! " << msg << std::endl;
    exit(1);
}

%{
#include <stdio.h>
#include <ast/ast.h>
#include <string>
#include <fmt/core.h>
void yyerror(const char *s);
extern int yylex(void);
extern NodePtr root;
%}

/// types
%union {
    int ival;                       // value
    // std::string     name;           // ident name
    OpType          op;             // operation
    
    std::vector<int>*                   vecInt;     // array_int
    std::vector<TreeIntegerLiteral*>*   vecTreeInt;
    std::vector<TreeVarDef*>*           vecTreeVarDef; 
    std::vector<TreeExpr*>*             vecTreeExpr;
    std::vector<TreeStmt*>*             vecStmt;
    std::vector<TreeFuncParam*>*        vecFuncParam;

    TreeExpr*       expr;           // expression
    TreeStmt*       stmt;           // statement
    TreeType*       type;           // type
    TreeIdent*      ident;          // ident 
    TreeIntegerLiteral* integer;    // 
    TreeVarDef*     varDef;         // var def
    TreeVarDecl*    varDecl;        // var decl
    TreeLVal*       lVal;           // left value
    TreeFuncRParams* funcRP;        // func call's params
    TreeFuncParam*  funcP;          // func's param
    TreeFuncParams* funcPs;         // 
    CompUnit*       comp;           // 
    Node*           nptr;           // node
    
}
/// define token
%token INT VOID                                 // "int"
%token <ival> NUM                               // number
%token ADD SUB MUL DIV AND NOT OR  MOD          // + - * / % && ! || ^ *
%token RP LP RB LB RC LC                        // () [] {}
%token SEMI COMMA                               // ; , 
%token IF ELSE WHILE BREAK CONTINUE RETURN      // if else while break continue return
%token LT LE GT GE EQ NE                        // < <= > >= == !=
%token ASSIGNOP                                 // =
%token IDENT                                    // 
%token comment

%start CompUnit



%type <ident>       Ident 
/* %type <type>        Btype FuncType */
%type <comp>        CompUnit    
%type <varDef>      VarDef
%type <integer>     Number

%type <vecTreeInt>      ArryList
%type <vecTreeVarDef>   VarDef_
%type <vecTreeExpr>     LValList FuncRP_
%type <vecStmt>         BlockItems 
%type <vecFuncParam>    FuncParam_

%type <funcP>           FuncParam    
%type <funcRP>          FuncRParams
%type <lVal>        LVal
%type <varDecl>     VarDecl      
%type <funcPs>      FuncParams

%type <stmt>        Stmt IFStmt Block BlockItem FuncDef CompUnit_ Decl
%type <expr>        Exp UnaryExp PirmaryExp MulExp AddExp RelExp EqExp LAndExp LOrExp InitVal

%left ADD SUB
%left MUL DIV

%%
// start
CompUnit    :   CompUnit CompUnit_          { $$ = $1; $$->child->push_back($2); root = $$;}
            |   CompUnit_                   { auto *tmp = new std::vector<TreeStmt*>();
                                                tmp->push_back($1);
                                              $$ = new CompUnit(tmp);
                                                root = $$;
                                            }
            ;

CompUnit_   :   Decl                        { $$ = $1;}
            |   FuncDef                     { $$ = $1;}
            ;


// Function type
/* FuncType    :   INT                         { fmt::print("[***]Func Int\n");$$ = new TreeType(0); }
            |   VOID                        { $$ = new TreeType(0); }
            ; */

// Indet typr
/* Btype       :   INT                         { fmt::print("BType Int\n");$$ = new TreeType(1); }
            ; */

// identfy  
Ident       :   IDENT                       { $$ = $1;}
            ;

// const number
Number      :   NUM                         { $$ = $1; }
            ;

// $$ = stmt     Decl             ::= VarDecl;
Decl        :   VarDecl                     { $$ = $1;}      
            ;

// $$ = varDecl    VarDecl       ::= BType VarDef {"," VarDef} ";";
VarDecl     :   INT VarDef VarDef_ SEMI   {     auto *tmp = new std::vector<TreeVarDef*>();
                                                tmp->push_back($2);
                                                $$ = new TreeVarDecl(new TreeType(1), tmp);
                                                for (auto& def : *$3) $$->varDef->push_back(def);
                                            }
            ;

VarDef_     :                               {$$ = new std::vector<TreeVarDef*>();}
            |   VarDef_ COMMA VarDef        {$1->push_back($3); $$ = $1;}
            ;

// VarDef        ::= IDENT "=" InitVal
//                 | IDENT {"[" INT_CONST "]"};
VarDef      :   Ident ASSIGNOP InitVal      {$$ = new TreeVarDef($1, false, nullptr, $3);}
            |   Ident ArryList              {$$ = new TreeVarDef($1, true, $2, nullptr);}
            ;

// ArryLisy     ::= {'[' INT_CONST ']'}
// such as a[10][20][30] ArryList use vector
ArryList    :                               {$$ = new std::vector<TreeIntegerLiteral*>();}
            |   ArryList LB Number RB       {$1->push_back($3); $$ = $1;}
            ;

InitVal     :   Exp                         {$$ = $1;}
            ;   



// func def
FuncDef     :   INT Ident LP FuncParams RP Block
                                            { $$ = new TreeFuncDef(new TreeType(1), $2, $4, $6);}
            |   INT Ident LP RP Block       { $$ = new TreeFuncDef(new TreeType(1), $2, nullptr, $5);}
            |   VOID Ident LP FuncParams RP Block
                                            { $$ = new TreeFuncDef(new TreeType(0), $2, $4, $6);}
            |   VOID Ident LP RP Block      { $$ = new TreeFuncDef(new TreeType(0), $2, nullptr, $5);} 
            ;


FuncParams  :   FuncParam FuncParam_        { auto *tmp = new std::vector<TreeFuncParam*> ();
                                              tmp->push_back($1);
                                              $$ = new TreeFuncParams(tmp);
                                              for (auto& param : *$2) $$->child->push_back(param);
                                            }
            ;

FuncParam   :   INT Ident                 { $$ = new TreeFuncParam(new TreeType(1), $2, false, nullptr);}
            |   INT Ident LB RB ArryList  { $$ = new TreeFuncParam(new TreeType(1), $2, true, $5);}
            ;

FuncParam_  :                               { $$ = new std::vector<TreeFuncParam*>();}
            |   FuncParam_ COMMA FuncParam  { $$ = $1; $$->push_back($3);}
            ;

// $$ = TreeStmt*;
Block       :   LC BlockItems RC            {$$ = new TreeBlock($2);}
            ;

// $$ = vector<TreeStmt*>;
BlockItems  :                               {$$ = new std::vector<TreeStmt*>();}
            |   BlockItems BlockItem        {$$ = $1; $1->push_back($2);}
            ;

// $$ = TreeStmt*;
BlockItem   :   Decl                        {$$ = $1;}
            |   Stmt                        {$$ = $1;}
            ;

// $$ = TreeStmt*;
Stmt        :   LVal ASSIGNOP Exp SEMI      {$$ = new TreeLvalEqStmt($1, $3);}
            |   Exp SEMI                    {$$ = $1;}
            |   Block                       {$$ = $1;}
            |   IFStmt                      {$$ = $1;}
            |   WHILE LP Exp RP Stmt        {$$ = new TreeWhileStmt($3, $5);}
            |   BREAK SEMI                  {$$ = new TreeBreakStmt();}
            |   CONTINUE SEMI               {$$ = new TreeContinueStmt();}
            |   RETURN  Exp SEMI            {$$ = new TreeReturnStmt($2);}
            |   RETURN SEMI                 {$$ = new TreeReturnStmt(nullptr);}
            ;

// $$ = TreeStmt*;
IFStmt      :   IF LP Exp RP Stmt           {$$ = new TreeIfStmt($3, $5, nullptr, false);}
            |   IF LP Exp RP Stmt ELSE Stmt {$$ = new TreeIfStmt($3, $5, $7, true);}
            ;

// OK
Exp         :   LOrExp                      {$$ = $1;}
            ;

// OK
LOrExp      :   LAndExp                     {$$ = $1;}
            |   LOrExp OR LAndExp           {$$ = new TreeBinaryExpr(OpType::OP_Lor, $1, $3);}
            ;

// $$ = TreeExpr*;          OK
LAndExp     :   EqExp                       {$$ = $1;}
            |   LAndExp AND EqExp           {$$ = new TreeBinaryExpr(OpType::OP_Land, $1, $3);}
            ;

// $$ = TreeExpr*;          OK
EqExp       :   RelExp                      {$$ = $1;}
            |   EqExp EQ RelExp             {$$ = new TreeBinaryExpr(OpType::OP_Eq, $1, $3);}
            |   EqExp NE RelExp             {$$ = new TreeBinaryExpr(OpType::OP_Ne, $1, $3);}
            ;

// $$ = TreeExpr*;          OK
RelExp      :   AddExp                      {$$ = $1;}
            |   RelExp LT AddExp            {$$ = new TreeBinaryExpr(OpType::OP_Lt, $1, $3);}
            |   RelExp LE AddExp            {$$ = new TreeBinaryExpr(OpType::OP_Le, $1, $3);}
            |   RelExp GT AddExp            {$$ = new TreeBinaryExpr(OpType::OP_Gt, $1, $3);}
            |   RelExp GE AddExp            {$$ = new TreeBinaryExpr(OpType::OP_Ge, $1, $3);}
            ;

// $$ = TreeExpr*;          OK
AddExp      :   MulExp                      {$$ = $1;}
            |   AddExp ADD MulExp           {$$ = new TreeBinaryExpr(OpType::OP_Add, $1, $3);}
            |   AddExp SUB MulExp           {$$ = new TreeBinaryExpr(OpType::OP_Sub, $1, $3);}
            ;

// $$ = TreeExpr*;          OK
MulExp      :   UnaryExp                    {$$ = $1;}
            |   MulExp MUL UnaryExp         {$$ = new TreeBinaryExpr(OpType::OP_Mul, $1, $3);}
            |   MulExp DIV UnaryExp         {$$ = new TreeBinaryExpr(OpType::OP_Div, $1, $3);}
            |   MulExp MOD UnaryExp         {$$ = new TreeBinaryExpr(OpType::OP_Mod, $1, $3);}
            ;

// $$ = TreeExpr*;          OK
UnaryExp    :   PirmaryExp                  {$$ = $1;}
            |   Ident LP FuncRParams RP     {$$ = new TreeUnaryExpr(OpType::OP_Func, $3, $1);}
            |   Ident LP RP                 {$$ = new TreeUnaryExpr(OpType::OP_Func, nullptr, $1);}
            |   ADD UnaryExp                {$$ = new TreeUnaryExpr(OpType::OP_Pos, $2);}
            |   SUB UnaryExp                {$$ = new TreeUnaryExpr(OpType::OP_Neg, $2);}
            |   NOT UnaryExp                {$$ = new TreeUnaryExpr(OpType::OP_Lnot, $2);}
            ;

// (Exp)    LVal    Number
// $$ = TreeExpr*;          OK
PirmaryExp  :   LP Exp RP                   {$$ = $2;}
            |   LVal                        {$$ = $1;}
            |   Number                      {$$ = $1;}
            ;

// LVal          ::= IDENT {"[" Exp "]"};
// $$ = TreeLVal*           OK
LVal        :   Ident LValList              {$$ = new TreeLVal($1, $2);}
            ;

// $$ = vector<TreeExpr*>*; OK
LValList    :                               {$$ = new std::vector<TreeExpr*>();}
            |   LValList LB Exp RB          {$$ = $1; $$->push_back($3);}
            ;

// $$ = TreeFuncRParams;
FuncRParams :   Exp FuncRP_                 {auto *tmp = new std::vector<TreeExpr*>();
                                                tmp->push_back($1);
                                             $$ = new TreeFuncRParams(tmp);
                                             for (auto& t : *$2) $$->child->push_back(t);   
                                            }
            ;

// $$ = vector<TreeExpr*>*; OK
FuncRP_     :                               {$$ = new std::vector<TreeExpr*>();}
            |   FuncRP_ COMMA Exp           {$$ = $1; $$->push_back($3);}
            ;


%%

void yyerror(const char *s) {
    printf("error: %s\n", s);
}

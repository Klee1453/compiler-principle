#include "ast/ast.h"
#include "ast/symbolTable.h"
#include <fmt/core.h>
#include <iostream>
#include <cassert>

extern int isInLoop;
extern std::vector<std::map<std::string, IdentTypeNode>> IDSymbolTable;
extern std::vector<std::map<std::string, FuncTypeNode>> FuncSymbolTable;

static std::string op_str(OpType op_type) {
    switch (op_type) {
#define OpcodeDefine(x, s)     \
        case x: return s;
#include "common/common.def"
    default:
        return "<unknown>";
    }
}

/**
 * do the type check during print the ast tree
*/

void print_tree(Node* node, std::string prefix, bool isfunctiondef) {
    assert(node != nullptr);
    fmt::print(prefix);

    // new domain
    if (auto *comp = node->as<CompUnit*>()) {
        newDomain();
        addRunTimeFunc();
        fmt::print("CompUnit\n");
        for (unsigned int i = 0; i < comp->child->size(); ++i) {
            print_tree(comp->child->at(i), prefix+"    ");
        }
        deleteDomain();
    }


    if (auto *type = node->as<TreeType*>()) {
        switch (type->type)
        {
        case 0:
            fmt::print("Type VOID\n");
            break;
        case 1:
            fmt::print("Type INT\n");
        default:
            break;
        }
    }

    // identy name
    if (auto *id = node->as<TreeIdent*>()) {
        fmt::print("Ident {}\n", id->IdentName);
    }

    // 赋值
    // left value assigment
    if (auto *les = node->as<TreeLvalEqStmt*>()) {
        fmt::print("BinOp Assign\n");
        check(les); // 类型检查
        print_tree(les->lval, prefix + "    ");
        print_tree(les->exp, prefix + "    ");
    }

    // return 
    if (auto *ret = node->as<TreeReturnStmt*>()) {
        if(!check(ret)) throw("Return error\n");
        if (ret->expr != nullptr) {
            fmt::print("Return\n");
            print_tree(ret->expr, prefix + "    ");
        }
        else {
            fmt::print("Return Nothing\n");
        }
    }

    // if
    if (auto *ifel = node->as<TreeIfStmt*>()) {
        fmt::print("If\n");
        check(ifel);
        fmt::print(prefix);
        fmt::print("IF Condition\n");
        print_tree(ifel->exp, prefix + "    ");
        fmt::print(prefix);
        fmt::print("If Block\n");
        print_tree(ifel->stmtIf, prefix+"    ");
        if (ifel->hasElse) {
            fmt::print(prefix);
            fmt::print("Else Block\n");
            print_tree(ifel->stmtElse, prefix + "    ");
        }
    }

    // while
    if (auto *whil = node->as<TreeWhileStmt*>()) {
        fmt::print("While\n");
        check(whil);
        fmt::print(prefix);
        fmt::print("While Condition Express\n");
        print_tree(whil->exp, prefix + "    ");
        isInLoop++;
        fmt::print(prefix);
        fmt::print("While Body\n");
        print_tree(whil->stmt, prefix + "    ");
        isInLoop--;
    }

    // break
    if ([[maybe_unused]]auto *bre = node->as<TreeBreakStmt*>()) {
        if (!isInLoop) {
            throw("Not in loop but break\n");
        }
        fmt::print("Break\n");
    }

    // continue
    if ([[maybe_unused]]auto *conti = node->as<TreeContinueStmt*>()) {
        if (!isInLoop) {
            throw("Not in loop but continue\n");
        }
        fmt::print("Continue\n");
    }

    // block
    // 当遇到大括号节点
    if (auto *block = node->as<TreeBlock*>()) {
        fmt::print("Block\n");
        newDomain();    // 向栈中压入一个新的作用域
        if (isfunctiondef) {
            // 处理函数定义时，函数参数也需要被视为局部变量加入到函数体的作用域中
            // copy the fparams' domain to the func's block's domain
            int n = IDSymbolTable.size();
            IDSymbolTable[n-1] = IDSymbolTable[n-2];
        }
        // 递归打印块内的每个语句
        for (unsigned int i = 0; i < block->child->size(); ++i) {
            print_tree(block->child->at(i), prefix + "    ");
        }
        // 离开大括号块时，弹出当前作用域
        deleteDomain();
    }

    // 新变量声明
    // declear var
    if (auto *varDecl = node->as<TreeVarDecl*>()) {
        addToIDSymbolTable(varDecl);    // 检查类型，加入符号表
        std::string type = varDecl->type->type == 0 ? "VOID" : "INT"; 
        fmt::print("Var Declear {}\n", type);
        for (unsigned int i = 0; i < varDecl->varDef->size(); ++i) {
            print_tree(varDecl->varDef->at(i), prefix + "    ");
        }
    }

    // define a var
    // do type check in varDecl
    if (auto *varDef = node->as<TreeVarDef*>()) {
        if (varDef -> isArry) {
            fmt::print("Var Define {}", varDef->ident->IdentName);
            for (unsigned int i = 0; i < varDef->child->size(); ++i) {
                fmt::print("[{}]", varDef->child->at(i)->value);
            }
            fmt::print("\n");
        }
        else {
            fmt::print("Var Define And Init {}\n",varDef->ident->IdentName);
            print_tree(varDef->initVal, prefix + "    ");
        }
    }

    // define function
    if (auto *funcDef = node->as<TreeFuncDef*>()) {
        
        nowFunc = addToFuncSymbolTable(funcDef);
        newDomain();
        addFuncParamsToTable(funcDef);
        std::string type = funcDef->type->type == 0 ? "VOID" : "INT";
        fmt::print("Func Def {} {}\n", type, funcDef->ident->IdentName);
        // params
        if (funcDef->params != nullptr) {
            fmt::print(prefix);
            fmt::print("Func Params\n");
            print_tree(funcDef->params, prefix + "    ");
        }
        // block
        print_tree(funcDef->block, prefix + "    ", true);
        deleteDomain();
        nowFunc = nullptr;
    }

    // do type checking at FuncDef
    if (auto *funcPs = node->as<TreeFuncParams*>()) {
        for (unsigned int i = 0; i < funcPs->child->size();++i) {
            if (i != 0) fmt::print(prefix);
            fmt::print("Func Param {}\n", i);
            print_tree(funcPs->child->at(i), prefix+"    ");
        }
    }

    // 
    if (auto *funcP = node->as<TreeFuncParam*>()) {
        std::string type = funcP->type->type == 0 ? "VOID" : "INT";
        fmt::print("Func Param {} {}",type, funcP->ident->IdentName);
        if (funcP->isArry) {
            fmt::print("[]");
            for (unsigned int i = 0; i < funcP->child->size(); ++i) {
                fmt::print("[{}]", funcP->child->at(i)->value);
            }
        }
        fmt::print("\n");
    }

    if (auto *funcRP = node->as<TreeFuncRParams*>()) {
        for (unsigned int i = 0; i < funcRP->child->size(); ++i) {
            if (i != 0) fmt::print(prefix);
            fmt::print("RealParam{}\n",i);
            print_tree(funcRP->child->at(i), prefix + "    ");
        }
    }

    // left value
    if (auto *lval = node->as<TreeLVal *>()) {
        check_type(lval);
        // array
        if (lval->exprs != nullptr && lval->exprs->size() >= 1) {
            // first line cout name
            fmt::print("LeftArrayValue {}\n", lval->ident->IdentName); 
            // then cout the exprs
            for (unsigned int i = 0; i < lval->exprs->size(); ++i) {
                print_tree(lval->exprs->at(i), prefix + "    ");
            }
        }
        // ident
        else {
            fmt::print("LeftValue {}\n", lval->ident->IdentName);
        }
    }

    // 
    if (auto *bin_op = node->as<TreeBinaryExpr *>()) {           // could change into Binary
        check_type(bin_op);
        fmt::print("BinOp \"{}\"\n", op_str(bin_op->op));
        print_tree(bin_op->lhs, prefix + "    ");
        print_tree(bin_op->rhs, prefix + "    ");
    }

    if (auto *un_op = node->as<TreeUnaryExpr *>()) {             // could change into Unary
        check_type(un_op);
        if (un_op->id == nullptr) { // Unary Expression
            fmt::print("UnOp \"{}\"\n", op_str(un_op->op));
            print_tree(un_op->operand, prefix + "    ");
        }
        else {                      // function call
            fmt::print("Funcion Call {}\n", un_op->id->IdentName);
            if (un_op->operand != nullptr)
                print_tree(un_op->operand, prefix + "    ");
        }
    }

    if (auto* lit = node->as < TreeIntegerLiteral *>()) {        // could achange into int
        fmt::print("Int {}\n", lit->value);
    }
    
}
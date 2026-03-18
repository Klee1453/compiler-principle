#pragma once
#include <vector>
#include <map>
#include <string>
#include <ast/ast.h>
#include <iostream>

enum DeType {
    ERROR,
    INT,
    VOID,
    FUNC_INT,
    FUNC_VOID,
    POINTER,
    ARRAY_INT
};

struct IdentTypeNode {
    DeType type;
    // for array
    int dimension;
    std::vector<int> domainSize {};
    // IdentTypeNode::IdentTypeNode() : type(DeType::ERROR), dimension(0) {};
    // IdentTypeNode::IdentTypeNode(DeType t, int d) : type(t), dimension(d) {};
};

struct FuncTypeNode {
    DeType type;
    // for funcDef
    std::vector<IdentTypeNode> funcParams {};

    // FuncTypeNode::FuncTypeNode(DeType t) : type(t) {
    //     funcParams.clear();
    // };
};

// ===================================================================================
// 【命令式符号表】
// 命令式风格意味着我们只维护一份当前处于激活状态的全体环境，并通过显式的 push / pop 去修改其内部状态。
// 当编译器走到哪一层大括号 {}, 它就处于那个环境。
// `IDSymbolTable` 就是一个模拟环境嵌套的栈结构（外层作用域在栈底，内层在栈顶）。
// 每进入一个更深的作用域时，命令式系统就会向栈中 push 一个新的 Map 来表示这个新作用域中的变量定义；
// 遇到新的变量定义时，总是向栈顶的 Map 中添加，并在此时检查重复定义；
// 查找变量时，从栈顶(最内层作用域)往栈底(全局作用域)查找，这完美模拟了局部变量掩盖全局变量的特性。
// 离开 `{}` 时，只需要把栈顶对应的 Map 从栈中 pop 出去，就自动清理了这层产生的所有临时变量。
// ===================================================================================

int isInLoop = false;
FuncTypeNode* nowFunc = nullptr;
std::vector<std::map<std::string, IdentTypeNode>> IDSymbolTable {};
std::vector<std::map<std::string, FuncTypeNode>> FuncSymbolTable {};

// pre declear
DeType check_type(TreeExpr* expr);
FuncTypeNode* checkFuncDefine(TreeIdent* ident);
IdentTypeNode* checkIdDefine(TreeIdent* ident);

/**
 * 检查赋值语句（如 a = b + 1;）语义是否合法
 * 等号左侧变量的类型 必须等于 右侧表达式推导出的类型，且要求两者都为标准的 INT
*/
bool check (TreeLvalEqStmt* leq) {
    DeType left = check_type(leq->lval);
    DeType right = check_type(leq->exp);
    if (left == right && right == DeType::INT) {
        return true;
    }
    else {
        throw("The lval assignment is fail\n");
    }
    return false;
}

/**
 * 检查 return 语句的语义是否合法
 * 判断的核心：
 * 1. 结合全局变量 `nowFunc` (由外层函数遍历控制) 确保我们确实在某个函数体内。
 * 2. 如果所在函数的类型是 VOID，绝对不允许其 return 中附带有任何表达式。
 * 3. 如果所在函数的类型是 INT，绝对不允许空 return，而且检查返回的表达式类型也必须推导为 INT。
*/
bool check (TreeReturnStmt* ret) {
    // check the return type is weather equal to now function
    if (nowFunc == nullptr) {
        throw("Now has not in a function but has a return statement\n");
    }
    else if (nowFunc->type == DeType::FUNC_VOID && ret->expr != nullptr) {
        throw("Function should return nothing(void), but actually return something\n");
    }
    else if (nowFunc->type == DeType::FUNC_INT && ret->expr == nullptr) {
        throw("Function should return INT but actually return nothing\n");
    }
    else if (nowFunc->type == DeType::FUNC_INT && check_type(ret->expr) == DeType::INT) {
        // its OK
        return true;
    }
    else if (nowFunc->type == DeType::FUNC_VOID && ret->expr == nullptr) {
        // is OK
        return true;
    }
    else {
        throw("Return error type\n");
    }
    return false;

}

/**
 * 检查 if 语句的条件表达式语义
 * 判断的核心：if (条件) 中的 “条件” 不能缺失，并且在 SysY / C 语言规范中，
 * 这个分支控制条件的推导结果通常应当为 INT 类型（0 为 false，非 0 为 true）。
*/
bool check (TreeIfStmt* iff) {
    // check if condition
    if (iff->exp == nullptr) {
        throw("If need confition express\n");
    }
    if (check_type(iff->exp) != DeType::INT) {
        throw("Invalid for the condition express for if\n");
    }
    return true;
}

/**
 * 检查 while 语句的条件表达式语义
*/
bool check (TreeWhileStmt* wh) {
    if (check_type(wh->exp) != DeType::INT) {
        throw("Invalid for the condition express for while\n");
    }
    return true;
}

/**
 * 递归检查 / 推导表达式具体类型的核心函数
 * 对所有的子表达式计算类型 (DeType)
 *
 * @param expr The expression to find the type
 * @return The type of expression
*/
DeType check_type (TreeExpr* expr) {
    // Lval ，例如某个变量 a，或者是数组的元素 a[1]
    if (auto *lval = expr->as<TreeLVal*>()) {
        // 检查 LVal 的标识符是否存在于符号表中
        IdentTypeNode *tmp = checkIdDefine(lval->ident);
        if (tmp == nullptr) {
            throw("Use undefine Ident\n");
        }
        else if ((tmp->dimension == 0) && (lval->hasExpress)) {

            throw("The Ident can't be used as array\n");
        }
        
        // 数组
        if (tmp->type == DeType::ARRAY_INT) {
            // 数组调用的中括号维数不能多于原定义的维数
            if (lval->exprs->size() > (unsigned int)tmp->dimension) {
                throw("Array has too many params\n");
            }
            
            // 数组访问的 index 参数必须可以推导为 INT
            for (unsigned int i = 0; i < lval->exprs->size(); ++i) {
                DeType child = check_type(lval->exprs->at(i));
                if (child == DeType::VOID) {
                    throw("The index of array can't be void\n");
                }
                else if (child != DeType::INT) {
                    throw("The index of array is not INT\n");
                }
            }
            
            // 如果所填的中括号数等于数组原本维数，说明它精确访问到了一个具体的整型值，返回 INT
            if (lval->exprs->size() == (unsigned int)tmp->dimension) {
                return DeType::INT;
            }
            // 否则这就是一个未完全降解的数组头指针调用，返回其截断后的数组类型标记
            else {
                return (DeType)((DeType::ARRAY_INT) + (tmp->dimension - lval->exprs->size()));
            }
        }
        // 普通 INT 变量
        else if (tmp->type == DeType::INT) {
            return DeType::INT;
        }
    }
    // const number显然是 INT
    else if ([[maybe_unused]]auto *inte = expr->as<TreeIntegerLiteral*>()) {
        return DeType::INT;
    }
    // Unary 一元表达式
    else if (auto *una = expr->as<TreeUnaryExpr*>()) {
        // maybe (-a)  or (foo(a,b))
        // func call -> 复杂的函数调用类型匹配
        if (una->op == OpType::OP_Func) {
            // 首先查函数表，看函数在全局作用域有没有定义过
            FuncTypeNode* functmp = checkFuncDefine(una->id);
            if (functmp == nullptr) {
                throw("The function used is not defined\n");
            }
            
            // 比对调用时传入的参数个数类型与函数声明时是否严格一致
            if (una->operand != nullptr) {
                auto* rParams = una->operand->as<TreeFuncRParams*>();
                int rnum = rParams->child->size();
                int num = functmp->funcParams.size();
                if (rnum > num) {
                    throw("Function call with too many params\n");
                }
                else if (rnum < num) {
                    throw("Function call with less params\n");
                }
                for (int i = 0; i < num; ++i) {
                    // i means the ith child of funccall
                    DeType rtype = check_type(rParams->child->at(i));
                    if (rtype != (DeType)(functmp->funcParams[i].type +functmp->funcParams[i].dimension)) {
                        // throw("Function call with error real param at " + std::to_string(i) + "\n");
                        throw("Function call with error real param\n");
                    }
                    else if (rtype > DeType::ARRAY_INT) {
                        // check the domain size is weather same
                        IdentTypeNode* iden = checkIdDefine(rParams->child->at(i)->as<TreeLVal*>()->ident);
                        int rdomainNum = (int)(rtype - DeType::ARRAY_INT) - 1;
                        int rn = iden->domainSize.size() - 1, ln = functmp->funcParams[i].domainSize.size() - 1;
                        for (int ii = 0; ii < rdomainNum; ++ii) {
                            if (iden->domainSize[rn--] != functmp->funcParams[i].domainSize[ln--]) {
                                throw("Function call with invalid domain size\n");
                            }
                        }
                    }
                }
            }
            else {
                // Func call with no param
                if (functmp->funcParams.size() != 0) {
                    throw("Funcion call with less params\n");
                }
            }
            // then return the type of func
            if (functmp->type == DeType::FUNC_INT) {
                return DeType::INT;
            }
            else {
                return DeType::VOID;
            }

        }
        // just number
        else {
            return check_type(una->operand);
        }

    }
    // Binary
    else if (auto *bina = expr->as<TreeBinaryExpr*>()) {
        DeType left, right;
        left = check_type(bina->lhs);
        right = check_type(bina->rhs);
        if (left == right && left != DeType::VOID) {
            return left;
        }
        else {
            throw("Binary expression with invalid type of left or right\n");
        }
    }
    return DeType::ERROR;
};

/**
 * find the ident of id
*/
IdentTypeNode* checkIdDefine(TreeIdent* ident) {
    // 逆序遍历符号表栈
    for (auto it = IDSymbolTable.rbegin(); it != IDSymbolTable.rend(); ++it) {
        auto& scopeMap = *it; 
        auto found = scopeMap.find(ident->IdentName);
        if (found != scopeMap.end()) {
            return &(found->second);
        }
    }
    return nullptr;
}

/**
 * 
*/
FuncTypeNode* checkFuncDefine(TreeIdent* ident) {
    // 根据 SysY 语言规范（也如文档提及），函数只能在全局作用域定义。
    // 因此这里只需要检查第一层作用域（下标为 0 的全局映射表）。
    
    if (FuncSymbolTable.empty()) return nullptr;
    
    auto& globalScope = FuncSymbolTable[0];
    auto found = globalScope.find(ident->IdentName);
    
    if (found != globalScope.end()) {
        return &(found->second);
    }
    
    return nullptr;
}

/**
 * Insert into SymbolTable functions
 * Insert into func and ID
*/

/**
 * @param node The node that include series of var def
 * @return is success insert
 * check first, and then insert one, check one
 * TODO
*/
bool addToIDSymbolTable (TreeVarDecl* node) {
    if (IDSymbolTable.empty()) {
        throw("There is no domain to add\n"); // 确保至少有一个作用域存在
    }
    
    // 始终在当前（栈顶）作用域中添加变量
    auto& currentScope = IDSymbolTable.back(); 

    for (unsigned int i = 0; i < node->varDef->size(); ++i) {
        TreeVarDef* var = node->varDef->at(i);
        
        // 检查该变量在当前作用域中是否已存在（屏蔽了对外层同名变量的重复定义检查，因为局部变量允许覆盖外层变量）
        if (currentScope.find(var->ident->IdentName) != currentScope.end()) {
            throw("This var has defined in this domain before\n");
        }
        // add it to table
        IdentTypeNode idNode;
        if (var->isArry) {
            // array's params are all int_const, cannot error
            idNode.type = DeType::ARRAY_INT;
            idNode.dimension = var->child->size();
            for (unsigned int ii = 0; ii < var->child->size(); ++ii) {
                idNode.domainSize.push_back((int)var->child->at(ii)->value);
            }
        }
        else {
            // check the expr's type first
            DeType rightExpr = check_type(var->initVal);
            if (rightExpr != DeType::INT) {
                throw("Error right value when define a lval\n");
            }
            idNode.type = DeType::INT;
            idNode.dimension = 0;
        }
        currentScope[var->ident->IdentName] = idNode;
    }
    return true;
};

/**
 * @param node The node that include series of var def
 * @return is success insert
 * check first and insert then
*/
FuncTypeNode* addToFuncSymbolTable (TreeFuncDef* node) {
    // 根据文档，函数只能定义在全局作用域 (栈大小应为 1，或直接压入 0 号索引)
    if (FuncSymbolTable.empty()) {
        throw("There is no domain to add\n");
    }
    // 假设 IDSymbolTable.size() 大于 1 表示当前在一个花括号块内
    if (IDSymbolTable.size() >= 2) {
        throw("Function can only define in the global domain\n");
    }

    auto& globalFuncScope = FuncSymbolTable[0];

    // check
    if (globalFuncScope.find(node->ident->IdentName) != globalFuncScope.end()) {
        throw("This function has been defined before\n");
    }

    // add
    unsigned int type = node->type->type;
    FuncTypeNode* funcnode = new FuncTypeNode;
    funcnode->type = (type) ? DeType::FUNC_INT : DeType::FUNC_VOID;
    // add params  the params also need to add to the new domain
    TreeFuncParams* params = node->params;
    if (params != nullptr) {
        for (unsigned int i = 0; i < params->child->size(); ++i) {
            IdentTypeNode idnode;
            TreeFuncParam* param = params->child->at(i);
            if (param->isArry) {
                idnode.type = DeType::ARRAY_INT;
                idnode.dimension = param->child->size() + 1;
                idnode.domainSize.push_back(0);
                for (unsigned int ii = 0; ii < param->child->size(); ++ii) {
                    idnode.domainSize.push_back((int)param->child->at(ii)->value);
                }
            }
            else {
                idnode.type = DeType::INT;
                idnode.dimension = 0;
            }
            funcnode->funcParams.push_back(idnode);
        }
    }
    FuncSymbolTable[0][node->ident->IdentName] = *funcnode;
    return funcnode;
}

void addFuncParamsToTable(TreeFuncDef* node) {
    // the domain must be empty just insert
    int n = IDSymbolTable.size();
    TreeFuncParams* params = node->params;
    if (params == nullptr) {
        return;
    }
    for (unsigned int i = 0; i < params->child->size(); ++i) {
        TreeFuncParam* param = params->child->at(i);
        // check
        auto it = IDSymbolTable[n-1].find(param->ident->IdentName);
        if (it != IDSymbolTable[n-1].end()) {
            throw("Function define with same param\n");
        }
        else {
            IdentTypeNode nn;
            if (param->isArry) {
                nn.type = DeType::ARRAY_INT;
                nn.dimension = param->child->size() + 1;
                nn.domainSize.push_back(0);
                for (unsigned int ii = 0; ii < param->child->size(); ++ii) {
                    nn.domainSize.push_back((int)param->child->at(ii)->value);
                }
            }
            else {
                nn.type = DeType::INT;
                nn.dimension = 0;
            }
            IDSymbolTable[n-1][param->ident->IdentName] = nn;
        }
    }
}

/**
 * @return is successful add a new domain, which means into a new block {}
 * add empty map to vector
*/
void newDomain () {
    std::map<std::string, IdentTypeNode> idMap {};
    std::map<std::string, FuncTypeNode> funcMap {};
    IDSymbolTable.push_back(idMap);
    FuncSymbolTable.push_back(funcMap);
}

/**
 * Like stack, just 'pop' the top map
*/
void deleteDomain () {
    // IDSymbolTable.erase(IDSymbolTable.begin() + IDSymbolTable.size() - 1);
    // FuncSymbolTable.erase(FuncSymbolTable.begin() + FuncSymbolTable.size() - 1);
    if (!IDSymbolTable.empty()) IDSymbolTable.pop_back();
    if (!FuncSymbolTable.empty()) FuncSymbolTable.pop_back();
}

void addRunTimeFunc() {
    // getint getch getarray
    FuncTypeNode *getInt = new FuncTypeNode, *getCh = new FuncTypeNode, *getArray = new FuncTypeNode;
    getInt->type = DeType::FUNC_INT;
    getCh->type = DeType::FUNC_INT;
    getArray->type = DeType::FUNC_INT;
    getArray->funcParams.push_back({DeType::ARRAY_INT, 1});
    FuncSymbolTable[0]["getint"] = *getInt;
    FuncSymbolTable[0]["getch"] = *getCh;
    FuncSymbolTable[0]["getarray"] = *getArray;
    // putint putch putarray
    FuncTypeNode *putInt = new FuncTypeNode, *putCh = new FuncTypeNode, *putArray = new FuncTypeNode;
    putInt->type = DeType::FUNC_VOID;
    putInt->funcParams.push_back({DeType::INT, 0});
    putCh->type = DeType::FUNC_VOID;
    putCh->funcParams.push_back({DeType::INT, 0});
    putArray->type = DeType::FUNC_VOID;
    putArray->funcParams.push_back({DeType::INT, 0});
    putArray->funcParams.push_back({DeType::ARRAY_INT, 1});
    FuncSymbolTable[0]["putint"] = *putInt;
    FuncSymbolTable[0]["putch"] = *putCh;
    FuncSymbolTable[0]["putarray"] = *putArray;
    // starttime stoptime
    FuncTypeNode *startT = new FuncTypeNode, *stopT = new FuncTypeNode;
    startT->type = DeType::VOID;
    stopT->type = DeType::VOID;
    FuncSymbolTable[0]["starttime"] = *startT;
    FuncSymbolTable[0]["stoptime"] = *stopT;
}
#include "ast/ast2ir.h"
#include "ast/symbolTable.h"

#include <fmt/core.h>
#include <iostream>
#include <cassert>

/**
 * need to do
 * logical not and bits not
*/

int continueNo = 1;
int breakNo = 1;
int ifNo = 1;
int whileNo = 1;
int retNo = 1;
int shortAndNo = 1;
int shortOrNo = 1;
int shortBinNo = 1;
int shortLvalNo = 1;
int shortFuncRNo = 1;
int shortUopNo = 1;
int tmpNo = 1;


int isIRinLoop = 0;
BasicBlock* loopExit = nullptr;
BasicBlock* loopEntry = nullptr;
BasicBlock* funcEntry = nullptr;
bool isIRInFunc = false;
std::vector<TreeVarDef*> globalVarBinding {};
std::vector<std::pair<BasicBlock*, BasicBlock*>> allLoops {};

std::vector<std::map<std::string, Value*>> IDValueTable {};
std::vector<std::map<std::string, Function*>> FuncValueTable {};
std::map<std::string, int> IDnameTimes;
Module* CompMod = nullptr;

extern std::vector<std::map<std::string, IdentTypeNode>> IDSymbolTable;
extern std::vector<std::map<std::string, FuncTypeNode>> FuncSymbolTable;

// ------------------------------------------------------------------------
// 【1. 计算求值模块】翻译算术表达式、取值和逻辑（即“求右值”）
// 目的：高级语言的公式（比如 a + b * c）无法被硬件直接理解。
// 操作：深度优先地遍历树节点，生成一条条线性的执行指令（像排流水线一样）。
// 重点：指令的计算结果保存在由 `Value*` 代理的临时“虚拟寄存器”中，而不是物理内存。
//      遇到算数运算时，分别递归求出左右两子树的 Value*，最后通过 Create 运算指令连接。
// ------------------------------------------------------------------------
Value* translate_expr(TreeExpr* expr, BasicBlock* bb) {
    // 处理左值读取(如引用某个变量或数组的值)
    if (auto *lval = expr->as<TreeLVal*>()) {
        // var or array
        // array a[x1][x2][x3]
        lval->ident->IdentName = getTranstedName(lval->ident->IdentName);
        IdentTypeNode* ll = checkIdDefine(lval->ident);
        
        if ((ll->dimension != 0) && ( (lval->exprs == nullptr) || (lval->exprs != nullptr && lval->exprs->size() < ll->dimension) )) {
            // this way we need to return a pointer
            // return a offset, while the last params is 0
            std::vector<Value*> v;
            std::vector <std::optional<std::size_t>> BoundList;
            IdentTypeNode* IdType = checkIdDefine(lval->ident);
            for (int i = 0; i < IdType->dimension; ++i) {
                if (IdType->domainSize[i] == 0) {
                    if (i != 0) {
                        throw std::runtime_error("Func's Array Param's index(1,2,3,...) can't be 0 expect index(0)\n");
                    }
                    BoundList.push_back(std::nullopt);
                }
                else {
                    BoundList.push_back(IdType->domainSize[i]);
                }
            }
            // get values
            int exprsNum = 0;
            if (lval->exprs == nullptr) {
                exprsNum = 0;
            }
            else {
                exprsNum = lval->exprs->size();
            }
            for (int i = 0; i < IdType->dimension; ++i) {
                if (i < exprsNum) {
                    v.push_back(translate_expr(lval->exprs->at(i), bb));
                }
                else {
                    v.push_back(ConstantInt::Create(0));
                }
            }
            // ptr
            bool isG = false;
            Value* ptr = findValue(lval->ident->IdentName, isG);
            // offset
            Value* off = OffsetInst::Create(Type::getIntegerTy(), ptr, v, BoundList, bb);
            return off;
        }
        else if (lval->hasExpress) {
            // array and need to cal the index(values) and BoundList
            // use offset to cal address 
            // use load to get the value if it is jvbu
            // global var can use directly
            std::vector<Value*> v;
            std::vector <std::optional<std::size_t>> BoundList;
            // check IDSymbolTable to get the BoundList
            IdentTypeNode* IdType = checkIdDefine(lval->ident);
            for (int i = 0; i < IdType->dimension; ++i) {
                if (IdType->domainSize[i] == 0) {
                    if (i != 0) {
                        throw std::runtime_error("Func's Array Param's index(1,2,3,...) can't be 0 expect index(0)\n");
                    }
                    BoundList.push_back(std::nullopt);
                }
                else {
                    BoundList.push_back(IdType->domainSize[i]);
                }
            }
            // get the values
            for (unsigned int i = 0; i < lval->exprs->size(); ++i) {
                v.push_back(translate_expr(lval->exprs->at(i), bb));
            }
            // ptr
            bool isG = false;
            Value* ptr = findValue(lval->ident->IdentName, isG);
            // offset and load
            Value* offset = OffsetInst::Create(Type::getIntegerTy(), ptr, v, BoundList, bb);
            Value* load = LoadInst::Create(offset, bb);
            return load;
        }
        // just number(ID)
        else {
            // just number and load is ok
            bool isG = false;
            Value* ptr = findValue(lval->ident->IdentName, isG);
            Value* load = LoadInst::Create(ptr, bb);
            return load;
        }
    }
    // OK int
    else if (auto *cint = expr->as<TreeIntegerLiteral*>()) {
        Value *res = ConstantInt::Create(cint->value);
        return res;
    }
    // OK Binary
    else if (auto *bop = expr->as<TreeBinaryExpr*>()) {
        Value* lhs = translate_expr(bop->lhs, bb);
        Value* rhs = translate_expr(bop->rhs, bb);
        Type* ty = nullptr;
        if (check_type(bop->lhs) == DeType::INT) {
            ty = Type::getIntegerTy();
        }
        else {
            ty = Type::getUnitTy();
        }
        Value* res = nullptr;
        switch (bop->op)
        {
        case OpType::OP_Add:
            res = BinaryInst::CreateAdd(lhs, rhs, ty, bb);
            break;
        case OpType::OP_Sub:
            res = BinaryInst::CreateSub(lhs, rhs, ty, bb);
            break;
        case OpType::OP_Mul:
            res = BinaryInst::CreateMul(lhs, rhs, ty, bb);
            break;
        case OpType::OP_Div:
            res = BinaryInst::CreateDiv(lhs, rhs, ty, bb);
            break;
        case OpType::OP_Mod:
            res = BinaryInst::CreateMod(lhs, rhs, ty, bb);
            break;
        case OpType::OP_Land:
            res = BinaryInst::CreateAnd(lhs, rhs, ty, bb);
            break;
        case OpType::OP_Lor:
            res = BinaryInst::CreateOr(lhs, rhs, ty, bb);
            break;
        case OpType::OP_Lxor:
            res = BinaryInst::CreateXor(lhs, rhs, ty, bb);
            break;
        case OpType::OP_Lt:
            res = BinaryInst::CreateLt(lhs, rhs, ty, bb);
            break;
        case OpType::OP_Gt:
            res = BinaryInst::CreateGt(lhs, rhs, ty, bb);
            break;
        case OpType::OP_Le:
            res = BinaryInst::CreateLe(lhs, rhs, ty, bb);
            break;
        case OpType::OP_Ge:
            res = BinaryInst::CreateGe(lhs, rhs, ty, bb);
            break;
        case OpType::OP_Eq:
            res = BinaryInst::CreateEq(lhs, rhs, ty, bb);
            break;
        case OpType::OP_Ne:
            res = BinaryInst::CreateNe(lhs, rhs, ty, bb);
            break;
        default:
            break;
        }
        return res;
    }
    // OK Unary --> (-a) or (foo(a, b)) 
    else if (auto *uop = expr->as<TreeUnaryExpr*>()) {
        // function call
        if (uop->op == OpType::OP_Func) {
            Function* f = CompMod->getFunction(uop->id->IdentName);
            std::vector<Value*> Args {};
            if (uop->operand != nullptr) {
                auto* rparams = uop->operand->as<TreeFuncRParams*>();
                for (unsigned int i = 0; i < rparams->child->size(); ++i) {
                    Args.push_back(translate_expr(rparams->child->at(i), bb));
                }
            }
            Value* res = CallInst::Create(f, Args, bb);
            return res;
        }
        // just calculate
        else {
            Value* res = nullptr;
            if (uop->op == OpType::OP_Lnot) {
                // logical not can change to : eq src 0
                Value* z = ConstantInt::Create(0);
                Value* src = translate_expr(uop->operand, bb);
                res = BinaryInst::CreateEq(src, z, Type::getIntegerTy(), bb);
            }
            else if (uop->op == OpType::OP_Neg) {
                // -a
                Value* z = ConstantInt::Create(0);
                Value* src = translate_expr(uop->operand, bb);
                res = BinaryInst::CreateSub(z, src, Type::getIntegerTy(), bb);
            }
            else if (uop->op == OpType::OP_Lnot) {
                Value* c = ConstantInt::Create(0xffffffff);
                Value* src = translate_expr(uop->operand, bb);
                res = BinaryInst::CreateXor(c, src, Type::getIntegerTy(), bb);
            }
            return res;
        }
    }

    return nullptr;
}

// ------------------------------------------------------------------------
// 【2. 赋值模块】处理等号赋值（如 arr[i] = 10 或 a = b + 1）
// 目的：将算出的“结果数据”安放到真正“物理内存堆栈”的固定位置去。
// 操作：
// 1. 先计算获取左侧目标变量（LVal）的“内存首地址”（从符号表里查到）。
// 2. 递归右侧表达式（RExp），算出一个带有结果的 Value*。 
// 3. 构建一条 Store 指令，把右边的数值写入到左边的地址指针中，完成更新。
// ------------------------------------------------------------------------
void translate_expr(TreeLvalEqStmt* expr, BasicBlock* bb) {
    
    // Lval: global or jvbu
    // if global, just change and update table
    // if jvbu, get address + store + update table
    TreeLVal* lval = expr->lval->as<TreeLVal*>();
    TreeExpr* rexp = expr->exp;
    lval->ident->IdentName = getTranstedName(lval->ident->IdentName);
    Value* r = translate_expr(rexp, bb);
    bool isG = false;
    Value* ptr = findValue(lval->ident->IdentName, isG);
    // OK array
    if (lval->hasExpress) {
        // offset and load and store and update
        // boundlist
        std::vector<Value*> v;
        std::vector<std::optional<std::size_t>> BoundList;
        auto* IdType = checkIdDefine(lval->ident);
        for (int i = 0; i < IdType->dimension; ++i) {
            if (IdType->domainSize[i] == 0) {
                if (i != 0) {
                    throw std::runtime_error("Finc's Array Param's error\n");
                }
                BoundList.push_back(std::nullopt);
            }
            else {
                BoundList.push_back(IdType->domainSize[i]);
            }
        }
        // value*
        for (unsigned int i = 0; i < lval->exprs->size(); ++i) {
            v.push_back(translate_expr(lval->exprs->at(i), bb));
        }
        // store
        Value* offset = OffsetInst::Create(Type::getIntegerTy(), ptr, v, BoundList, bb);
        Value* store = StoreInst::Create(r, offset, bb);
    }
    // OK just like a = b + 1, and a store is enough
    else {
        // get ptr and store is OK
        Value* store = StoreInst::Create(r, ptr, bb);
    }

    return ;
};

// ------------------------------------------------------------------------
// 【3. 变量声明模块】在内存中为新诞生的变量分配空间并登记
// 核心机制（Alloca破窗法）：这是编译原理入门里处理复杂重新赋值最常用的捷径。
//  - 全局变量：直接抛到独立的数据分段中(GlobalVariable)。
//  - 局部变量：强制在它们所在函数的**第一行指令块(Entry Block)**统一用 Alloca 开辟一块堆栈空间！
//    这样以后哪怕代码里反复发生 `i = 1; i = 2;` 的覆写，
//    在 IR 的视角都只是对这块固定的地址做普通的 Store 操作，
//    完美避开了手动维死板的 SSA Phi节点（分支融合点）分配这件极其烧脑的事情！
// ------------------------------------------------------------------------
void translate_expr(TreeVarDecl* decl, BasicBlock* bb) {
    // if size == 1 for gloable var
    if (IDValueTable.size() == 1) {
        // gloable
        // 1. create GlobalVariable
        // 2. if is a num, store the def waiting for the binding
        // 3. update the 2 tables
        // 4. don't change name
        Type* ty = nullptr;
        if (decl->type->type == 1) {
            ty = Type::getIntegerTy();
        }
        for (unsigned int i = 0; i < decl->varDef->size(); ++i) {
            // 
            TreeVarDef* def = decl->varDef->at(i);   
            GlobalVariable *gvar = nullptr;
            IdentTypeNode idTypeNode;
            
            // array
            if (def->isArry) {
                int arraySize = 1;
                for (unsigned int j = 0; j < def->child->size(); ++j) {
                    arraySize *= def->child->at(j)->value;
                    idTypeNode.domainSize.push_back(def->child->at(j)->value);
                }
                gvar = GlobalVariable::Create(ty, arraySize, false, def->ident->IdentName, CompMod);
                // id type node
                idTypeNode.type = DeType::ARRAY_INT;
                idTypeNode.dimension = def->child->size();
            }
            else {
                idTypeNode.type = DeType::INT;
                idTypeNode.dimension = 0;
                gvar = GlobalVariable::Create(ty, 1, false, def->ident->IdentName, CompMod);
                globalVarBinding.push_back(def);
            }
            gvar->setName(def->ident->IdentName);
            // update the table
            IDValueTable[0][def->ident->IdentName] = gvar;
            IDSymbolTable[0][def->ident->IdentName] = idTypeNode;
        }
    }
    else {
        // (局部变量)
        // 采用 "Alloca法"：统一在当前所处函数的 Entry 块内申请栈空间
        // 然后在当前 BasicBlock 利用 Store 指令存入初始值
        Function* pp = bb->getParent();
        BasicBlock* fentry = &pp->getEntryBlock();
        Type* ty = nullptr;
        if (decl->type->type == 1) {
            ty = Type::getIntegerTy();
        }
        for (unsigned int i = 0; i < decl->varDef->size(); ++i) {
            IdentTypeNode idTypeNode;
            idTypeNode.type = DeType::INT, idTypeNode.dimension = 0;
            TreeVarDef* def = decl->varDef->at(i);
            // change the var's name in the tree
            def->ident->IdentName = transName(def->ident->IdentName);
            int arraySize = 1;
            if (def->isArry) {
                idTypeNode.type = DeType::ARRAY_INT;
                idTypeNode.dimension = def->child->size();
                for (unsigned int j = 0; j < def->child->size(); ++j) {
                    arraySize *= def->child->at(j)->value;
                    idTypeNode.domainSize.push_back(def->child->at(j)->value);
                }
                if (idTypeNode.dimension == 0) {
                    idTypeNode.type = DeType::INT;
                }
            }
            Value* talloc = AllocaInst::Create(ty, arraySize, fentry);
            talloc->setName(def->ident->IdentName);
            if (!def->isArry) {
                // if not array, we need to store the true value to it
                Value* trueValue = translate_expr(def->initVal, bb);
                Value* trueValueStore = StoreInst::Create(trueValue, talloc, bb);
            }
            // update table
            IDValueTable[IDValueTable.size() - 1][def->ident->IdentName] = talloc;
            IDSymbolTable[IDSymbolTable.size() - 1][def->ident->IdentName] = idTypeNode;
        }
    }
}

// ------------------------------------------------------------------------
// 【4. 函数定义模块】组装出一个函数的编译大框架
// 原理：单个函数可以被看做是一个孤立的作用域副本。
// 步骤：
// 1. 建立函数的签名边界：需要什么参数类型进来，需要吐出什么类型结（构建 FunctionType）。
// 2. 建立此函数的三大核心交通枢纽块：入口块(fentry)、执行主体块(fbody)、返回收口块(fret)。
// 3. 将函数的返回值空间、接客的参数统一在首个 BasicBlock 里用 Alloca 提现到栈。
// 4. 重置新循环（保证作用域独立），继续深入腹地翻译大括号 `{...}` 内的执行指令。
// ------------------------------------------------------------------------
Value* translate_expr(TreeFuncDef* fdef, BasicBlock* bb) {
    // function declear
    // static Function *Create(FunctionType *FTy, bool ExternalLinkage = false,
    //                     std::string_view Name = "", Module *M = nullptr);
    // construct the result
    Type* res = nullptr;
    std::vector<Type*> params {};
    if (fdef->type->type == 0) res = Type::getUnitTy();
    else res = Type::getIntegerTy();
    // construct the params
    if (fdef->params != nullptr) {
        auto* v = fdef->params->child;
        for (unsigned int i = 0; i < v->size(); ++i) {
            if (v->at(i)->isArry) {
                params.push_back(PointerType::get(Type::getIntegerTy()));
            }
            else {
                params.push_back(Type::getIntegerTy());
            }
        }
    }
    // construct the function type
    FunctionType* fty = FunctionType::get(res, params);
    // create funciton
    Function* fun = Function::Create(fty, false, fdef->ident->IdentName, CompMod);
    FuncValueTable[0][fdef->ident->IdentName] = fun;
    // create extry exit returnvalue
    BasicBlock* fentryBB = BasicBlock::Create(fun);
    fentryBB->setName("FuncEntry");
    BasicBlock* fbodyBB = BasicBlock::Create(fun);
    fbodyBB->setName("FuncBody");
    BasicBlock* fretBB = BasicBlock::Create(fun);
    fretBB->setName("FuncRet");
    // %ret and add to params table
    Value* fret = nullptr;
    if (fdef->type->type == 1) {
        fret = AllocaInst::Create(Type::getIntegerTy(), 1, fentryBB);
        fret->setName("ret");
    }
    else {
        // fret = AllocaInst::Create(Type::getUnitTy(), 1, fentryBB);
    }
    // fill retBB
    if (fdef->type->type == 0) {
        // ret nothing
        Value* resI = RetInst::Create(ConstantUnit::Create(), fretBB);
    }
    else {
        // get the return value from table use load
        Value* load = LoadInst::Create(fret, fretBB);
        Value* retI = RetInst::Create(load, fretBB);
    }

    // ret and jvbu need to add to new domain
    // translate the body of function
    newDomain();
    newValueDomain();
    isIRInFunc = 1;
    ifNo = 1, whileNo = 1, retNo = 1;

    addToNewDomain(fret, fdef, fun);
    BasicBlock* fBodyRet = translate_stmt(fdef->block, fbodyBB, true);

    isIRInFunc = 0;
    deleteValueDomain();
    deleteDomain();
    clearNameNumTable();
    // fentryBB jump to func's body
    Value* bodyRet2exit = JumpInst::Create(fretBB, fBodyRet);
    Value* j2body = JumpInst::Create(fbodyBB, fentryBB);
}

BasicBlock* translate_stmt(TreeStmt* stmt, BasicBlock* bb, bool fromFunc) {
    // ------------------------------------------------------------------------
    // 将高层级的结构化控制流（If, While等）打平到基本块(BasicBlock)
    //  并利用Branch指令和Jump指令将它们链接成控制流图(CFG)。
    // ------------------------------------------------------------------------

    // 处理 If 语句
    if (auto* If = stmt->as<TreeIfStmt*>()) { 
        BasicBlock *isT, *isF, *exit, *entry;

        if (If->hasElse) {
            int No = ifNo++;
            std::string bbname;
            
            entry = BasicBlock::Create(bb->getParent(), bb);
            bbname = "IF_" + std::to_string(No) + "_EntryBB";
            entry->setName(bbname);
            
            isT = BasicBlock::Create(bb->getParent(), bb);
            bbname = "IF_" + std::to_string(No) + "_TrueBB";
            isT->setName(bbname);

            isF = BasicBlock::Create(bb->getParent(), bb);
            bbname = "IF_" + std::to_string(No) + "_FalseBB";
            isF->setName(bbname);
            
            exit = BasicBlock::Create(bb->getParent(), bb);
            bbname = "IF_" + std::to_string(No) + "_ExitBB";
            exit->setName(bbname);

            Value* tmp = AllocaInst::Create(Type::getIntegerTy(), 1, &bb->getParent()->front());
            tmp->setName("tmp" + std::to_string(tmpNo++) + "_IfCondValue");
            
            // bool isG;
            // Value* tt = StoreInst::Create(ConstantInt::Create(114514), findValue("return", isG), bb);

            shortPathForIf(If, If->exp, bb, entry, tmp, No);

            // entry
            Value* load = LoadInst::Create(tmp, entry);
            Value* branch = BranchInst::Create(isT, isF, load, entry);

            // isT
            BasicBlock* isTRetBB = translate_stmt(If->stmtIf, isT);
            Value* isTjump = JumpInst::Create(exit, isTRetBB);

            // isF
            BasicBlock* isFRetBB = translate_stmt(If->stmtElse, isF);
            Value* isFjump = JumpInst::Create(exit, isFRetBB);

            return exit;
        }
        else {
            int No = ifNo++;
            std::string bbname;
            
            entry = BasicBlock::Create(bb->getParent(), bb);
            bbname = "IF_" + std::to_string(No) + "_EntryBB";
            entry->setName(bbname);
            
            isT = BasicBlock::Create(bb->getParent(), bb);
            bbname = "IF_" + std::to_string(No) + "_TrueBB";
            isT->setName(bbname);
            
            exit = BasicBlock::Create(bb->getParent(), bb);
            bbname = "IF_" + std::to_string(No) + "_ExitBB";
            exit->setName(bbname);

            Value* tmp = AllocaInst::Create(Type::getIntegerTy(), 1, &bb->getParent()->front());
            tmp->setName("tmp" + std::to_string(tmpNo++) + "_IfCondValue");

            shortPathForIf(If, If->exp, bb, entry, tmp, No);

            // entry
            Value* load = LoadInst::Create(tmp, entry);
            Value* branch = BranchInst::Create(isT, exit, load, entry);

            // isT
            BasicBlock* isTRetBB = translate_stmt(If->stmtIf, isT);
            Value* isTjump = JumpInst::Create(exit, isTRetBB);

            return exit;
        }
    }
    // 处理 while 语句
    // 翻译策略：创建一个条件判断块(entry)，一个循环体(body)以及一个循环出口(exit)。
    // 每次执行完 body 都要跳回 entry 进行判断；break跳出到exit，continue跳回到entry
    else if (auto* While = stmt->as<TreeWhileStmt*>()) {
        BasicBlock *entry, *body, *exit;
        std::string bbname;
        int No = whileNo++;
        entry = BasicBlock::Create(bb->getParent(), bb);
        bbname = "While_" + std::to_string(No) + "_EntryBB";
        entry->setName(bbname);
        body = BasicBlock::Create(bb->getParent(), bb);
        bbname = "While_" + std::to_string(No) + "_BodyBB";
        body->setName(bbname);
        exit = BasicBlock::Create(bb->getParent(), bb);
        bbname = "While_" + std::to_string(No) + "_ExitBB";
        exit->setName(bbname);
        Value* j2en = JumpInst::Create(entry, bb);
        Value* condValue = translate_expr(While->exp, entry);
        Value* branch = BranchInst::Create(body, exit, condValue, entry);

        isIRinLoop++;
        loopExit = exit;
        loopEntry = entry;
        allLoops.push_back({entry, exit});      // 将当前环境推入栈，方便内部 break / continue 跳转定位
        BasicBlock* bodyExit = translate_stmt(While->stmt, body);
        allLoops.pop_back();                    // 恢复上下文
        loopEntry = nullptr;
        loopExit = nullptr;
        isIRinLoop--;

        // 无论如何，主体执行块结尾必须连接回 entry 进行新一轮判定
        Value* j2exit = JumpInst::Create(entry, bodyExit);

        // 如果之后还有别的语句，均顺接到新创建的 exit 基本块，所以这里向外返回 exit
        return exit;
    }
    // OK break
    else if (auto* Break = stmt->as<TreeBreakStmt*>()) {
        // break out from the minmun loop, for and while
        if (!isIRinLoop) {
            throw std::runtime_error("Break but not in loop\n");
        }
        
        BasicBlock* afterBreak = BasicBlock::Create(bb->getParent(), bb);
        afterBreak->setName("After_Break_" + std::to_string(breakNo++));

        Value* jmp = JumpInst::Create(allLoops.back().second, bb);
        return afterBreak;
    }
    // OK continue
    else if (auto* Continue = stmt->as<TreeContinueStmt*>()) {
        if (!isIRinLoop) {
            throw std::runtime_error("Continue but not in loop\n");
        }
        
        Value* jmp = JumpInst::Create(allLoops.back().first, bb);

        BasicBlock* afterConti = BasicBlock::Create(bb->getParent(), bb);
        afterConti->setName("After_Continue_" + std::to_string(continueNo++));
        
        return afterConti;
    }
    // OK return
    else if (auto* Ret = stmt->as<TreeReturnStmt*>()) {
        BasicBlock* retBB = &bb->getParent()->back();
        bool isG = false;
        Value* ptr = findValue("return", isG);
        // return something
        if (Ret->expr) {
            Value* retValue = translate_expr(Ret->expr, bb);
            Value* storeRet = StoreInst::Create(retValue, ptr, bb);
            Value* j2RetBB = JumpInst::Create(retBB, bb);
            
        }
        // return nothing
        else {
            Value* j2RetBB = JumpInst::Create(retBB, bb);
        }
        // normally, ret should be a bb's last inst, return nullptr
        // but if there's some insts after retInst, the func should return bb
        BasicBlock* afterRet = BasicBlock::Create(bb->getParent(), bb);
        afterRet->setName("AfterRet_" + std::to_string(retNo++));
        return afterRet;
    }
    // OK FuncDef
    else if (auto* fdef = stmt->as<TreeFuncDef*>()) {
        translate_expr(fdef, bb);
        return bb;
    }
    // OK VarDecl
    else if (auto* vdecl = stmt->as<TreeVarDecl*>()) {
        translate_expr(vdecl, bb);
        return bb;
    }
    // OK LvalEqStmt
    else if (auto* leq = stmt->as<TreeLvalEqStmt*>()) {
        translate_expr(leq, bb);
        return bb;
    }
    // OK Block
    else if (auto* Block = stmt->as<TreeBlock*>()) {
        BasicBlock* now = bb;
        newValueDomain();
        newDomain();
        // if func
        if (fromFunc) {
            int n = IDValueTable.size();
            IDValueTable[n-1] = IDValueTable[n-2];
        }
        for (unsigned int i = 0; i < Block->child->size(); ++i) {
            TreeStmt* tmp = Block->child->at(i);
            // if 
            if (tmp == nullptr) {
            }
            now = translate_stmt(tmp, now);
            if (now == nullptr) {
            }
        }
        deleteValueDomain();
        deleteDomain();
        return now;
    }
    // OK else translate_expr don't need to new bb or else
    else {
        TreeExpr* expr = dynamic_cast<TreeExpr*>(stmt);
        if (expr) {
            translate_expr(expr, bb);
        }
        return bb;
    }

    return bb;
}

// return a new module
// call when into a CompUnit
Module* newModule() {
    CompMod = new Module();
    return CompMod;
}

// OK!!
Value* findValue(std::string ss, bool& isGlobal) {
    // 
    isGlobal = false;
    int size = IDValueTable.size() - 1;
    for (int i = size; i >= 0; --i) {
        auto it = IDValueTable[i].find(ss);
        if (it != IDValueTable[i].end()) {
            if (i == 0) {
                isGlobal = true;
            }
            return it->second;
        }
    }
    // not find
    return nullptr;
}

void newValueDomain () {
    std::map<std::string, Value*> tid;
    IDValueTable.push_back(tid);
    std::map<std::string, Function*> tfunc;
    FuncValueTable.push_back(tfunc);
}

void deleteValueDomain () {
    IDValueTable.pop_back();
    FuncValueTable.pop_back();
}

void clearSynbolTable () {
    IDSymbolTable.clear();
    FuncSymbolTable.clear();
}

void addRuntimeFuncValue () {
    Type* res;
    std::vector<Type*> params;
    FunctionType* fty;
    Function* f;
    // int getint()
    res = Type::getIntegerTy();
    params.clear();
    fty = FunctionType::get(res, params);
    f = Function::Create(fty, true, "getint", CompMod);
    FuncValueTable[0]["getint"] = f;
    // int getch()
    f = Function::Create(fty, true, "getch", CompMod);
    FuncValueTable[0]["getch"] = f;
    // int getarray(int a[])
    res = Type::getIntegerTy();
    params.clear();
    params.push_back(PointerType::get(Type::getIntegerTy()));
    fty = FunctionType::get(res, params);
    f = Function::Create(fty, true, "getarray", CompMod);
    FuncValueTable[0]["getarray"] = f;
    // void putint(int a)
    res = Type::getUnitTy();
    params.clear();
    params.push_back(Type::getIntegerTy());
    fty = FunctionType::get(res, params);
    f = Function::Create(fty, true, "putint", CompMod);
    FuncValueTable[0]["putint"] = f;
    // void putch(char ch)
    f = Function::Create(fty, true, "putch", CompMod);
    FuncValueTable[0]["putint"] = f;
    // void putarray(int, int [])
    params.push_back(PointerType::get(Type::getIntegerTy()));
    fty = FunctionType::get(res, params);
    f = Function::Create(fty, true, "putarray", CompMod);
    FuncValueTable[0]["putarray"] = f;
    // void starttime()
    res = Type::getUnitTy();
    params.clear();
    fty = FunctionType::get(res, params);
    f = Function::Create(fty, true, "starttime", CompMod);
    FuncValueTable[0]["starttime"] = f;
    // void stoptime()
    f = Function::Create(fty, true, "stoptime", CompMod);
    FuncValueTable[0]["stoptime"] = f;
}

void addToNewDomain (Value* ret, TreeFuncDef* fdef, Function* ff) {
    int index = IDValueTable.size() - 1;
    if (ret != nullptr) {
        IDValueTable[index]["return"] = ret;
    }
    // change fdef's name
    if (fdef->params != nullptr) {
        for (int i = 0; i < fdef->params->child->size(); ++i) {
            fdef->params->child->at(i)->ident->IdentName = transName(fdef->params->child->at(i)->ident->IdentName);
        }
    }
    // symbol table use lab2's code
    addFuncParamsToTable(fdef);
    // from ff get the arguments value
    BasicBlock* fentryBB = &ff->getEntryBlock();
    for (int i = 0; i < ff->arg_size(); ++i) {
        // if is a pointer
        if (ff->getArg(i)->getType()->isPointerTy()) {
            IDValueTable[index][fdef->params->child->at(i)->ident->IdentName] = ff->getArg(i);
        }
        // else
        else {
            // alloca and store
            Value* allo = AllocaInst::Create(Type::getIntegerTy(), 1, fentryBB);
            allo->setName(fdef->params->child->at(i)->ident->IdentName);
            Value* store = StoreInst::Create(ff->getArg(i), allo, fentryBB);
            IDValueTable[index][fdef->params->child->at(i)->ident->IdentName] = allo;
        }
    }
    // add func to funcTable
    auto it = FuncValueTable[0].find(fdef->ident->IdentName);
    if (it == FuncValueTable[0].end()) {
        FuncValueTable[0][fdef->ident->IdentName] = ff;
    }
    addToFuncSymbolTable(fdef);
}

// TODO
void doGlobalVarBinding(std::vector<TreeVarDef*>& defs, BasicBlock* bb) {
    // TODO
    // get address, and store is OK
    for (int i = 0; i < defs.size(); ++i) {
        TreeVarDef* def = defs[i];
        Value *l, *r;
        l = IDValueTable[0][def->ident->IdentName];
        if (def->initVal == nullptr) continue;
        r = translate_expr(def->initVal, bb);
        Value* store = StoreInst::Create(r, l, bb);
    }
}

std::string transName (std::string name) {
    return name + "." + std::to_string(IDnameTimes[name]++);
}

void clearNameNumTable() {
    IDnameTimes.clear();
}

std::string getTranstedName (std::string name) {
    // find the name with the same prefix of name from the table
    int index = IDValueTable.size() - 1;
    for (int i = index; i >= 0; --i) {
        for (auto it = IDValueTable[i].begin(); it != IDValueTable[i].end(); ++it) {
            if (name == getPrefix(it->first)) {
                return it->first;
            }
        }
    }
    return name;
}

std::string getPrefix (std::string name) {
    std::size_t pos = name.find('.');
    std::string prefix = name.substr(0, pos);
    return prefix;
}

Module* translate_root (Node* node) {
    // translate the root to the ir
    // new domain, and 
    auto* cmp = node->as<CompUnit*>();
    clearSynbolTable();
    newValueDomain();
    newDomain();
    // new compMod
    CompMod = new Module();
    // value need to add the runtime function
    addRuntimeFuncValue();
    addRunTimeFunc();
    // translate
    for (unsigned int i = 0; i < cmp->child->size(); ++i) {
        translate_stmt(cmp->child->at(i), nullptr);
    }
    // from CompMod get the main
    Function* mm = CompMod->getFunction("main");
    BasicBlock* mmentry = &mm->front();
    BasicBlock* gloVarBB = BasicBlock::Create(mm, mmentry);
    gloVarBB->setName("GlobalBind");
    doGlobalVarBinding(globalVarBinding, gloVarBB);
    Value* j2e = JumpInst::Create(mmentry, gloVarBB);

    deleteValueDomain();
    deleteDomain();
    return CompMod;
}


/**
 * ------------------------------------------------------------------------
 * 【5. 短路判定生成模块（进阶）】专为 If 极简条件语句中遇见的 && 和 || 设立
 * 背景：对于表达式 `if (A && B)`，只要发现左子树 A 是假的，连计算 B 的 IR 指令都不该跑。
 * 做法：与平平无奇的加法把全部算完放一块截然不同；
 * 这里通过人为给右子树 B 套上一层额外的基本块壁垒（隔离成 Inner、Right等），
 * 先算出 A，用一个 Branch 查其真假：A 为假时则果断用一把 Jump 刀越界砍飞，飞过 B 所在的区，
 * 跳进最后的目的地，由此在最底层汇编级实现了漂亮的 短路判定。
 * ------------------------------------------------------------------------
 * @param: ifStmt: the root if statement
 * @param: expr: now translating expr
 * @param: bb: now is in bb
 * @param: des: need to branch or jump to
 * @param: ptr: the return value should store to ptr
 * 
 * @return: the bb exit maybe unused
*/ 
BasicBlock* shortPathForIf (TreeIfStmt* ifStmt, TreeExpr* expr, BasicBlock* bb, BasicBlock* des, Value* ptr, int No) {
    // Binary
    // ||
    // &&
    // others
    if (auto* bop = expr->as<TreeBinaryExpr*>()) {
        // ||
        if (bop->op == OpType::OP_Lor) {
            // bb
            // orInnerBB
            // orRightBB
            // orFinalBB
            BasicBlock* OrInnerBB = BasicBlock::Create(bb->getParent(), bb);
            BasicBlock* OrRightBB = BasicBlock::Create(bb->getParent(), bb);
            BasicBlock* OrFinalBB = BasicBlock::Create(bb->getParent(), bb);
            OrInnerBB->setName("If_" + std::to_string(No) + "_Or_" + std::to_string(shortOrNo) + "_InnerBB");
            OrRightBB->setName("If_" + std::to_string(No) + "_Or_" + std::to_string(shortOrNo) + "_RightBB");
            OrFinalBB->setName("If_" + std::to_string(No) + "_Or_" + std::to_string(shortOrNo++) + "_FinalBB");
            // first translate left in bb
            Value* leftRetPtr = AllocaInst::Create(Type::getIntegerTy(), 1, &bb->getParent()->front());
            leftRetPtr->setName("tmp" + std::to_string(tmpNo++));
            BasicBlock* leftRetBB = shortPathForIf(ifStmt, bop->lhs, bb, OrInnerBB, leftRetPtr, No);    // may unused

            // in OrInnerBB, 
            Value* loadLeft = LoadInst::Create(leftRetPtr, OrInnerBB);
            Value* storeLeft = StoreInst::Create(loadLeft, ptr, OrInnerBB);                         // if now is OK, return the left value and is OK
            Value* leftBranch = BranchInst::Create(des, OrRightBB, loadLeft, OrInnerBB);

            // in OrRightBB
            Value* rightRetPtr = AllocaInst::Create(Type::getIntegerTy(), 1, &bb->getParent()->front());
            rightRetPtr->setName("tmp" + std::to_string(tmpNo++));
            BasicBlock* rightRetBB = shortPathForIf(ifStmt, bop->rhs, OrRightBB, OrFinalBB, rightRetPtr, No);   // may ununed

            // in OrFinalBB
            Value* loadRight = LoadInst::Create(rightRetPtr, OrFinalBB);
            Value* storeRight = StoreInst::Create(loadRight, ptr, OrFinalBB);
            Value* jump = JumpInst::Create(des, OrFinalBB);
        }
        // &&
        else if (bop->op == OpType::OP_Land) {
            // bb
            // AndInnerBB
            // AndRightBB
            // AndFinalBB
            BasicBlock* AndInnerBB = BasicBlock::Create(bb->getParent(), bb);
            BasicBlock* AndRightBB = BasicBlock::Create(bb->getParent(), bb);
            BasicBlock* AndFinalBB = BasicBlock::Create(bb->getParent(), bb);
            AndInnerBB->setName("If_" + std::to_string(No) + "_And_" + std::to_string(shortAndNo) + "_InnerBB");
            AndRightBB->setName("If_" + std::to_string(No) + "_And_" + std::to_string(shortAndNo) + "_RightBB");
            AndFinalBB->setName("If_" + std::to_string(No) + "_And_" + std::to_string(shortAndNo++) + "_FinalBB");
            // first translate left in bb
            Value* leftRetPtr = AllocaInst::Create(Type::getIntegerTy(), 1, &bb->getParent()->front());
            leftRetPtr->setName("tmp" + std::to_string(tmpNo++));
            BasicBlock* leftRetBB = shortPathForIf(ifStmt, bop->lhs, bb, AndInnerBB, leftRetPtr, No);

            // in AndInnerBB
            Value* loadLeft = LoadInst::Create(leftRetPtr, AndInnerBB);
            Value* storeLeft = StoreInst::Create(loadLeft, ptr, AndInnerBB);
            Value* leftBranch = BranchInst::Create(AndRightBB, des, loadLeft, AndInnerBB);

            // in AndRightBB
            Value* rightRetPtr = AllocaInst::Create(Type::getIntegerTy(), 1, &bb->getParent()->front());
            rightRetPtr->setName("tmp" + std::to_string(tmpNo++));
            BasicBlock* rightRetBB = shortPathForIf(ifStmt, bop->rhs, AndRightBB, AndInnerBB, rightRetPtr, No);

            // in AndFinalBB
            Value* loadRight = LoadInst::Create(rightRetPtr, AndFinalBB);
            Value* storeRight = StoreInst::Create(loadRight, ptr, AndFinalBB);
            Value* jump = JumpInst::Create(des, AndFinalBB);
        }
        // < <= > >= == !=      (+ - * / % xor)
        else {
            // can't short path, just like && and ||
            BasicBlock* RelInnerBB = BasicBlock::Create(bb->getParent(), bb);
            BasicBlock* RelRightBB = BasicBlock::Create(bb->getParent(), bb);
            BasicBlock* RelFinalBB = BasicBlock::Create(bb->getParent(), bb);
            RelInnerBB->setName("If_" + std::to_string(No) + "_Bina_" + std::to_string(shortBinNo) + "_InnerBB");
            RelRightBB->setName("If_" + std::to_string(No) + "_Bina_" + std::to_string(shortBinNo) + "_RightBB");
            RelFinalBB->setName("If_" + std::to_string(No) + "_Bina_" + std::to_string(shortBinNo++) + "_FinalBB");
            // get left value in bb
            Value* leftRetPtr = AllocaInst::Create(Type::getIntegerTy(), 1, &bb->getParent()->front());
            leftRetPtr->setName("tmp" + std::to_string(tmpNo++));
            BasicBlock* leftRetBB = shortPathForIf(ifStmt, bop->lhs, bb, RelInnerBB, leftRetPtr, No);

            // RelInnerBB
            Value* jumpLeft = JumpInst::Create(RelRightBB, RelInnerBB);

            // RelRightBB
            Value* rightRetPtr = AllocaInst::Create(Type::getIntegerTy(), 1, &bb->getParent()->front());
            rightRetPtr->setName("tmp" + std::to_string(tmpNo++));
            BasicBlock* rightRetBB = shortPathForIf(ifStmt, bop->rhs, RelRightBB, RelFinalBB, rightRetPtr, No);

            // RelFinalBB
            Value* loadLeft = LoadInst::Create(leftRetPtr, RelFinalBB);
            Value* loadRight = LoadInst::Create(rightRetPtr, RelFinalBB);
            Value* finalRes = nullptr;
            switch (bop->op)
            {
                case OpType::OP_Lt:
                    finalRes = BinaryInst::CreateLt(loadLeft, loadRight, Type::getIntegerTy(), RelFinalBB);
                    break;
                case OpType::OP_Le:
                    finalRes = BinaryInst::CreateLe(loadLeft, loadRight, Type::getIntegerTy(), RelFinalBB);
                    break;
                case OpType::OP_Gt:
                    finalRes = BinaryInst::CreateGt(loadLeft, loadRight, Type::getIntegerTy(), RelFinalBB);
                    break;
                case OpType::OP_Ge:
                    finalRes = BinaryInst::CreateGe(loadLeft, loadRight, Type::getIntegerTy(), RelFinalBB);
                    break;
                case OpType::OP_Eq:
                    finalRes = BinaryInst::CreateEq(loadLeft, loadRight, Type::getIntegerTy(), RelFinalBB);
                    break;
                case OpType::OP_Ne:
                    finalRes = BinaryInst::CreateNe(loadLeft, loadRight, Type::getIntegerTy(), RelFinalBB);
                    break;
                case OpType::OP_Add:
                    finalRes = BinaryInst::CreateAdd(loadLeft, loadRight, Type::getIntegerTy(), RelFinalBB);
                    break;
                case OpType::OP_Sub:
                    finalRes = BinaryInst::CreateSub(loadLeft, loadRight, Type::getIntegerTy(), RelFinalBB);
                    break;
                case OpType::OP_Mul:
                    finalRes = BinaryInst::CreateMul(loadLeft, loadRight, Type::getIntegerTy(), RelFinalBB);
                    break;
                case OpType::OP_Div:
                    finalRes = BinaryInst::CreateDiv(loadLeft, loadRight, Type::getIntegerTy(), RelFinalBB);
                    break;
                case OpType::OP_Mod:
                    finalRes = BinaryInst::CreateMod(loadLeft, loadRight, Type::getIntegerTy(), RelFinalBB);
                    break;
                case OpType::OP_Lxor:
                    finalRes = BinaryInst::CreateXor(loadLeft, loadRight, Type::getIntegerTy(), RelFinalBB);
                    break;
                default:
                    throw std::runtime_error("Error\n");
                    break;
            }
            Value* store = StoreInst::Create(finalRes, ptr, RelFinalBB);
            Value* jumpRight = JumpInst::Create(des, RelFinalBB);
        }
    }
    // UnaryOP
    // -a  !a foo(a)
    else if (auto* uop = expr->as<TreeUnaryExpr*>()) {
        // func call
        if (uop->op == OpType::OP_Func) {
            Function* ff = CompMod->getFunction(uop->id->IdentName);
            std::vector<Value*> args {};
            std::vector<Value*> ptrs {};
            BasicBlock* now = bb;
            int funcRPNo = shortFuncRNo++;
            
            if (uop->operand != nullptr) {
                TreeFuncRParams* rparams = uop->operand->as<TreeFuncRParams*>();
                for (int i = 0; i < rparams->child->size(); ++i) {
                    BasicBlock* rparamFinal = BasicBlock::Create(bb->getParent(), bb);
                    rparamFinal->setName("If_" + std::to_string(No) + "_FuncRPa_" + std::to_string(funcRPNo) + "_" + std::to_string(i) + "_FinalBB");
                    
                    // Value* tmp = AllocaInst::Create(Type::getIntegerTy(), 1, &bb->getParent()->front());
                    Value* tmp = nullptr;
                    if (ff->getArg(i)->getType() == Type::getIntegerTy()) {
                        tmp = AllocaInst::Create(Type::getIntegerTy(), 1, &bb->getParent()->front());
                    }
                    else {
                        tmp = AllocaInst::Create(PointerType::get(Type::getIntegerTy()), 1, &bb->getParent()->front());
                    }

                    tmp->setName("tmp" + std::to_string(tmpNo++));
                    ptrs.push_back(tmp);

                    shortPathForIf(ifStmt, rparams->child->at(i), now, rparamFinal, tmp, No);
                    now = rparamFinal;
                }

                for (int i = 0; i < rparams->child->size(); ++i) {
                    Value* load = LoadInst::Create(ptrs[i], now);
                    args.push_back(load);
                }
            }

            // call func in BB now
            Value* call = CallInst::Create(ff, args, now);
            Value* store = StoreInst::Create(call, ptr, now);
            Value* jump = JumpInst::Create(des, now);
        }
        // -a !a
        else {
            BasicBlock* uopFinal = BasicBlock::Create(bb->getParent(), bb);
            uopFinal->setName("If_" + std::to_string(No) + "_Uop_" + std::to_string(shortUopNo++) + "_FinalBB");

            Value* tmp = AllocaInst::Create(Type::getIntegerTy(), 1, &bb->getParent()->front());
            tmp->setName("tmp" + std::to_string(tmpNo++));

            shortPathForIf(ifStmt, uop->operand, bb, uopFinal, tmp, No);

            // 
            Value* load = LoadInst::Create(tmp, uopFinal);
            Value* res = nullptr;
            if (uop->op == OpType::OP_Lnot) {
                res = BinaryInst::CreateEq(ConstantInt::Create(0), load, Type::getIntegerTy(), uopFinal);
            }
            else {      // -
                res = BinaryInst::CreateSub(ConstantInt::Create(0), load, Type::getIntegerTy(), uopFinal);
            }
            Value* store = StoreInst::Create(res, ptr, uopFinal);
            Value* jump = JumpInst::Create(des, uopFinal);
        }
    }
    // Lval
    // a  a[0][0]
    else if (auto* lval = expr->as<TreeLVal*>()) {
        lval->ident->IdentName = getTranstedName(lval->ident->IdentName);
        // array
        bool isG = false;
        IdentTypeNode* lvalIDType = checkIdDefine(lval->ident);
        Value* lvalValue = findValue(lval->ident->IdentName, isG);
        // Pointer
        if ((lvalIDType->dimension != 0) && ((lval->exprs == nullptr) || (lval->exprs != nullptr && lval->exprs->size() < lvalIDType->dimension))) {
            std::vector<Value*> values {};
            std::vector<std::optional<std::size_t>> BoundList;

            // fill Boundlist
            for (int i = 0; i < lvalIDType->dimension; ++i) {
                if (lvalIDType->domainSize[i] == 0 && i == 0) {
                    BoundList.push_back(std::nullopt);
                }
                else {
                    BoundList.push_back(lvalIDType->domainSize[i]);
                }
            }

            // fill values
            BasicBlock* now = bb;
            int lvalNo = shortLvalNo++;
            std::vector<Value*> ptrs {};
            for (int i = 0; i < lvalIDType->dimension; ++i) {
                if (i < lval->exprs->size()) {
                    BasicBlock* valueFinal = BasicBlock::Create(now->getParent(), now);
                    valueFinal->setName("If_" + std::to_string(No) + "_Lval_" + std::to_string(lvalNo) + "_" + std::to_string(i) +"_FinalBB");
                    Value* tmp = AllocaInst::Create(Type::getIntegerTy(), 1, &bb->getParent()->front());
                    tmp->setName("tmp" + std::to_string(tmpNo++));
                    ptrs.push_back(tmp);

                    // 
                    shortPathForIf(ifStmt, lval->exprs->at(i), now, valueFinal, tmp, No);
                    now = valueFinal;
                }
            }
            for (int i = 0; i < lvalIDType->dimension; ++i) {
                if (i < lval->exprs->size()) {
                    Value* load = LoadInst::Create(ptrs[i], now);
                    values.push_back(load);
                }
                else {
                    values.push_back(ConstantInt::Create(0));
                }
            }

            Value* offset = OffsetInst::Create(Type::getIntegerTy(), lvalValue, values, BoundList, now);
            Value* store = StoreInst::Create(offset, ptr, now);

            // jump
            Value* jump = JumpInst::Create(des, now);
        }
        // array, return a num
        else if (lval->hasExpress) {
            std::vector<Value*> values {};
            std::vector<std::optional<std::size_t>> BoundList;

            // fill Boundlist
            for (int i = 0; i < lvalIDType->dimension; ++i) {
                if (lvalIDType->domainSize[i] == 0 && i == 0) {
                    BoundList.push_back(std::nullopt);
                }
                else {
                    BoundList.push_back(lvalIDType->domainSize[i]);
                }
            }

            // fill values
            BasicBlock* now = bb;
            int lvalNo = shortLvalNo++;
            std::vector<Value*> ptrs {};
            for (int i = 0; i < lvalIDType->dimension; ++i) {
                BasicBlock* valueFinal = BasicBlock::Create(now->getParent(), now);
                valueFinal->setName("If_" + std::to_string(No) + "_Lval_" + std::to_string(lvalNo) + "_" + std::to_string(i) +"_FinalBB");
                Value* tmp = AllocaInst::Create(Type::getIntegerTy(), 1, &bb->getParent()->front());
                tmp->setName("tmp" + std::to_string(tmpNo++));
                ptrs.push_back(tmp);

                // trans the child expr
                shortPathForIf(ifStmt, lval->exprs->at(i), now, valueFinal, tmp, No);
                now = valueFinal;
            }
            for (int i = 0; i < lvalIDType->dimension; ++i) {
                Value* load = LoadInst::Create(ptrs[i], now);
                values.push_back(load);
            }

            Value* offset = OffsetInst::Create(Type::getIntegerTy(), lvalValue, values, BoundList, now);
            Value* load = LoadInst::Create(offset, now);
            Value* store = StoreInst::Create(load, ptr, now);

            // jump
            Value* jump = JumpInst::Create(des, now);

        }
        // just like (i), just a number
        else {
            Value* load = LoadInst::Create(lvalValue, bb);
            Value* store = StoreInst::Create(load, ptr, bb);
            Value* jump = JumpInst::Create(des, bb);
        }
    }
    // const int
    else if (auto* cint = expr->as<TreeIntegerLiteral*>()) {
        Value* constInt = ConstantInt::Create(cint->value);
        Value* store = StoreInst::Create(constInt, ptr, bb);
        Value* jump = JumpInst::Create(des, bb);
    }
}


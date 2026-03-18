#include "ast/ast.h"
#include <fmt/core.h>

extern int yyparse();
extern FILE* yyin;

NodePtr root;

int main(int argc, char **argv) {
    // read in from file
    if(argc > 1) {
        for(int i=1; i<argc; i++) {
            if(!(yyin = fopen(argv[i], "r"))) {
                perror(argv[1]);
                return 1;
            }
            root = nullptr;
            yyparse();              // 解析输入文件，构建 AST
            try{    
                print_tree(root);   // 打印 AST 同时检查语义合法性
            }
            catch (const char *s) {
                fmt::print("\033[31m[ERROR] :\033[0m {}", s);
                return 1;
            }
        }
    }
    else {
        yyparse();
        try{    
            print_tree(root); 
        }
        catch (const char *s) {
            fmt::print("\033[31m[ERROR] :\033[0m {}", s);
            return 1;
        }
    }
    
    fmt::print("[***]Hello, World!\n");
    
    return 0;
}

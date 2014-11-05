#define DEBUG
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <vector>
//#include <fstream>
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FileSystem.h"

#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Basic/TargetOptions.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/Lexer.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/Parse/ParseAST.h"	
#include "clang/Rewrite/Frontend/Rewriters.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/AST/ASTContext.h"

#include "clang-c/Index.h"
#include <algorithm>

//
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"


// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"
#include "clang/AST/ASTContext.h"
#include "iostream"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <algorithm>
#include <vector>
using namespace clang::tooling;
using namespace llvm;
using namespace clang;
using namespace clang::ast_matchers;

const int BUFSIZE = 80;
std::string checkDataFileName = "checkData1.txt";//存入遍历free时得到的信息
std::string checkLog2FileName = "checkLog2.txt";
typedef struct checkPoint{
    //std::string flagName;
    std::string name;//插入点的变量
    int row,col;//行号,列号
    std::string declName;//原先定义的位置,先保留不用
    int declRow,declCol;   
}checkPoint;
std::vector<checkPoint> vec_cp;//存malloc时得到的点
std::vector<checkPoint> vec_cpF;//存free时得到的点,临时使用


Rewriter rewrite;

//将loc的string信息变为row,col
inline void loc_strToint(int & int_loc_row,int & int_loc_col,const char*str_loc){
    char buf[BUFSIZE];
    sscanf(str_loc,"%[^:]:%d:%d",buf,&int_loc_row,&int_loc_col);
    
}

//---跟以前的一样
class MyRecursiveASTVisitor
    : public RecursiveASTVisitor<MyRecursiveASTVisitor>
{
    public:
    MyRecursiveASTVisitor(Rewriter &R) : Rewrite(R) { }
    bool VisitVarDecl(VarDecl *d);
    Rewriter &Rewrite;
};
bool MyRecursiveASTVisitor::VisitVarDecl(VarDecl *d){
	return true;
}
class MyASTConsumer : public ASTConsumer{
 public:
	MyASTConsumer(Rewriter &Rewrite) : rv(Rewrite){
	}
	virtual bool HandleTopLevelDecl(DeclGroupRef d);
    MyRecursiveASTVisitor rv;
};
bool MyASTConsumer::HandleTopLevelDecl(DeclGroupRef d)
{
  typedef DeclGroupRef::iterator iter;

  for (iter b = d.begin(), e = d.end(); b != e; ++b){
    rv.TraverseDecl(*b);
  }
  return true; // keep going
}

//---跟以前的一样


//一些附加信息
static llvm::cl::OptionCategory MyToolCategory("my-tool options");
// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);
// A help message for this specific tool can be added afterwarDRE.
static cl::extrahelp MoreHelp("\nMore help text...");



//匹配到malloc()里的变量
StatementMatcher MallocVarMatcher = declRefExpr(hasParent(binaryOperator(
               hasOperatorName("="),
                hasRHS(cStyleCastExpr(has(callExpr(has(declRefExpr(to(functionDecl(hasName("malloc")))))))))))).bind("mallocVar");

//匹配到malloc()那个二元表达式
StatementMatcher MallocMatcher = binaryOperator(
               hasOperatorName("="),
               //hasLHS(anything()),
                hasRHS(cStyleCastExpr(has(callExpr(has(declRefExpr(to(functionDecl(hasName("malloc")))))))))).bind("malloc");

//匹配到直接初始化的malloc()
DeclarationMatcher Malloc2Matcher = varDecl(has(cStyleCastExpr(has(callExpr(has(declRefExpr(to(functionDecl(hasName("malloc")))))))))).bind("malloc2");
//has(cStyleCastExpr(has(callExpr(declRefExpr(to(functionDecl(hasName("malloc"))))))))).bind("malloc2");

//匹配到free()的表达式                                                
StatementMatcher FreeMatcher = callExpr(has(declRefExpr(to(functionDecl(hasName("free")))))).bind("free");
//匹配到free()里的变量
StatementMatcher FreeVarMatcher = declRefExpr(hasParent(implicitCastExpr(hasParent(implicitCastExpr(hasParent(callExpr(has(declRefExpr(to(functionDecl(hasName("free")))))))))))).bind("freeVar");                

StatementMatcher TestMatcher = declStmt(has(varDecl(has(cStyleCastExpr(has(callExpr(has(declRefExpr(to(functionDecl(hasName("malloc")))))))))))).bind("test");
DeclarationMatcher Test2Matcher = varDecl(has(cStyleCastExpr(anything()))).bind("test2");

StatementMatcher ArraySubMatcher = 	arraySubscriptExpr().bind("arraySub");
StatementMatcher ArraySubRefMatcher = declRefExpr(hasParent(implicitCastExpr(hasParent(arraySubscriptExpr())))).bind("arraySubRef");

StatementMatcher IfMatcher = ifStmt().bind("if");
typedef struct ifRange{
    int srow,scol;//start 
    int erow,ecol;//end
    SourceLocation sl;
}ifRange;
std::vector<ifRange> vec_ir;
int index_vec_ir = 0;
bool findInIf(int row,int col,int &index){

    int len = vec_ir.size();
    if(len == 0)return false;
    if(vec_ir[len-1].erow < row)return false;
    if(vec_ir[len-1].erow == row && vec_ir[len-1].erow < col)return false;
    int k = index_vec_ir;

    bool flag = false;
    while(k<len && vec_ir[k].srow <= row){
        if(vec_ir[k].erow >=row ){
            if(vec_ir[k].erow == row && vec_ir[k].ecol < col){
                ++k;continue;
            }
            flag = true;
            break;    
        }
        ++k;
    }
    if(!flag)return false;
    if(flag)index_vec_ir = k;
    index = k;
    return true;
}


typedef struct checkArray{
     int row,col;//行号,列号
     std::string name;
     std::vector<int>bounds;
}checkArray;
std::vector<checkArray> vec_ca;
int vec_caIndex = 0;
bool judgeOp(char c){
    if(c == '+' || c == '-' || c == '*' || c == '/' || c == '(' || c == ')')
        return true;
    else
        return false;
}
void changePlusPlus(char * source,char * result,const char *T){
    int findIndex = 0,rIndex=0;
    int i=0,j=0,k=0,len = strlen(source);
    char se[128];
    while(i<len && source[i] == ' ')++i;
    int start = i;
    for(k=start,j=0;k<len;++k){
        if(source[k] != ' ')
            se[j++] = source[k];
    }
    int slen = j;
    se[j] = '\0';
    for(k=0;k<slen;++k){
        if(se[k] == T[findIndex]){
            if(se[k+1] == T[findIndex+1]){
                int q = k+2;
                if(k-1<0||judgeOp(se[k-1])){//e.x (++b)
             
                    result[rIndex++] = '(';
                    while(q < slen && !judgeOp(se[q]) ){
                        result[rIndex++] = se[q];
                        ++q;
                    }
                    result[rIndex++] = T[0];
                    strcpy(result+rIndex,"1)");
                    rIndex += 2;
                    k = q-1;              
                
                }else{
                    k = q-1;    //out has +1 
                }
                continue;
            }
        }
        result[rIndex++] = se[k];
    }    
    result[rIndex] = '\0';
}
void changeTogether(char * source,char * result){
    char tempResult[128];
    changePlusPlus(source,tempResult,"++");
    changePlusPlus(tempResult,result,"--");
}
bool getBoundsByStr(const char* str,std::vector<int> & vec){
    char buf[32];
    int bufIndex = -1;
    int len = strlen(str);
    bool FindFlag = false;//e.x char ** cannot be checked
    for(int i=0;i<len;++i){
        if(str[i] == '['){
            bufIndex = 0;
            FindFlag = true;
        }else if(str[i] == ']'){
            buf[bufIndex++] = '\0';
            int a;
            if(buf == "")return false;
            sscanf(buf,"%d",&a);
            vec.push_back(a);
            bufIndex = -1; 
        }else if(bufIndex != -1){
            buf[bufIndex++] = str[i];
        }
    }
    return FindFlag;
}

void getBoundsStrByStr(const char *str,std::vector<std::string> &vec){
    char buf[32];
    int bufIndex = -1;
    int len = strlen(str);
    for(int i=0;i<len;++i){
        if(str[i] == '['){
            bufIndex = 0;
        }else if(str[i] == ']'){
            buf[bufIndex++] = '\0';
            std::string temp = buf;
            vec.push_back(temp);
            bufIndex = -1; 
        }else if(bufIndex != -1){
            buf[bufIndex++] = str[i];
        }
    }

}
class ArraySubRefPrinter : public MatchFinder::MatchCallback{
public:
    virtual void run(const MatchFinder::MatchResult &Result){
        const DeclRefExpr* DRE = Result.Nodes.getNodeAs<DeclRefExpr>("arraySubRef");
        #ifdef DEBUG
        llvm::errs()<<"----ArraySubRef find----\n";
        #endif
        if(!DRE)   return ;
        const SourceManager *SM = Result.SourceManager;
        SourceRange sr = SourceRange(DRE->getLocStart(),DRE->getLocEnd());
        std::string grt = rewrite.getRewrittenText(sr) ;

        checkArray ca;        
        SourceLocation locStart = DRE->getLocStart();
        std::string str_locStart = locStart.printToString(*SM);
        loc_strToint(ca.row,ca.col,str_locStart.c_str());
        
        if(!getBoundsByStr(((DRE->getType()).getAsString()).c_str(),ca.bounds))
            return;
        
        #ifdef DEBUG
//        llvm::errs()<<"grt:"<<grt<<"\n";
//        llvm::errs() <<"arraySubRef loc:" <<"|"<<ca.row<<"|"<<ca.col<<"\n";
//        llvm::errs() <<"arraySubRef type:"<<(DRE->getType()).getAsString()<<"\n";
        for(int i=0;i<ca.bounds.size();++i)
            llvm::errs()<<ca.bounds[i]<<"|";
        llvm::errs()<<"\n";
        #endif
        vec_ca.push_back(ca);
        
        #ifdef DEBUG
        llvm::errs()<<"----ArraySubREf end----\n";
        #endif
    }
};
class ArraySubPrinter : public MatchFinder::MatchCallback{
public:
    virtual void run(const MatchFinder::MatchResult &Result){
        const ArraySubscriptExpr* AS = Result.Nodes.getNodeAs<ArraySubscriptExpr>("arraySub");
        #ifdef DEBUG
        llvm::errs()<<"----ArraySub find----\n";
        #endif
        if(!AS)   return ;
        const SourceManager *SM = Result.SourceManager;
        checkArray ca;
        SourceLocation locStart = AS->getLocStart();
        std::string str_locStart = locStart.printToString(*SM);
        loc_strToint(ca.row,ca.col,str_locStart.c_str());
            
        SourceRange sr = SourceRange(AS->getLocStart(),AS->getLocEnd());
        std::string grt = rewrite.getRewrittenText(sr) ;

        int caFindIndex = -1;
        for(int i=vec_caIndex;i<vec_ca.size();++i){
            if(vec_ca[i].row == ca.row && vec_ca[i].col == ca.col){
                caFindIndex = i;
                break;    
            }
        }
        if(caFindIndex == -1) return ;
        
        vec_caIndex = caFindIndex;
        std::vector<std::string> vec_str;
        getBoundsStrByStr(grt.c_str(),vec_str);


        #ifdef DEBUG
        llvm::errs() <<"grt:"<<grt<<"\n";
        llvm::errs() <<"arraySub loc:" <<"|"<<ca.row<<"|"<<ca.col<<"\n";
        llvm::errs() <<"arraySub type:"<<(AS->getType()).getAsString()<<"\n";
        llvm::errs() <<"arraySub base:"<<((AS->getBase())->getType()).getAsString()<<"\n";
        llvm::errs() <<"arraySub Idx:"<<((AS->getIdx())->getType()).getAsString()<<"\n";
        llvm::errs() <<"arraySub rhs:"<<((AS->getRHS())->getType()).getAsString()<<"\n";

        
        const Expr * lhs = AS->getLHS();

        llvm::errs()<<"lhs type:"<<(lhs->getType()).getAsString()<<"\n";
        lhs->dump();
        #endif

        
        
		//string + 不支持int类型,所以先换成char*
        char buf[2][32];
        sprintf(buf[0],"%d",ca.row);
        sprintf(buf[1],"%d",ca.col);

                                     
        
        int bdlen = vec_ca[vec_caIndex].bounds.size();
        std::string str_insert = "if(";

        for(int i=0;i<bdlen;++i){
            char intBuf[32];
            sprintf(intBuf,"%d\0",vec_ca[vec_caIndex].bounds[i]);
            str_insert = str_insert + intBuf;
            char changeResult[128];
            char changeStr[128];
            strcpy(changeStr,vec_str[i].c_str());
            changeTogether(changeStr,changeResult);//remove ++ , --
            llvm::errs()<<"changeResult:"<<changeResult<<"\n";
            str_insert = str_insert + "<=("+changeResult+")";
            if(i == bdlen-1)
                str_insert += "){\n";
            else
                str_insert += "||";
        }
        str_insert = str_insert + "\tprintf(\"" +  buf[0]+":"+buf[1]+":"+grt+" array visit out of bounds!\\n\");\n";


        str_insert = str_insert + "\n{\n\tFILE *fp = fopen(\"" + checkLog2FileName +"\",\"a\");\n"
        "\tif(fp == NULL){\n" +
        "\t\tprintf(\"fail to open file " + checkLog2FileName + "!\\n\");\n" +
        "\t\texit(-1);\n" + 
        "\t}\n" + 
        "\tfprintf(fp,\""+  buf[0]+":"+buf[1]+":"+grt+" array visit out of bounds!\\n\");\n" +
        "\tfclose(fp);\n}\n}\n";
        
        ++vec_caIndex;
        
            
        llvm::errs() << "-----\n"<<str_insert<<"\n----\n";
		//找位置插装
		int fIndex;
		if(findInIf(ca.row,ca.col,fIndex)){
            rewrite.InsertText(vec_ir[fIndex].sl,str_insert.c_str(),true,true); 	    
		}else{
            int locOffset = ca.col;
            SourceLocation SL_locWithOffset = locStart.getLocWithOffset(-1*ca.col+1);
            rewrite.InsertText(SL_locWithOffset,str_insert.c_str(),true,true); 
        }

        
        #ifdef DEBUG
        llvm::errs()<<"----ArraySub end----\n";
        #endif
        
    }
    
    
};
class IfPrinter : public MatchFinder::MatchCallback{
public:
    virtual void run(const MatchFinder::MatchResult &Result){
        const IfStmt * ifs = Result.Nodes.getNodeAs<IfStmt>("if");
        #ifdef DEBUG
        llvm::errs()<<"----(if) find----\n";
        #endif
        const SourceManager *SM = Result.SourceManager;
        
        const Expr* ifcn = ifs->getCond();
        ifRange ir;
        
        SourceLocation ifsst = ifs->getLocStart();
        std::string str_ifsst = ifsst.printToString(*SM);
        loc_strToint(ir.srow,ir.scol,str_ifsst.c_str());
        
        SourceLocation ifcned = ifcn->getLocEnd();
        std::string str_ifcned = ifcned.printToString(*SM);
        loc_strToint(ir.erow,ir.ecol,str_ifcned.c_str());
        
        ir.sl = ifsst;
        
        vec_ir.push_back(ir);
        #ifdef DEBUG
        llvm::errs()<<"st:"<<ir.srow<<":"<<ir.scol<<"|ed:"<<ir.erow<<":"<<ir.ecol<<"\n";
        llvm::errs()<<"----(if) end----\n";
        #endif
    }
};
class Test2Printer : public MatchFinder::MatchCallback{
public:
    virtual void run(const MatchFinder::MatchResult &Result){
        const VarDecl* VD = Result.Nodes.getNodeAs<VarDecl>("test2");
        #ifdef DEBUG
        llvm::errs()<<"----VD(test2) find----\n";
        #endif
        
        if(!VD)   return ;
        VD->dump();
        #ifdef DEBUG
        llvm::errs()<<"----VD(test2) find----\n";
        #endif
    }
};
class TestPrinter : public MatchFinder::MatchCallback{
public:
    virtual void run(const MatchFinder::MatchResult &Result){
        const DeclStmt* CSCE = Result.Nodes.getNodeAs<DeclStmt>("test");
        #ifdef DEBUG
        llvm::errs()<<"----(test) find----\n";
        #endif
        
        if(!CSCE)   return ;
        CSCE->dump();
        #ifdef DEBUG
        llvm::errs()<<"----(test) find----\n";
        #endif
    }
};

class MallocPrinter : public MatchFinder::MatchCallback{
public:
    virtual void run(const MatchFinder::MatchResult &Result){
        const BinaryOperator* BO = Result.Nodes.getNodeAs<BinaryOperator>("malloc");
        #ifdef DEBUG
        llvm::errs()<<"----BinaryOperator(malloc) find----\n";
        #endif
        if(!BO)   return ;
        const SourceManager *SM = Result.SourceManager;
        SourceLocation locEnd = BO->getLocEnd();
        checkPoint cp;
      
	
		//得到插装位置,找到mallocVarMatcher之前对应匹配到的信息(其实可以不用MallocVarMatcher,MallocVarMatcher只能匹配到纯粹的指针(不带*的))
        std::string str_locEnd = locEnd.printToString(*SM);
        loc_strToint(cp.row,cp.col,str_locEnd.c_str());
        bool findFlag = false;
        int findI;
        
        #ifdef DEBUG
        llvm::errs() <<"binary loc:" <<"|"<<cp.row<<"|"<<cp.col<<"\n";
        #endif
        
        for(unsigned i=0;i<vec_cp.size();++i){
            if(vec_cp[i].row == cp.row){
                    findFlag = true;
                    findI = i;
                    break;
                }
        }
		//左子树得到的 = 的左边
        Expr * lhs = BO->getLHS();
        cp.name = rewrite.ConvertToString((Stmt*)lhs);
        
        QualType qt = lhs->getType();
        #ifdef DEBUG
        llvm::errs()<<"lhs cp.name :"<<cp.name<<"\n";
        llvm::errs()<<"lhs type :"<<qt.getAsString()<<"\n";
        lhs->dump();
        #endif
        
		//找到的话直接用
        if(findFlag){        
            const NamedDecl *ND = ((DeclRefExpr*)lhs)->getFoundDecl();
            std::string str_decl = ND->getNameAsString();
            cp.declName = str_decl;
            SourceLocation declLocStart = ND->getLocStart();
            std::string str_declLocStart = declLocStart.printToString(*SM);
            loc_strToint(cp.declRow,cp.declCol,str_declLocStart.c_str());
        
        }else{//没找到的话,添加进来
            cp.declName = cp.name;
            cp.declRow = cp.row;
            cp.declCol = cp.col;
        }
        
		//string + 不支持int类型,所以先换成char*
        char buf[4][32];
        sprintf(buf[0],"%d",cp.row);
        sprintf(buf[1],"%d",cp.col);
        sprintf(buf[2],"%d",cp.declRow);
        sprintf(buf[3],"%d",cp.declCol);  
                                     
        //将程序运行时的指针值存下来 %x                                     
        std::string str_insert = 
        "\n{\n\tFILE *fp = fopen(\"" + checkDataFileName +"\",\"a\");\n"
        "\tif(fp == NULL){\n" +
        "\t\tprintf(\"fail to open file " + checkDataFileName + "!\\n\");\n" +
        "\t\texit(-1);\n" + 
        "\t}\n" + 
        
        //"\tfprintf(fp,\"m " + cp.name + " " + buf[0] + " " + buf[1] + " " + cp.declName + " " + buf[2] + " " + buf[3] + " %x \\n\"," + cp.name + ");\n" +
        "\tfprintf(fp,\"m " + cp.name + " " + buf[0] + " " + buf[1] + " %x \\n\"," + cp.name + ");\n" +
        "\tfclose(fp);\n" +

        "}\n";
        
        
        
        //llvm::errs() << "-----\n"<<str_insert<<"\n----\n";
		//找位置插装
        int locOffset = 2;
        SourceLocation SL_locWithOffset = locEnd.getLocWithOffset(locOffset);
        rewrite.InsertText(SL_locWithOffset,str_insert.c_str(),true,true); 
        
		if(!findFlag){        
            vec_cp.push_back(cp);
        }

        #ifdef DEBUG
        llvm::errs()<<"----BinaryOperator(malloc) end----\n";
        #endif
    }
    
    
};

class Malloc2Printer : public MatchFinder::MatchCallback{
public:
    virtual void run(const MatchFinder::MatchResult &Result){
        
        const VarDecl * VD = Result.Nodes.getNodeAs<VarDecl>("malloc2");
        #ifdef DEBUG
        llvm::errs()<<"----VarDecl(malloc2) find----\n";
        #endif
        if(!VD)   return ;
        const SourceManager *SM = Result.SourceManager;
        SourceLocation locEnd = VD->getLocEnd();
        checkPoint cp;
      
	
		//得到插装位置,找到mallocVarMatcher之前对应匹配到的信息(其实可以不用MallocVarMatcher,MallocVarMatcher只能匹配到纯粹的指针(不带*的))
        std::string str_locEnd = locEnd.printToString(*SM);
        loc_strToint(cp.row,cp.col,str_locEnd.c_str());

        int findI;
        
        #ifdef DEBUG
        llvm::errs() <<"varDecl loc:" <<"|"<<cp.row<<"|"<<cp.col<<"\n";
        #endif
        
		VD->dump();
        
        const NamedDecl *ND = (NamedDecl*)VD;
        
        llvm::errs()<<"ND "<<ND<<"\n";
        std::string str_decl = ND->getNameAsString();
        cp.declName = str_decl;
        SourceLocation declLocStart = ND->getLocStart();
        std::string str_declLocStart = declLocStart.printToString(*SM);
        loc_strToint(cp.declRow,cp.declCol,str_declLocStart.c_str());
        
        cp.name = str_decl;
               
		//string + 不支持int类型,所以先换成char*
        char buf[4][32];
        sprintf(buf[0],"%d",cp.row);
        sprintf(buf[1],"%d",cp.col);
        sprintf(buf[2],"%d",cp.declRow);
        sprintf(buf[3],"%d",cp.declCol);  
                                     
        //将程序运行时的指针值存下来 %x                                     
        std::string str_insert = 
        "\n{\n\tFILE *fp = fopen(\"" + checkDataFileName +"\",\"a\");\n"
        "\tif(fp == NULL){\n" +
        "\t\tprintf(\"fail to open file " + checkDataFileName + "!\\n\");\n" +
        "\t\texit(-1);\n" + 
        "\t}\n" + 
        
        //"\tfprintf(fp,\"m " + cp.name + " " + buf[0] + " " + buf[1] + " " + cp.declName + " " + buf[2] + " " + buf[3] + " %x \\n\"," + cp.name + ");\n" +
        "\tfprintf(fp,\"m " + cp.name + " " + buf[0] + " " + buf[1] + " %x \\n\"," + cp.name + ");\n" +
        "\tfclose(fp);\n" +

        "}\n";
        
        
        
        //llvm::errs() << "-----\n"<<str_insert<<"\n----\n";
		//找位置插装
        int locOffset = 2;
        SourceLocation SL_locWithOffset = locEnd.getLocWithOffset(locOffset);
        rewrite.InsertText(SL_locWithOffset,str_insert.c_str(),true,true); 
        
	   
        vec_cp.push_back(cp);
    

        #ifdef DEBUG
        llvm::errs()<<"----VarDecl(malloc2) end----\n";
        #endif
        
    }
    
    
};
class FreePrinter : public MatchFinder::MatchCallback{//free插装在前面
public:
    virtual void run(const MatchFinder::MatchResult &Result){//free
        const CallExpr* CE = Result.Nodes.getNodeAs<CallExpr>("free");
        #ifdef DEBUG
        llvm::errs()<<"----CallExpr(free) find----\n";
        #endif
        if(!CE)   return ;
        

        //得到free()里的第0个参数,进行类似的操作
		const SourceManager *SM = Result.SourceManager;
        const Expr * arg = CE->getArg(0);        
        checkPoint cp;
        cp.name = rewrite.ConvertToString((Stmt*)arg);
        
        SourceLocation locStart = CE->getLocStart();
        std::string str_locStart = locStart.printToString(*SM);
        loc_strToint(cp.row,cp.col,str_locStart.c_str());
        
        #ifdef DEBUG
        llvm::errs()<<"cp:"<<cp.row<<" "<<cp.col<<" " + str_locStart + "\n";
        #endif
        
        bool findFlag = false;
        for(unsigned i=0;i<vec_cpF.size();++i){
            if(cp.name == vec_cpF[i].name &&
             cp.row == vec_cpF[i].row){
                findFlag = true;
                cp.col = vec_cpF[i].col;
                cp.declName = vec_cpF[i].declName;
                cp.declRow = vec_cpF[i].declRow;
                cp.declCol = vec_cpF[i].declCol;
                break;
            }
        }
        if(!findFlag){
            cp.declName = cp.name;
            cp.declRow = cp.row;
            cp.declCol = cp.col;
        }
                
        char buf[4][32];
        sprintf(buf[0],"%d",cp.row);
        sprintf(buf[1],"%d",cp.col);
        sprintf(buf[2],"%d",cp.declRow);
        sprintf(buf[3],"%d",cp.declCol);                        
		//之后可以进行开文件的优化
        std::string str_insert = 
        "\n{\n\tFILE *fp = fopen(\"" + checkDataFileName +"\",\"a\");\n"
        "\tif(fp == NULL){\n" +
        "\t\tprintf(\"fail to open file " + checkDataFileName + "!\\n\");\n" +
        "\t\texit(-1);\n" + 
        "\t}\n"  + 
        
        //"\tfprintf(fp,\"f " + cp.name + " " + buf[0] + " " + buf[1] + " " + cp.declName + " " + buf[2] + " " + buf[3] + " %x \\n\"," + cp.name + ");\n" +
        "\tfprintf(fp,\"f " + cp.name + " " + buf[0] + " " + buf[1] + " %x \\n\"," + cp.name + ");\n" +
        "\tfclose(fp);\n" +
        
        /*
        "\tint fd=open(FIFO_SERVER,O_WRONLY |O_NONBLOCK,0);\n" + 
        "\tif(fd==-1){perror(\"open\");exit(1);}\n" + 
        "\tchar w_buf[100];\n" + 
        "\tsprintf(w_buf,\"f "+ cp.name + " " + buf[0] + " " + buf[1] + " %x \\n\"," + cp.name + ");\n" + 
        "\tif(write(fd,w_buf,100)==-1){\n" + 
        "\t\tif(errno==EAGAIN)\n" + 
        "\t\t\tprintf(\"The FIFO has not been read yet.Please try later\\n\");\n" + 
        "\t}\n" + 
        "\telse\n" + 
        "\t\tprintf(\"write %s to the FIFO\\n\",w_buf);\n" +
        "\tsleep(1);\n" + 
        "\tclose(fd);\n" + 
        */
        "}\n";
        

       // llvm::errs() << "-----\n"<<str_insert<<"\n----\n";
        SourceLocation SL_locWithOffset = locStart;
        rewrite.InsertText(SL_locWithOffset,str_insert.c_str(),true,true);   
        #ifdef DEBUG
        llvm::errs()<<"----CallExpr(free) end----\n";
        #endif

    }
};

class FreeVarPrinter : public MatchFinder::MatchCallback{//freeVar 插装在前面
public:
    virtual void run(const MatchFinder::MatchResult &Result){//freeVar
        const DeclRefExpr* DRE = Result.Nodes.getNodeAs<DeclRefExpr>("freeVar");
        #ifdef DEBUG
        llvm::errs()<<"----DRE(freeVar) find----\n";
        #endif
        
        if(!DRE)   return ;

        const SourceManager *SM = Result.SourceManager;
        
        const NamedDecl *ND = DRE->getFoundDecl();
        checkPoint cp;
        cp.name = rewrite.ConvertToString((Stmt*)DRE);
        std::string str_decl = ND->getNameAsString();
        cp.declName = str_decl;
       
        SourceLocation declLocStart = ND->getLocStart();
        std::string str_declLocStart = declLocStart.printToString(*SM);
        loc_strToint(cp.declRow,cp.declCol,str_declLocStart.c_str());

        SourceLocation locStart = DRE->getLocStart();
        std::string str_locStart = locStart.printToString(*SM);
        loc_strToint(cp.row,cp.col,str_locStart.c_str());


        
        vec_cpF.push_back(cp);
        #ifdef DEBUG
        llvm::errs()<<"----DRE(freeVar) end----\n";
        #endif
    }
};
class MallocVarPrinter : public MatchFinder::MatchCallback{
public:
    virtual void run(const MatchFinder::MatchResult &Result){
        const DeclRefExpr* DRE = Result.Nodes.getNodeAs<DeclRefExpr>("mallocVar");
        #ifdef DEBUG
        llvm::errs()<<"----DRE(mallocVar) find----\n";
        #endif

        if(!DRE)   return ;

        const SourceManager *SM = Result.SourceManager;
        SourceLocation locStart = DRE->getLocStart();
        
        std::string str_locStart = locStart.printToString(*SM);
        
        checkPoint cp;
        loc_strToint(cp.row,cp.col,str_locStart.c_str());


        cp.name = rewrite.ConvertToString((Stmt*)DRE);

        //找到该变量原先的声明处 
        const NamedDecl *ND = DRE->getFoundDecl();
        std::string str_decl = ND->getNameAsString();
        cp.declName = ND->getNameAsString();
        
        SourceLocation declLocStart = ND->getLocStart();
        std::string str_declLocStart = declLocStart.printToString(*SM);
        loc_strToint(cp.declRow,cp.declCol,str_declLocStart.c_str());
        
        #ifdef DEBUG
        llvm::errs()<<"\ncp: "
        <<cp.row<<":"<<cp.col<<":"<<cp.declRow<<":"<<cp.declCol<<"\n";
        #endif

        
        vec_cp.push_back(cp);
        
        #ifdef DEBUG
        llvm::errs() << cp.name << "|\n" <<  cp.declName <<"|\n"
        << cp.declRow << ":" << cp.declCol << "\n";
        llvm::errs() << "ND:\n";
        ND->dump();
        llvm::errs() << "DRE:\n";
        DRE->dump();

        llvm::errs()<<"----DRE(mallocVar) end----\n";
        #endif

    }
};

bool addHeaderPath(HeaderSearchOptions & hso){
    char buf[128];
    int index = 0;
    FILE*fp;
    fp = fopen("config.ini","r");
    if(!fp)return false;
    while(fgets(buf,128,fp)!=0 && strlen(buf)!=0&&index<10){
        if(buf[0] == '#')continue;
        hso.AddPath(buf,
          clang::frontend::Angled,
          false,
          false);
        ++index;
    }
    fclose(fp);
    return true;

}

//使用的格式:  ./checkMemory 被测试文件名 --
int main(int argc,const char **argv) {

	//----此块基本一样    start----
    struct stat sb;             

    //set compilerinstance for rewriter		
	std::string fileName(argv[1]);
	if (stat(fileName.c_str(), &sb) == -1){
        perror(fileName.c_str());
        exit(EXIT_FAILURE);
    }

	
	CompilerInstance compiler;
	DiagnosticOptions diagnosticOptions;
	compiler.createDiagnostics();


	//invocation可以传递任何flag给preprocessor
	CompilerInvocation *Invocation = new CompilerInvocation;

	CompilerInvocation::CreateFromArgs(*Invocation, argv + 1, argv + argc-1,compiler.getDiagnostics());

	compiler.setInvocation(Invocation);


	//建立TargetOptions和TargetInfo,并设置好Target
	// Set default target triple
	llvm::IntrusiveRefCntPtr<TargetOptions> pto( new TargetOptions());
	pto->Triple = llvm::sys::getDefaultTargetTriple();
	llvm::IntrusiveRefCntPtr<TargetInfo>
	 pti(TargetInfo::CreateTargetInfo(compiler.getDiagnostics(),
		                              pto.getPtr()));
	compiler.setTarget(pti.getPtr());

	//FileManager,SourceManager 以及heaDREearch的Options的设置
	compiler.createFileManager();
	compiler.createSourceManager(compiler.getFileManager());

	HeaderSearchOptions &headerSearchOptions = compiler.getHeaderSearchOpts();
	headerSearchOptions.AddPath("/usr/include",
          clang::frontend::Angled,
          false,
          false);
	headerSearchOptions.AddPath("/usr/include/c++",
		  clang::frontend::Angled,
		  false,
		  false);
    headerSearchOptions.AddPath("/usr/include/i386-linux-gnu",
          clang::frontend::Angled,
          false,
          false);
	headerSearchOptions.AddPath("/usr/include/i386-linux-gnu/sys",
          clang::frontend::Angled,
          false,
          false);

    headerSearchOptions.AddPath("/usr/local/lib/clang/3.5.0/include",
          clang::frontend::Angled,
          false,
          false);

    if(!addHeaderPath(headerSearchOptions)){
        #ifdef DEBUG
        llvm::errs()<<"fail to open config file\n";
        #endif
    }

    

    
/*
    headerSearchOptions.AddPath("/usr/include",
          clang::frontend::Angled,
          false,
          false);
	*/
	//langOptions设置，要传给rewriter
   	LangOptions langOpts;
	langOpts.GNUMode = 1; 
	langOpts.CXXExceptions = 1; 
	langOpts.RTTI = 1; 
	langOpts.Bool = 1; 
	langOpts.CPlusPlus = 1; 
	Invocation->setLangDefaults(langOpts, clang::IK_CXX,clang::LangStandard::lang_cxx0x);

	//create PP
	compiler.createPreprocessor();//(TU_Complete);//
	compiler.getPreprocessorOpts().UsePredefines = false;
	//createASTContext
	compiler.createASTContext();
  
 	//set the sourceManager for rewriter 

	
	rewrite.setSourceMgr(compiler.getSourceManager(), compiler.getLangOpts());
	
	//插装文件入口

	const FileEntry *pFile = compiler.getFileManager().getFile(fileName);
	compiler.getSourceManager().createMainFileID(pFile);
	compiler.getDiagnosticClient().BeginSourceFile(compiler.getLangOpts(),&compiler.getPreprocessor());
	                                        
	
	MyASTConsumer astConsumer(rewrite);
	//将.c转成_out.c
	// Convert <file>.c to <file_out>.c
	std::string outName (fileName);
	/*size_t ext = outName.rfind(".");
	//根据有没有找到‘。’来决定在哪里加入_out
	if (ext == std::string::npos)
		ext = outName.length();
	outName.insert(ext, "_out");
	*/
	outName.insert(outName.length(),"_out");
	llvm::errs() << "Output to: " << outName << "\n";
	
	std::string OutErrorInfo;
	//新建输入到新文件的流
	llvm::raw_fd_ostream outFile(outName.c_str(), OutErrorInfo);//,llvm::sys::fs::F_None);//版本问题//////


	//----此块基本一样    end----

	if (OutErrorInfo.empty()){
		// Parse the AST
		//用PP，astConsumer，ASTContext来解释AST
		ParseAST(compiler.getPreprocessor(), &astConsumer, compiler.getASTContext());
		compiler.getDiagnosticClient().EndSourceFile();
    
		//建立ClangTool 
        CommonOptionsParser OptionsParser(argc, argv);//, MyToolCategory);
        ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());
		//开始匹配             
        
        /*//for test
        TestPrinter testPrinter;
        MatchFinder testFinder;
        testFinder.addMatcher(TestMatcher, &testPrinter);
        Tool.run(newFrontendActionFactory(&testFinder));
        
        Test2Printer test2Printer;
        MatchFinder test2Finder;
        test2Finder.addMatcher(Test2Matcher, &test2Printer);
        Tool.run(newFrontendActionFactory(&test2Finder));
        */
        IfPrinter ifPrinter;
        MatchFinder ifFinder;
        ifFinder.addMatcher(IfMatcher, &ifPrinter);
        Tool.run(newFrontendActionFactory(&ifFinder));


        ArraySubRefPrinter arraySubRefPrinter;
        MatchFinder arraySubRefFinder;
        arraySubRefFinder.addMatcher(ArraySubRefMatcher,&arraySubRefPrinter);
        Tool.run(newFrontendActionFactory(&arraySubRefFinder));
        
        ArraySubPrinter arraySubPrinter;
        MatchFinder arraySubFinder;
        arraySubFinder.addMatcher(ArraySubMatcher,&arraySubPrinter);
        Tool.run(newFrontendActionFactory(&arraySubFinder));
        
        
        
        MallocVarPrinter mallocVarPrinter;
        MatchFinder mallocVarFinder;
        mallocVarFinder.addMatcher(MallocVarMatcher, &mallocVarPrinter);
        Tool.run(newFrontendActionFactory(&mallocVarFinder));

        MallocPrinter mallocPrinter;
        MatchFinder mallocFinder;
        mallocFinder.addMatcher(MallocMatcher, &mallocPrinter);
        Tool.run(newFrontendActionFactory(&mallocFinder));
        
        Malloc2Printer malloc2Printer;
        MatchFinder malloc2Finder;
        malloc2Finder.addMatcher(Malloc2Matcher, &malloc2Printer);
        Tool.run(newFrontendActionFactory(&malloc2Finder));
        

       
        
        FreeVarPrinter freeVarPrinter;
        MatchFinder freeVarFinder;
        freeVarFinder.addMatcher(FreeVarMatcher, &freeVarPrinter);
        Tool.run(newFrontendActionFactory(&freeVarFinder));
        
        FreePrinter freePrinter;
        MatchFinder freeFinder;
        freeFinder.addMatcher(FreeMatcher, &freePrinter);
        Tool.run(newFrontendActionFactory(&freeFinder));
   
    
                  
    	const RewriteBuffer *RewriteBuf =rewrite.getRewriteBufferFor(compiler.getSourceManager().getMainFileID());
		
        if(RewriteBuf != NULL){
            #ifdef DEBUG
            llvm::errs() << " RewriteBuf not NULL \n";
			//在文件头加上改头文件,防止没有 stdlib,stdio 而不能使用printf和exit函数
		    #endif
			outFile << "#include\"plugHead.h\"\n";
            
            outFile << std::string(RewriteBuf->begin(), RewriteBuf->end());		
        }else{
            #ifdef DEBUG
            llvm::errs() << " RewriteBuf is NULL \n";
			#endif

        	outFile << "#include\"plugHead.h\"\n";
            std::ifstream infile(fileName.c_str());
            if(!infile){
                llvm::errs() << " fail to open the input file!\n";
                exit(-1);                
            }
            std::string str_in;
            while(std::getline(infile,str_in)){
                outFile << str_in <<"\n";
            }
        
        }
        outFile.close();


        #ifdef DEBUG        
        std::string checkStructErrorInfo;
        std::string checkStructFileName = "checkStruct.txt";
        //新建输入到新文件的流,将已经找到的malloc过的结构体信息写入文件,供另一个处理程序读取
        llvm::raw_fd_ostream csFile(checkStructFileName.c_str(),checkStructErrorInfo);//,llvm::sys::fs::F_None);
        if (checkStructErrorInfo.empty()){
      	    for(unsigned int i=0;i<vec_cp.size();++i){            
    	        csFile << vec_cp[i].name << " " << vec_cp[i].row << " " << vec_cp[i].col << " " << vec_cp[i].declName << " " << vec_cp[i].declRow << " " << vec_cp[i].declCol << "\n" ;
	        }            
        }
	    csFile.close();  
        for(unsigned int i=0;i<vec_cp.size();++i){
	        llvm::errs()<<vec_cp[i].name<<"|"<<vec_cp[i].declName<<":"<<vec_cp[i].declRow<<":"<<vec_cp[i].declCol<<"\n";  
	    }	
        #endif
	    
	}
	else{
		llvm::errs() << "Cannot open " << outName << " for writing\n";
	}
	

      
    return 0;
}

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

typedef struct checkPoint{
    //std::string flagName;
    std::string name;//插入点的变量
    int row,col;//行号,列号
    std::string declName;//原先定义的位置,先保留不用
    int declRow,declCol;   
}checkPoint;
std::vector<checkPoint> cpVec;//存malloc时得到的点
std::vector<checkPoint> cpVecF;//存free时得到的点,临时使用


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

//匹配到free()的表达式                                                
StatementMatcher FreeMatcher = callExpr(has(declRefExpr(to(functionDecl(hasName("free")))))).bind("free");
//匹配到free()里的变量
StatementMatcher FreeVarMatcher = declRefExpr(hasParent(implicitCastExpr(hasParent(implicitCastExpr(hasParent(callExpr(has(declRefExpr(to(functionDecl(hasName("free")))))))))))).bind("freeVar");                



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
        
        for(unsigned i=0;i<cpVec.size();++i){
            if(cpVec[i].row == cp.row){
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
        "\tfclose(fp);\n\n" +
        "\tint fd=open(FIFO_SERVER,O_WRONLY |O_NONBLOCK,0);\n" + 
        "\tif(fd==-1){perror(\"open\");exit(1);}\n" + 
        "\tchar w_buf[100];\n" + 
        "\tsprintf(w_buf,\"m "+ cp.name + " " + buf[0] + " " + buf[1] + " %x \\n\"," + cp.name + ");\n" + 
        "\tif(write(fd,w_buf,100)==-1){\n" + 
        "\t\tif(errno==EAGAIN)\n" + 
        "\t\t\tprintf(\"The FIFO has not been read yet.Please try later\\n\");\n" + 
        "\t}\n" + 
        "\telse\n" + 
        "\t\tprintf(\"write %s to the FIFO\\n\",w_buf);\n" +
        "\tsleep(1);\n" + 
        "\tclose(fd);\n" + 
        "}\n";
        
        
        
        //llvm::errs() << "-----\n"<<str_insert<<"\n----\n";
		//找位置插装
        int locOffset = 2;
        SourceLocation SL_locWithOffset = locEnd.getLocWithOffset(locOffset);
        rewrite.InsertText(SL_locWithOffset,str_insert.c_str(),true,true); 
        
		if(!findFlag){        
            cpVec.push_back(cp);
        }

        #ifdef DEBUG
        llvm::errs()<<"----BinaryOperator(malloc) end----\n";
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
        for(unsigned i=0;i<cpVecF.size();++i){
            if(cp.name == cpVecF[i].name &&
             cp.row == cpVecF[i].row){
                findFlag = true;
                cp.col = cpVecF[i].col;
                cp.declName = cpVecF[i].declName;
                cp.declRow = cpVecF[i].declRow;
                cp.declCol = cpVecF[i].declCol;
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


        
        cpVecF.push_back(cp);
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

        
        cpVec.push_back(cp);
        
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
	headerSearchOptions.AddPath("/usr/include/c++",
		  clang::frontend::Angled,
		  false,
		  false);
	
    headerSearchOptions.AddPath("/usr/local/lib/clang/3.5.0/include",
          clang::frontend::Angled,
          false,
          false);
    headerSearchOptions.AddPath("/usr/include/i386-linux-gnu",
          clang::frontend::Angled,
          false,
          false);
    headerSearchOptions.AddPath("/usr/include",
          clang::frontend::Angled,
          false,
          false);
	
    
    
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
        
        MallocVarPrinter mallocVarPrinter;
        MatchFinder mallocVarFinder;
        mallocVarFinder.addMatcher(MallocVarMatcher, &mallocVarPrinter);
        Tool.run(newFrontendActionFactory(&mallocVarFinder));
        
        MallocPrinter mallocPrinter;
        MatchFinder mallocFinder;
        mallocFinder.addMatcher(MallocMatcher, &mallocPrinter);
        Tool.run(newFrontendActionFactory(&mallocFinder));
        
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
      	    for(unsigned int i=0;i<cpVec.size();++i){            
    	        csFile << cpVec[i].name << " " << cpVec[i].row << " " << cpVec[i].col << " " << cpVec[i].declName << " " << cpVec[i].declRow << " " << cpVec[i].declCol << "\n" ;
	        }            
        }
	    csFile.close();  
        for(unsigned int i=0;i<cpVec.size();++i){
	        llvm::errs()<<cpVec[i].name<<"|"<<cpVec[i].declName<<":"<<cpVec[i].declRow<<":"<<cpVec[i].declCol<<"\n";  
	    }	
        #endif
	    
	}
	else{
		llvm::errs() << "Cannot open " << outName << " for writing\n";
	}
	

      
    return 0;
}

/***   libClangtest.cpp   ******************************************************

   
 *****************************************************************************/
//#define DEBUG
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <vector>

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

#define ARRAYINFONUM 64

using namespace clang;
Rewriter rewrite;

class MyASTConsumer : public ASTConsumer
{
 public:
	MyASTConsumer(Rewriter &Rewrite){
	}
};
#define CONSTANT_ARRAY 1
#define INCOMPLETE_ARRAY 2
#define VARIABLE_ARRAY 3
//存放数组名字和边界的信息
typedef struct arrayInfo{
	std::string name;
	int bound;
	int type; 
	std::vector<int> vbd;
	arrayInfo(){}
	arrayInfo(const std::string &n,const int b,const int c,std::vector<int>d):name(n),bound(b),type(c),vbd(d){}
}arrayInfo;
std::vector<arrayInfo> arrayInfoSet;

std::string str_insert_head = "#include \"plugHead.h\"\n";
std::string logName = "checkLog2.txt";
CXFile CXFileName;


unsigned int prevIfSt = 0;
unsigned int prevIfEd = 0;


//访问CXCursor的函数，根据返回值来确定 递归CXCursor里的CXCursor/访问下一个CXCursor/终止
CXChildVisitResult visitor(CXCursor cursor,CXCursor parent,CXClientData clientData){

	CXFile file;
	unsigned int line;
	unsigned int column;
	unsigned int offset;

	
	CXSourceLocation loc = clang_getCursorLocation(cursor);
	//得到当前访问的CXCursor 的file,line,column,offset
	clang_getFileLocation(loc,&file,&line,&column,&offset);
	
	CXTranslationUnit TU = clang_Cursor_getTranslationUnit(cursor);
	CXSourceRange range = clang_getCursorExtent(cursor);
	
	//将CXSourceLocation转化为SourceLocation
	clang::SourceLocation sloc = sloc.getFromRawEncoding(loc.int_data);
	
	CXToken* tokens;
	unsigned int numTokens;
	clang_tokenize(TU,range,&tokens,&numTokens);//all the tokens in this cursor are saved in "tokens"

    //rewrite null string to avoid the segmentation fault
    CXSourceLocation linenullcxsloc = clang_getLocation(TU,file,line,1);
	clang::SourceLocation linenullsloc = linenullsloc.getFromRawEncoding(linenullcxsloc.int_data);
	std::string nullStr = "";
	rewrite.InsertText(linenullsloc,nullStr.c_str(),true,true); 
	
	
	if(CXFileName != file)
	    return CXChildVisit_Continue;
    #ifdef DEBUG
    llvm::errs()<<"---------part-----------\n";
    llvm::errs()<<"cursorKind "<<clang_getCursorKind(cursor)<<"\n";
    
    llvm::errs()<<"numtokens:"<<numTokens<<"\n";
    for(int i=0;i<numTokens-1;++i){
				std::string token = clang_getCString(clang_getTokenSpelling(TU,tokens[i]));
				llvm::errs()<<token.c_str()<<"|";
	}
    llvm::errs()<<"\n---------part-----------\n";
    #endif
    
	if(clang_Cursor_isNull(cursor) != 0){

		return CXChildVisit_Continue;//访问下一个CXCursor
	}

	if(clang_getCursorKind(cursor) == CXCursor_BinaryOperator){//二元运算
    	#ifdef DEBUG
		llvm::errs()<<"CXCursor_BinaryOperator\n";
    	#endif
		std::string divisorStr;
		//确定是除号
		int i;
		bool flag = false;

		for(i=0;i<numTokens-1;++i){
			if(strcmp(clang_getCString(clang_getTokenSpelling(TU,tokens[i])),"/")==0){
				flag = true;
				break;
			}
		}
	    
		if(flag){	
    		
    		#ifdef DEBUG		
    		llvm::errs()<<"CXCursor_BinaryOperator tokens\n";
		    if(numTokens > 0){
			    for(int j=0;j<numTokens-1;++j){
				    std::string token = clang_getCString(clang_getTokenSpelling(TU,tokens[j]));
				    llvm::errs()<<token.c_str()<<"|";
			    }
			    llvm::errs() << "\n";
		    }
	    	#endif
	    	
			for(++i;i<numTokens-1;++i){
		        std::string token = clang_getCString(clang_getTokenSpelling(TU,tokens[i]));
		        if(strcmp(token.c_str(),"++") == 0 ){
		            std::string tokenTemp = clang_getCString(clang_getTokenSpelling(TU,tokens[i-1]));
		            if(strcmp(tokenTemp.c_str(),"/")==0||strcmp(tokenTemp.c_str(),"+")==0||strcmp(tokenTemp.c_str(),"-")==0||strcmp(tokenTemp.c_str(),"*")==0||
		            strcmp(tokenTemp.c_str(),"(")==0){//++a
		                std::string tokenNext = clang_getCString(clang_getTokenSpelling(TU,tokens[i+1]));
		                divisorStr += "(1+"+ tokenNext +")";
		                ++i;
		            }else{//a++
		                continue;
		            }
		        }else if(strcmp(token.c_str(),"--") == 0 ){
		            std::string tokenTemp = clang_getCString(clang_getTokenSpelling(TU,tokens[i-1]));
		            if(strcmp(tokenTemp.c_str(),"/")==0||strcmp(tokenTemp.c_str(),"+")==0||strcmp(tokenTemp.c_str(),"-")==0||strcmp(tokenTemp.c_str(),"*")==0||
		            strcmp(tokenTemp.c_str(),"(")==0){//--a
		                std::string tokenNext = clang_getCString(clang_getTokenSpelling(TU,tokens[i+1]));
		                divisorStr += "("+ tokenNext +"-1)";
		                ++i;
		            }else{//a--
		                continue;
		            }
		        }else{ 
    				divisorStr += token;
				}
			}
			
	    	
			char lineStr[32],columnStr[32];//将unsigned转化成char*
			sprintf(lineStr,"%u",line);
			sprintf(columnStr,"%u",column);
			//除0判断语句
			std::string judgeStr = 
			"\nif("+divisorStr+" == 0){\n" +
            "\tprintf(\""+lineStr+":"+columnStr+": divide 0!"+"("+divisorStr+")\\n\");\n" +


            "\t\n\t{\n\t\tFILE *fp = fopen(\"" + logName +"\",\"a\");\n" +
            "\t\tif(fp == NULL){\n" +
            "\t\t\tprintf(\"fail to open file " + logName + "!\\n\");\n" +
            "\t\t\texit(-1);\n" + 
            "\t\t}\n" + 
            "\t\tfputs(\""+lineStr+":"+columnStr+": divide 0!"+"("+divisorStr+")\\n\",fp);\n"  +
            "\t\tfclose(fp);\n" +
            "\t}\n"+
            //"\texit(-1);\n" +
            "}\n";
            
			//最后一个参数传列号为1，保证插桩在行头
			//CXCursor Cursor_seParent = clang_getCursorSemanticParent(cursor);
        //if(clang_getCursorKind(clang_getCursorSemanticParent(Cursor_seParent)) ==  CXCursor_FirstInvalid)
			CXSourceLocation linecxsloc = clang_getLocation(TU,file,line,1);
			clang::SourceLocation linesloc = linesloc.getFromRawEncoding(linecxsloc.int_data);
			//插入到rewrite中 最后两个参数表示 bool InsertAfter=true, bool indentNewLines=true
			rewrite.InsertText(linesloc,judgeStr.c_str(),true,true); 
			
			
		  
			return CXChildVisit_Continue;//这个CXCursor访问结束
		}
		
	}
	
	// /=
	if(clang_getCursorKind(cursor) == CXCursor_CompoundAssignOperator){
	    #ifdef DEBUG
	    llvm::errs()<<"visit CompoundAssignOperator\n";
	    #endif
	    
	    std::string divisorStr;
		//确定是/=
		int i;
		bool flag = false;
		for(i=numTokens-2;i>=0 ;--i){
			if(strcmp(clang_getCString(clang_getTokenSpelling(TU,tokens[i])),"/=")==0){
				flag = true;
				break;
			}
		}
		
		if(flag){
			for(++i;i<numTokens-1;++i){
		        std::string token = clang_getCString(clang_getTokenSpelling(TU,tokens[i]));
		        if(strcmp(token.c_str(),"++") == 0 ){
		            std::string tokenTemp = clang_getCString(clang_getTokenSpelling(TU,tokens[i-1]));
		            if(strcmp(tokenTemp.c_str(),"/")==0||strcmp(tokenTemp.c_str(),"+")==0||strcmp(tokenTemp.c_str(),"-")==0||strcmp(tokenTemp.c_str(),"*")==0||
		            strcmp(tokenTemp.c_str(),"(")==0){//++a
		                std::string tokenNext = clang_getCString(clang_getTokenSpelling(TU,tokens[i+1]));
		                divisorStr += "(1+"+ tokenNext +")";
		                ++i;
		            }else{//a++
		                continue;
		            }
		        }else if(strcmp(token.c_str(),"--") == 0 ){
		            std::string tokenTemp = clang_getCString(clang_getTokenSpelling(TU,tokens[i-1]));
		            if(strcmp(tokenTemp.c_str(),"/")==0||strcmp(tokenTemp.c_str(),"+")==0||strcmp(tokenTemp.c_str(),"-")==0||strcmp(tokenTemp.c_str(),"*")==0||
		            strcmp(tokenTemp.c_str(),"(")==0){//--a
		                std::string tokenNext = clang_getCString(clang_getTokenSpelling(TU,tokens[i+1]));
		                divisorStr += "("+ tokenNext +"-1)";
		                ++i;
		            }else{//a--
		                continue;
		            }
		        }else{ 
    				divisorStr += token;
				}
			}
		
			char lineStr[32],columnStr[32];//将unsigned转化成char*
			sprintf(lineStr,"%u",line);
			sprintf(columnStr,"%u",column);
			//除0判断语句
			std::string judgeStr = 
			"\nif("+divisorStr+" == 0){\n" +
            "\tprintf(\""+lineStr+":"+columnStr+": divide 0!"+"("+divisorStr+")\\n\");\n" +


            "\t\n\t{\n\t\tFILE *fp = fopen(\"" + logName +"\",\"a\");\n" + 
            "\t\tif(fp == NULL){\n" +
            "\t\t\tprintf(\"fail to open file " + logName + "!\\n\");\n" +
            "\t\t\texit(-1);\n" + 
            "\t\t}\n" + 
            "\t\tfputs(\""+lineStr+":"+columnStr+": divide 0!"+"("+divisorStr+")\\n\",fp);\n"  +
            "\t\tfclose(fp);\n" +
            "\t}\n"+
            //"\texit(-1);\n" +
            "}\n";
			//最后一个参数传列号为1，保证插桩在行头
			if(prevIfSt<= line && line <= prevIfEd)// if this expr is in IfStmt
                line = prevIfSt;	
			CXSourceLocation linecxsloc = clang_getLocation(TU,file,line,1);
			clang::SourceLocation linesloc = linesloc.getFromRawEncoding(linecxsloc.int_data);
			//插入到rewrite中 最后两个参数表示 bool InsertAfter=true, bool indentNewLines=true
			
			rewrite.InsertText(linesloc,judgeStr.c_str(),true,true); 
			return CXChildVisit_Continue;//这个CXCursor访问结束
		}
	}
	
	
   
    if(clang_getCursorKind(cursor) == CXCursor_IfStmt){//防止 数组下标的判定插入错误,同时如果没有{}需要加上

        prevIfSt = line;
        int parenCnt = 0;
        int i=0;
        for(;i<numTokens-1;++i){
				std::string token = clang_getCString(clang_getTokenSpelling(TU,tokens[i]));		
				if(strcmp(token.c_str(),"(") == 0){parenCnt = 1;break;}

		}
		for(++i;i<numTokens-1;++i){
    		std::string token = clang_getCString(clang_getTokenSpelling(TU,tokens[i]));		
		    if(parenCnt == 0)break;
		    if(strcmp(token.c_str(),"(") == 0){++parenCnt;}
		    else if(strcmp(token.c_str(),")") == 0){--parenCnt;}
		}
		--i;
		std::string token = clang_getCString(clang_getTokenSpelling(TU,tokens[i+1]));
		#ifdef DEBUG
		llvm::errs()<<"check{ :"<<token<<"\n";
		#endif
		if(strcmp(token.c_str(),"{") != 0){
		    #ifdef DEBUG
		    llvm::errs()<<"insert {}\n";
		    #endif 
		    
		    
		    CXSourceLocation cxslBraceSt = clang_getTokenLocation(TU,tokens[i+1]);
		    
            clang::SourceLocation slBraceSt = slBraceSt.getFromRawEncoding(cxslBraceSt.int_data);
		    rewrite.InsertText(slBraceSt,"{",true,true);
		    int j=i;
		    bool elseFlag = false;
		    for(;j<numTokens-1;++j){//else exists
		        std::string token1 = clang_getCString(clang_getTokenSpelling(TU,tokens[j]));		
				if(strcmp(token1.c_str(),"else") == 0){elseFlag = true;break;}
		    }
		    if(!elseFlag){
		        CXSourceLocation cxslBraceEd = clang_getTokenLocation(TU,tokens[numTokens-2]);
		        unsigned int tpCol,tpOffset,tpRow;
                CXFile tpCXFile;
                clang_getFileLocation(cxslBraceEd,&tpCXFile,&tpRow,&tpCol,&tpOffset);
            
            
		        CXSourceLocation cxslBraceEd2 = clang_getLocation(TU,tpCXFile,tpRow+1,1);
                clang::SourceLocation slBraceEd = slBraceEd.getFromRawEncoding(cxslBraceEd2.int_data);
		        rewrite.InsertText(slBraceEd,"}",true,true);
	        }else{
	            
	            CXSourceLocation cxslBraceEd = clang_getTokenLocation(TU,tokens[j]);//if's "}"
	            clang::SourceLocation slBraceEd = slBraceEd.getFromRawEncoding(cxslBraceEd.int_data);
	            rewrite.InsertText(slBraceEd,"}",true,true);
	            
                std::string token1 = clang_getCString(clang_getTokenSpelling(TU,tokens[j+1]));		
                #ifdef DEBUG
	            llvm::errs()<<"else j:"<<j<<"token:"<<token1<<"\n";
	            #endif
	            if(strcmp(token1.c_str(),"{") != 0){//else does not have {}}
                    CXSourceLocation cxslBraceStElse = clang_getTokenLocation(TU,tokens[j+1]);
	                clang::SourceLocation slBraceStElse = slBraceStElse.getFromRawEncoding(cxslBraceStElse.int_data);
	                rewrite.InsertText(slBraceStElse,"{",true,true);
	                
	                CXSourceLocation cxslBraceEdElse = clang_getTokenLocation(TU,tokens[numTokens-1]);
		            unsigned int tpCol,tpOffset,tpRow;
                    CXFile tpCXFile;
                    clang_getFileLocation(cxslBraceEdElse,&tpCXFile,&tpRow,&tpCol,&tpOffset);
                    
                    #ifdef DEBUG
                    llvm::errs()<<"else }after:"<<(clang_getCString(clang_getTokenSpelling(TU,tokens[numTokens-1])))<<"\n";
                    llvm::errs()<<"tpRow:"<<tpRow<<" tpCol"<<tpCol<<"\n";
                    #endif
                    
                    
		            CXSourceLocation cxslBraceEd2Else = clang_getLocation(TU,tpCXFile,tpRow+1,1) ;
                    clang::SourceLocation slBraceEdElse = slBraceEdElse.getFromRawEncoding(cxslBraceEd2Else.int_data);
		            rewrite.InsertText(slBraceEdElse,"}",true,true);
	            
	            }
	        
	        }  
		}
		
		
		
		
		CXSourceLocation cxslIfEd = clang_getTokenLocation(TU,tokens[i]);
        unsigned int tempCol,tempOffset;
        CXFile tempCXFile;
        clang_getFileLocation(cxslIfEd,&tempCXFile,&prevIfEd,&tempCol,&tempOffset);
        
        #ifdef DEBUG
	    llvm::errs()<<"line start: "<<prevIfSt<<" end: "<<prevIfEd<<"\n";
	    #endif
	    
	    
    }
 
	//处理函数sqrt系列
	if(clang_getCursorKind(cursor) == CXCursor_CallExpr){
	    #ifdef DEBUG
	    llvm::errs()<<"visit CXCursor_CallExpr\n";  
	    if(numTokens > 0){
			for(int i=0;i<numTokens-1;++i){
				std::string token = clang_getCString(clang_getTokenSpelling(TU,tokens[i]));
				llvm::errs()<<token.c_str()<<"\n";
			}
		}
        #endif
		
		std::string str_check = "";
		std::string str_func_name = clang_getCString(clang_getTokenSpelling(TU,tokens[0]));
		//sqrt
        if(strcmp(str_func_name.c_str(),"sqrt") == 0 || strcmp(str_func_name.c_str(),"sqrtf") == 0 || strcmp(str_func_name.c_str(),"sqrtl") == 0){
            
            
			//处理++ --
			for(int i=1;i<numTokens-1;++i){
		        std::string token = clang_getCString(clang_getTokenSpelling(TU,tokens[i]));
		        if(strcmp(token.c_str(),"++") == 0 ){
		            std::string tokenTemp = clang_getCString(clang_getTokenSpelling(TU,tokens[i-1]));
		            if(strcmp(tokenTemp.c_str(),"/")==0||strcmp(tokenTemp.c_str(),"+")==0||strcmp(tokenTemp.c_str(),"-")==0||strcmp(tokenTemp.c_str(),"*")==0||
		            strcmp(tokenTemp.c_str(),"(")==0){//++a
		                std::string tokenNext = clang_getCString(clang_getTokenSpelling(TU,tokens[i+1]));
		                str_check += "(1+"+ tokenNext +")";
		                ++i;
		            }else{//a++
		                continue;
		            }
		        }else if(strcmp(token.c_str(),"--") == 0 ){
		            std::string tokenTemp = clang_getCString(clang_getTokenSpelling(TU,tokens[i-1]));
		            if(strcmp(tokenTemp.c_str(),"/")==0||strcmp(tokenTemp.c_str(),"+")==0||strcmp(tokenTemp.c_str(),"-")==0||strcmp(tokenTemp.c_str(),"*")==0||
		            strcmp(tokenTemp.c_str(),"(")==0){//--a
		                std::string tokenNext = clang_getCString(clang_getTokenSpelling(TU,tokens[i+1]));
		                str_check += "("+ tokenNext +"-1)";
		                ++i;
		            }else{//a--
		                continue;
		            }
		        }else{ 
    				str_check += token;
				}
			}
			

    
			
			char lineStr[32],columnStr[32];

    		sprintf(lineStr,"%u",line);
	    	sprintf(columnStr,"%u",column);
	    	CXSourceLocation linecxsloc = clang_getLocation(TU,file,line,1);
            clang::SourceLocation linesloc = linesloc.getFromRawEncoding(linecxsloc.int_data);
	    	str_check = "\nif(" + str_check + " < 0){\n" + 
	    	+"\tprintf(\""+lineStr+":"+columnStr+":"+str_check+"<0 , cannot be squared root!\\n\");\n" + 
	    	"\t\n\t{\n\t\tFILE *fp = fopen(\"" + logName +"\",\"a\");\n" + 
            "\t\tif(fp == NULL){\n" +
            "\t\t\tprintf(\"fail to open file " + logName + "!\\n\");\n" +
            "\t\t\texit(-1);\n" + 
            "\t\t}\n" + 
            "\t\tfputs(\""+lineStr+":"+columnStr+":"+str_check+"<0 , cannot be squared root!\\n\",fp);\n"  +
            "\t\tfclose(fp);\n" +
            "\t}\n"+
            
            //"\texit(-1);\n" + 
            "}\n";
            
		    rewrite.InsertText(linesloc,str_check.c_str(),true,true); 
		    return CXChildVisit_Continue;
		    
		}
		
        return CXChildVisit_Recurse;
	}
		
	
	if(numTokens > 0){//调试时输出所有tokens
			#ifdef DEBUG
			for(int i=0;i<numTokens-1;++i){
				std::string token = clang_getCString(clang_getTokenSpelling(TU,tokens[i]));
				llvm::errs()<<token.c_str()<<"\n";
			}
			#endif
			return CXChildVisit_Recurse;
			
	}	

	return CXChildVisit_Continue;
}
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
int main(int argc, char *argv[]) {
	if(argc < 2){
		llvm::errs()<<"Usage: ./libClangtest <fileName>\n";
		return -1;
	}
	
	
	//CXCursor operation
	CXIndex Index = clang_createIndex(0, 0);
	CXTranslationUnit TU = clang_parseTranslationUnit(Index, 0, argv, argc, 0,0, CXTranslationUnit_None);
    CXFileName =  clang_getFile(TU,argv[1]);
	
	CXCursor cxc = clang_getTranslationUnitCursor(TU);	


	CXClientData cxda;
	//end of CXCursor operation
	
	//set compilerinstance for rewriter		
	std::string fileName(argv[argc - 1]);
	CompilerInstance compiler;
	DiagnosticOptions diagnosticOptions;
	compiler.createDiagnostics();


	//invocation可以传递任何flag给preprocessor
	CompilerInvocation *Invocation = new CompilerInvocation;
	CompilerInvocation::CreateFromArgs(*Invocation, argv + 1, argv + argc,compiler.getDiagnostics());
	compiler.setInvocation(Invocation);


	//建立TargetOptions和TargetInfo,并设置好Target
	// Set default target triple
	llvm::IntrusiveRefCntPtr<TargetOptions> pto( new TargetOptions());
	pto->Triple = llvm::sys::getDefaultTargetTriple();
	llvm::IntrusiveRefCntPtr<TargetInfo>
	 pti(TargetInfo::CreateTargetInfo(compiler.getDiagnostics(),
		                              pto.getPtr()));
	compiler.setTarget(pti.getPtr());

	//FileManager,SourceManager 以及headSearch的Options的设置
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

 
    headerSearchOptions.AddPath("/usr/include",
          clang::frontend::Angled,
          false,
          false);
    
    if(!addHeaderPath(headerSearchOptions)){
       // #ifdef DEBUG
        llvm::errs()<<"fail to open config file\n";
       // #endif
    }
    
 /*   headerSearchOptions.AddPath("/usr/local/ssl/include",
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
	compiler.createPreprocessor();
	compiler.getPreprocessorOpts().UsePredefines = false;
	//createASTContext
	compiler.createASTContext();
  
 	//set the sourceManager for rewriter 
	rewrite.setSourceMgr(compiler.getSourceManager(), compiler.getLangOpts());
	
	//插桩文件入口
	const FileEntry *pFile = compiler.getFileManager().getFile(fileName);
	compiler.getSourceManager().createMainFileID(pFile);
	compiler.getDiagnosticClient().BeginSourceFile(compiler.getLangOpts(),&compiler.getPreprocessor());
		                                        
	MyASTConsumer astConsumer(rewrite);
	//将.c转成_out.c
	// Convert <file>.c to <file_out>.c
	std::string outName (fileName);

	outName.insert(outName.size(), "_out");
	llvm::errs() << "Output to: " << outName << "\n";
	std::string OutErrorInfo;
	//新建输入到新文件的流
	llvm::raw_fd_ostream outFile(outName.c_str(), OutErrorInfo);//, llvm::sys::fs::F_None);
	if (OutErrorInfo.empty()){
		// Parse the AST
		//用PP，astConsumer，ASTContext来解释AST
		ParseAST(compiler.getPreprocessor(), &astConsumer, compiler.getASTContext());
		compiler.getDiagnosticClient().EndSourceFile();

		//visit children to insert,主要是为了这个函数
		clang_visitChildren(cxc,visitor,cxda);
        
		const RewriteBuffer *RewriteBuf =
		rewrite.getRewriteBufferFor(compiler.getSourceManager().getMainFileID());
		outFile << str_insert_head.c_str();
		outFile << std::string(RewriteBuf->begin(), RewriteBuf->end());
		
	}
	else{
		llvm::errs() << "Cannot open " << outName << " for writing\n";
	}  
	#ifdef DEBUG 
    llvm::errs()<<"array name and size\n";
    for(int i=0;i<arrayInfoSet.size();++i){
		    llvm::errs()<<arrayInfoSet[i].name<<" "<<arrayInfoSet[i].bound<<"\n";
		    for(int j=0;j<arrayInfoSet[i].vbd.size();++j)
		        llvm::errs()<<arrayInfoSet[i].vbd[j]<<"|";
		    llvm::errs()<<"\n";
    }
    #endif
	outFile.close();


	//dispose the resources
	clang_disposeTranslationUnit(TU);
	clang_disposeIndex(Index);
	return 0;
}


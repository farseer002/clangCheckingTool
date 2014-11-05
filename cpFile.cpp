# include <iostream>
# include <string>
# include <fstream>
# include <cstring>
# include <set>

using namespace std;
std::string saveFileName="allFileName.txt";
char notUsedFile[]="plugHead.h";
set<string> st;
int Copy(const string a, const string b);
void CopyFile(const string &file, const string &filePath){
	std::string fileName;
	char fileBuffer[81];
	ofstream osfn;
	ofstream outputFile;
	ifstream inputHeadFile;
//	int count = 1;
//	int fileSize = file.size();
	
	/*while(count <= (fileSize - 2)){
		fileName = fileName + file[count];	
		count++;
	}*/
	fileName = file;
	if(strcmp(fileName.c_str(),"") != 0){
	    inputHeadFile.open(fileName.c_str(), ios::in);
	    if(inputHeadFile.fail()){
		    cout <<  " headFile:  " << fileName << "  Open fail" << endl;
		    return ;
	    }
	}
	
	osfn.open(saveFileName.c_str(), ios::app);
	if(osfn.fail()){
		cout << saveFileName << "Open fail" << endl;
		return ;
	}
	
	cout << " headFile:" << fileName << "   Open success" << endl;
	
	if(!st.count(fileName)){
	    if(strcmp(fileName.c_str(),notUsedFile) != 0){
	        st.insert(fileName);

        	osfn << fileName << endl;
	        cout<<"osfn:"<<fileName<<endl;
	        
	        fileName = filePath + fileName;
	
	        outputFile.open(fileName.c_str(), ios::out);
	        if(outputFile.fail()){
		        cout << fileName << "Open fail" << endl;
		        return ;
	        }
	
	        //inputHeadFile.getline(fileBuffer,81);
	        std::string fb;
	        while(std::getline(inputHeadFile,fb)){
	            outputFile<<fb<<endl;
	        }
	        /*while(!inputHeadFile.eof()){
	          outputFile << fileBuffer << "\n";
	          inputHeadFile.getline(fileBuffer,81);
	        }
	        outputFile << fileBuffer << "\n";
	        */
            inputHeadFile.close();
	        outputFile.close();
	        osfn.close();
	
	        cout << fileName << "    " << filePath << endl; 
            Copy(fileName, filePath);
	    }
	    
    }else{
        inputHeadFile.close();
	    outputFile.close();
	    osfn.close();
	
    }
}


int Copy(const string copiedFileName, const string filePath){
	ifstream inputFile;
	string fileBuffer;

	inputFile.open(copiedFileName.c_str(), ios::in);
	if(inputFile.fail()){
		cout << copiedFileName << "  open  fail" << endl;
		return 0;
	}
		
		
    while(getline(inputFile,fileBuffer)){
    	
        if(fileBuffer.find("#include") != string::npos ||
            (fileBuffer.find("#") != string::npos && fileBuffer.find("include") != string::npos) ){
            
            size_t ext = fileBuffer.find("\"");
            char strFb[128];
	        char buf[128];
            memset(strFb,0,sizeof(strFb));
            if(ext != string::npos){
                cout<<"find:ext="<<ext<<" "<<fileBuffer.c_str()+ext<<endl;            
                sscanf(fileBuffer.c_str()+ext,"%s",strFb);
          
                strFb[strlen(strFb)-1] = '\0';
                cout<<"strFb:"<<strFb<<endl;
                std::string fb = strFb+1;
    		    CopyFile(fb, filePath);
    		}
        }
        

    }
    
    inputFile.close();
	return 0;
}

int main(int argv,char **argc){
    if(argv != 2){
        cout << "Usage:./cpFile <mainfileName>.c"<<endl;
        return -1;
    }
	string filePath;
	
	ofstream osfn;
	osfn.open(saveFileName.c_str(),ios::out);
	if(osfn.fail()){
		cout << saveFileName << "Open fail" << endl;
		return -1;
	}
	osfn.close();
	
	filePath = "./CopyHeadFile/";
    Copy(argc[1], filePath);
    return 0;
}


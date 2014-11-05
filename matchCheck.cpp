#include<fstream>
#include<vector>
#include<algorithm>
#include<string>
#include<iostream>
#include<cstdlib>
#include<map>
using namespace std;

std::string checkDataName = "checkData1.txt";
std::string logName = "checkLog1.txt";

typedef struct checkPoint{
	std::string name;
	int row,col;
	std::string declName;
	int declRow,declCol;
	int flag;
	void *point;
}checkPoint;

map<void *,checkPoint> mp;
void openinFile(ifstream & infile,std::string name){
	infile.open(name.c_str());
	if(!infile){
		cout << "fail to open the file: "<<checkDataName<<"!\n";
		exit(-1);
	}
}
void openoutFile(ofstream & outfile,std::string name){
	outfile.open(name.c_str());
	if(!outfile){
		cout << "fail to open the file: "<<logName<<"!\n";
		exit(-1);
	}
}

int main(){
	ifstream cdFile;
	openinFile(cdFile,checkDataName);

    ofstream logFile;
    openoutFile(logFile,logName);
    
	mp.clear();
	while(!cdFile.eof()){
		checkPoint cp;
		std::string type;
		void *cpP;
		cdFile >> type;
		cdFile >> cp.name >> cp.row >> cp.col;
//		cp.declName >>cp.declRow >> cp.declCol;
		cdFile >> cpP;
		if(cdFile.eof())break;

		
		if(type == "m"){
	        if(!mp.count(cpP)){
                cp.flag = 1;
                mp[cpP] = cp;
                //cout<<"!mp.count(cpP) ! "<<cp.row<<":"<<cp.col<<":"<<cp.name<<"\n";
            }else{
                if(mp[cpP].flag == 1){//seems it's not likely to happen
                    cout<<cp.row<<":"<<cp.col<<":"<<cp.name<<":memory leaks! haven't free the previous pointer \n";    
			        logFile<<cp.row<<":"<<cp.col<<":"<<cp.name<<":memory leaks! haven't free the previous pointer \n"; 
                }
                cp.flag = 1;
                mp[cpP] = cp;
            }
		}
		else if(type == "f"){
		    if(!mp.count(cpP) || mp[cpP].flag == 0){
		        cout<<cp.row<<":"<<cp.col<<":"<<cp.name<<":error free!\n";
	            logFile<<cp.row<<":"<<cp.col<<":"<<cp.name<<":error free!\n";
		    
		    }else{
		        mp[cpP].flag = 0;
		    }
		
		}
   		

	}//end while
    map<void*,checkPoint>::iterator it;
    for(it=mp.begin();it!=mp.end();++it){
        if(it->second.flag == 1){
            //char cp.name[64];
            //changeName(cp.name,it->second.name);
            cout<<it->second.row<<":"<<it->second.col<<":"<<it->second.name<<":memory leaks! haven't free the pointer after the program's ending\n";    
	        logFile<<it->second.row<<":"<<it->second.col<<":"<<it->second.name<<":memory leaks! haven't free the pointer after the program's ending\n";    
        }
    
    }

    logFile.close();
	cdFile.close();

	return 0;
}

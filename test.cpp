#include<iostream>
#include "include/template.h"

using namespace std;
void generateSplitTask(int numTask,int numProcesses,vector<int>&task){
	task.resize(numProcesses+1);
	int numXPerProcesses=numTask/numProcesses;
	int numRemain=numTask-numXPerProcesses*numProcesses;
	task[0]=0;
	int i;
	for(i=0;i<numRemain;i++){
		task[i+1]=task[i]+numXPerProcesses+1;
	}
	for(;i<numProcesses;i++){
		task[i+1]=task[i]+numXPerProcesses;
	}
}
int main(){
	int a,b;
	vector<int>task;
	while(1){
		cout<<"numTask:";
		cin>>a;
		cout<<"numProcesses:";
		cin>>b;
		generateSplitTask(a,b,task);
		cout<<"Task:";
		printVec(task);
		cout<<endl;
	}
	return 0;
}

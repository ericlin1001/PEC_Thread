//Warning:: EvaluationFunction(e.g. F1~F30) must fit in concurrency-safety, i.e. DO NOT use global variables.
//Warning::if task.isFinished in master return =-1;
//Notice:: vector will reallocate space, when the capacity is not enough.
#include "include/template.h"
#include "include/BasicDE.h"
#include "include/SignalHandleHelper.h"
#include "include/IDHelper.h"
#include "pecFunction.h"
#include<pthread.h>
#include<algorithm>
#include<map>
#include<iostream>
#include<fstream>
#undef DEBUG
#define NOT_USED {cerr<<"Error::UNUSED."<<endl;}
//#define INFO
//#define DEBUG
//#define DEBUG1
using namespace std;
DefFunction(TestF,-100,100,100)
	return xs[0];
	EndDef

	///////////////////////
	void saveConfigData(int id,const char *f,const char *algorithm,const char *param,int run,int MaxRun,int numOfProcesses,int MaxFEs,int PopSize,int NumDim,double F,double CR,const char *state,double usedTime,double absError,
			vector<double>&x,double fx){
		ofstream runConfig;
		char buff[1000];
		sprintf(buff,"Run-configuration-%d.txt",id);
		if(strcmp(state,"start")==0){
#ifdef INFO
			cout<<"Save file:"<<buff<<endl;
#endif
		}
		runConfig.open(buff,ofstream::app|ofstream::out);
		runConfig<<"ID\tFunction\tAlgorithm\tParamemterFile\tRun\tMaxRun\tNumOfProcesses\tMaxFEs\tPopSize\tNumDim\tF-parameter\tCR-paramter\tState\tUsedTime\tAbsError\tX\tFx"<<endl;
		sprintf(buff,"%d\t%s\t%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%g\t%g\t%s\t%g\t%g",id,f,algorithm,param,run,MaxRun,numOfProcesses,MaxFEs,
				PopSize,NumDim,F,CR,state,usedTime,absError);
		runConfig<<buff<<"\t";
		if(x.size()==0){
			runConfig<<"-1";
		}else{
			for(int i=0;i<x.size();i++){
				sprintf(buff,"%g",x[i]);
				if(i!=0){
					runConfig<<",";
				}
				runConfig<<buff;
			} 
			sprintf(buff,"\t%g",fx);
			runConfig<<buff;

			runConfig<<endl;
			runConfig.flush();
			runConfig.close();
		}
	}

EA *de=0;
void IntHandler(int s){
	if(de!=NULL){
		delete de;
		de=NULL;
	}
	cout<<"My:Caught signal SIGINT"<<endl;
	exit(1);
}
int MainProgram(MPIHelper*mpi,int run,int MaxRun,int configID,EA *de,Function*f,SearchParam*param,bool isFindMin=true){
	/*************Shared Data*************/
	vector<double>x;
	double fx=-1;
	//srand(time(NULL));
	Save save;
	/**************end Shared Data***********/

	/********Master data*************/
	char state[50]="start";
	double usedTime;
	double absError;
	/**********end Master data*************/

	//
	//
#ifdef INFO 
	printf("client(%s) starts computing...\n",mpi->getName());
#endif
	if(mpi->isMaster()){
		printf("Runing %s \n",de->getName());
		printf("%cFunName(MyBestF,Optima)\n",isFindMin?'-':'+');
		//
		usedTime=-1;
		absError=-1;
		//
		char saveFileName[100];
		sprintf(saveFileName,"Data-%d.txt",configID);
		save.init(saveFileName);
		de->addSave(&save);
		saveConfigData(configID,f->getName(),de->getName(),param->getName(),run,MaxRun,mpi->getNumProcesses(),param->getInt("MaxFEs"),
				param->getInt("PopSize"),param->getInt("NumDim"),param->getDouble("F"),param->getDouble("CR"),state,usedTime,absError,x,fx);
		Tic::tic("begin");
	}
	if(isFindMin){
		de->getMin(f,param->getInt("MaxFEs"),x,fx);
	}else{
		de->getMax(f,param->getInt("MaxFEs"),x,fx);
	}
	if(mpi->isMaster()){
		usedTime=Tic::tic("end");
		absError=fabs(fx-f->getFBest());
		strcpy(state,"end");

		saveConfigData(configID,f->getName(),de->getName(),param->getName(),run,MaxRun,mpi->getNumProcesses(),param->getInt("MaxFEs"),
				param->getInt("PopSize"),param->getInt("NumDim"),param->getDouble("F"),param->getDouble("CR"),state,usedTime,absError,x,fx);

		printf("%c%s(%g,%g)\n",isFindMin?'-':'+',f->getName(),fx,f->getFBest());
		cout<<"ends successfully!"<<endl;
		cout<<endl;
	}
	return 0;
}
#ifdef ORIGINAL
#include "ParallelDE.h"
#else
#include "ParallelThreadDE.h"
#endif

int main(int argc,char *argv[]){
	SignalHandleHelper::registerSignalHandler(IntHandler);
	MPIHelper mpi(argc,argv);
	int configID;//Should be run once, in master node.
	//	SearchParam param("TestF.json");
    Function*f=new TestF();
#ifdef ORIGINAL
	de=new ParallelDE();
#else
	de=new ParallelThreadDE();
#ifdef THREAD
            TaskScheduler*ts=new AutoTaskScheduler();
#else
            TaskScheduler*ts=new BasicTaskScheduler();
#endif
    ((ParallelThreadDE*)de)->setScheduler(ts);
#endif
	   //
	//	Function*f=new F1();
	/*
	 */
	//Function*f=new PECFunction();
	SearchParam param("PEC.json");
	/*
	 */
	//
	de->setParam(&mpi,&param);
	f->setNumDim(param.getInt("NumDim"));
	const int MaxRun=param.getInt("MaxRun");
	int run=0;
	bool isFindMin=false;
//	for(run=1;run<=MaxRun;run++){
	for(int j=0;j<1;j++){
		if(mpi.isMaster()){
			configID=IDHelper::newID();//Should be run once, in master node.
		}else{
			configID=-1;//unused in slavery process.
		}
		MainProgram(&mpi,run,MaxRun,configID,de,f,&param,isFindMin);
		}
//	}
	delete de;
	delete f;
	//	cout<<"end of program."<<endl;
	return 0;
}

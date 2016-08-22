class ParallelDE:public EA
{
	private:
		//about function:f
		Function *f;
		vector<vector<double> >range;
		int numDim;
		//algorithm related parameters.
		int PopSize;
		double F,CR;
		//
		vector<vector<double> >x;//x,trail x.
		vector<vector<double> >tmpX;
		vector<double>fx;
		vector<double>tmpFx;
		vector<double>tx;
		//
		int bestI;
		MPIHelper*mpi;
		SearchParam *param;
		Save *save;
	private:
		inline void updateX(){
			updateX_schema2();
		}
		void updateX_schema3(){
			//main process
			vector<vector<double> >txs;
			vector<double> ftxs;
			txs.resize(PopSize);

			RandomPermutation perm(PopSize);
			for(int i=0;i<PopSize;i++){
				perm.generate();
				int a=perm.next(); int b=perm.next(); int c=perm.next();
				if(a==i){a=perm.next();}
				if(b==i){b=perm.next();}
				if(c==i){c=perm.next();}
				int randDim=rand()%numDim;
				for(int j=0;j<numDim;j++){
					if(j==randDim||drand()<=CR){
						tx[j]=x[a][j]+F*(x[b][j]-x[c][j]);
						if(tx[j]<range[j][0] || tx[j]>range[j][1]){
							tx[j]=drand(range[j][0],range[j][1]);
						}
					}else{
						tx[j]=x[i][j];
					}
				}
				txs[i]=tx;
			}
			evaluatePopulation(txs,ftxs);
			for(int i=0;i<PopSize;i++){
				vector<double>&tx=txs[i];
				double &ftx=ftxs[i];
				if(ftx<fx[i]){
					x[i]=tx;
					fx[i]=ftx;
					if(ftx<fx[bestI]){
						bestI=i;
					}
				}
			}
		}
		void updateX_schema2(){
			//main process
			vector<vector<double> >txs;
			vector<double> ftxs;
			txs.resize(PopSize);

			RandomPermutation perm(PopSize);
			for(int i=0;i<PopSize;i++){
				perm.generate();
				int a=perm.next(); int b=perm.next(); int c=perm.next();
				if(a==i){a=perm.next();}
				if(b==i){b=perm.next();}
				if(c==i){c=perm.next();}
				int randDim=rand()%numDim;
				for(int j=0;j<numDim;j++){
					if(j==randDim||drand()<=CR){
						tx[j]=x[a][j]+F*(x[b][j]-x[c][j]);
						if(tx[j]<range[j][0]){
							//	tx[j]=drand(range[j][0],range[j][1]);
							tx[j]=range[j][0];
						}
						if(tx[j]>range[j][1]){
							tx[j]=range[j][1];
						}
					}else{
						tx[j]=x[i][j];
					}
				}
				txs[i]=tx;
			}
			evaluatePopulation(txs,ftxs);
			for(int i=0;i<PopSize;i++){
				vector<double>&tx=txs[i];
				double &ftx=ftxs[i];
				if(ftx<fx[i]){
					x[i]=tx;
					fx[i]=ftx;
					if(ftx<fx[bestI]){
						bestI=i;
					}
				}
			}
		}
		void schema1_updateX(){
			//main process
			vector<vector<double> >txs;
			vector<double> ftxs;
			txs.resize(PopSize);

			RandomPermutation perm(PopSize);
			for(int i=0;i<PopSize;i++){
				perm.generate();
				int a=bestI; int b=perm.next(); int c=perm.next();
				int randDim=rand()%numDim;
				for(int j=0;j<numDim;j++){
					if(j==randDim||drand()<=CR){
						tx[j]=x[a][j]+F*(x[b][j]-x[c][j]);
						if(tx[j]<range[j][0] || tx[j]>range[j][1]){
							tx[j]=drand(range[j][0],range[j][1]);
						}
					}else{
						tx[j]=x[i][j];
					}
				}
				txs[i]=tx;
			}
			evaluatePopulation(txs,ftxs);
			for(int i=0;i<PopSize;i++){
				vector<double>&tx=txs[i];
				double &ftx=ftxs[i];
				if(ftx<fx[i]){
					x[i]=tx;
					fx[i]=ftx;
					if(ftx<fx[bestI]){
						bestI=i;
					}
				}
			}
		}
	private:
		double getBestFx()const{
			return fx[bestI];
		}
		void update(int maxGeneration){
#define SaveData if(save!=NULL){save->add(getBestFx());}
			SaveData;
			//cout<<"0:F(g="<<0<<")="<<getBestFx()<<endl;
			cout<<"0:"<<getBestFx()<<endl;
			for(int g=1;g<=maxGeneration;g++){
				//	if(maxGeneration<30||g%(maxGeneration/30)==0){
				//		cout<<g/(maxGeneration/30)<<":F(g="<<g<<")="<<getBestFx()<<endl;
				//	}
				cout<<g<<":"<<getBestFx()<<endl;
				updateX();
				SaveData;
			}
		}
	public:
		ParallelDE(){}
		ParallelDE(MPIHelper *h):mpi(h),save(0){
		}
		ParallelDE(MPIHelper *h,SearchParam*p):mpi(h),param(p){
			initParam(param);
		}
		~ParallelDE(){
			//	endEvaluate();
		}
		void setParam(MPIHelper *h,SearchParam*p){
			mpi=h;
			param=p;
			initParam(param);
		}
		void addSave(Save *s){
			save=s;
		}
		void initParam(SearchParam *param){
			this->param=param;
			//
			PopSize=param->getInt("PopSize");
			F=param->getDouble("F");
			CR=param->getDouble("CR");
			param->getBiVector("Range",range);
#ifdef DEBUG
			cout<<"Range:";
			for(int i=0;i<range.size();i++){
				printf("[%g,%g],",range[i][0],range[i][1]);
			}
			cout<<endl;
#endif
			char buff[100];
			sprintf(buff,"%s(%s)","ParallelDE",param->getString("Name"));
			//setName(param->getString("Name"));
			setName(buff);
		}

		void calulateBestI(){
			bestI=0;
			for(int i=0;i<PopSize;i++){
				if(fx[i]<fx[bestI]){ 
					bestI=i;
				}
			}
		}
#define MESS_END 0
#define MESS_EVAL_ARRAY 2

		void generateSplitTask(int numTask,int numProcesses,vector<int>&task){
			//uniformly distribute the tasks among all processes.
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
		void evaluatePopulation(vector<vector<double> >&xs,vector<double>&fx){
			fx.resize(xs.size());
			int numSlaves=mpi->getNumProcesses()-1;
			ASSERT(numSlaves>0);
			//
			vector<int>task;
			generateSplitTask(xs.size(),numSlaves,task);

			for(int i=1;i<=numSlaves;i++){
				int dest=i;
				int len=task[i]-task[i-1];
				if(len>=1){
					mpi->send(MESS_EVAL_ARRAY,dest);
					mpi->send(len,dest);//len
					//cout<<"client("<<dest<<") "<<"end send len:"<<len<<endl;
					for(int j=task[i-1];j<task[i];j++){
						mpi->send(&xs[j][0],xs[j].size(),dest);
					}
					//cout<<"client("<<dest<<") "<<"end send ."<<endl;
				}
			}

			for(int i=1;i<=numSlaves;i++){
				int dest=i;
				//cout<<"client("<<dest<<") "<<"recv from ."<<endl;
				for(int j=task[i-1];j<task[i];j++){
					mpi->recv(&fx[j],1,dest);
				}
				//cout<<"client("<<dest<<") "<<"end recv ."<<endl;
			}
			for(int i=0;i<xs.size();i++){
				cout<<"f(";	printArr(xs[i],xs[i].size());
				cout<<")="<<fx[i]<<endl;
				//Trace(fx[i]);
			}
		}
		void endEvaluate(){
			for(int i=1;i<mpi->getNumProcesses();i++){
				mpi->send(MESS_END,i);
			}
		}
		virtual double getMin(Function *f,int MaxFEs,vector<double>&out_x,double &out_fx){
			if(save!=0){
				save->setXY("Generation",f->getName());
			}
			if(mpi->getNumProcesses()<=1){
				BasicDE de;
				de.initParam(param);
				cout<<"Warning:NumProcesses<=1, Use BasicDE()"<<endl;
				return de.getMin(f,MaxFEs,out_x,out_fx);
			}
			//allocating space.
			this->f=f;
			numDim=f->getNumDim();
			tx.resize(numDim);
			x.resize(PopSize);
			fx.resize(PopSize);
			for(int i=0;i<PopSize;i++){
				x[i].resize(numDim);
			}
			//
			if(mpi->isMaster()){
				ASSERT(range.size()>=f->getNumDim());
				ASSERT(range[0].size()>=2);
				//population initializing....
				for(int i=0;i<PopSize;i++){
					for(int d=0;d<numDim;d++){
						x[i][d]=drand(range[d][0],range[d][1]);
					} 
				}
				evaluatePopulation(x,fx);
				calulateBestI();
#ifdef INFO
				cout<<"*******************Start update per generation"<<endl;
#endif
				//update, main process.
				update(MaxFEs/PopSize-1);
#ifdef INFO
				cout<<"*******************end Start update per generation"<<endl;
#endif
				endEvaluate();//stop evaluating....
				calulateBestI();
				out_x=x[bestI];
				out_fx=fx[bestI];
				return out_fx;
			}else{//slavery processes,only evaluate the f(x).
				bool isEnd=false;
#ifdef DEBUG
				cout<<mpi->getName()<<" starts..."<<endl;
#endif
				while(!isEnd){
					int type;
					int len;
#ifdef DEBUG
					cout<<mpi->getName()<<" is waiting...."<<endl;
#endif
					mpi->recv(type,0);
					switch(type){
						case MESS_END:
#ifdef DEBUG
							cout<<mpi->getName()<<" is exiting..."<<endl;
#endif
							isEnd=true;
							break;
						case MESS_EVAL_ARRAY:
#ifdef DEBUG
							cout<<mpi->getName()<<":MESS_EVAL_ARRAY"<<endl;
#endif
							mpi->recv(len,0);
#ifdef DEBUG
							cout<<mpi->getName()<<" recv len:"<<len<<endl;
#endif
							for(int i=0;i<len;i++){
								mpi->recv(&x[i][0],numDim,0);
							}
#ifdef DEBUG
							cout<<mpi->getName()<<" end recv"<<len<<endl;
#endif
							for(int i=0;i<len;i++){
								fx[i]=f->evaluate(&x[i][0]);
							}
#ifdef DEBUG
							cout<<mpi->getName()<<" end evaluation"<<len<<endl;
#endif
							for(int i=0;i<len;i++){
								mpi->send(&fx[i],1,0);
							}
#ifdef DEBUG
							cout<<mpi->getName()<<" end send"<<len<<endl;
#endif
							break;
						default:
							cerr<<"Error:Unknown mess_type"<<endl;
							//assert(false);
							break;
					}
				}
				return -1;
			}
		}
};

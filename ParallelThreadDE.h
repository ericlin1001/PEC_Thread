class Task{
    public:
    static int idcounter;
    int id;
    //vector<int>processIDs;
    bool isFinished;
    bool isCanceled;
    int refreshID;
    vector<double>x;
    Function*f;
    MPIHelper*mpi;
    public:
    int debugid;
    pthread_t thread;
    double fx;
    Task(){
        isFinished=false;
        f=0;
        mpi=0;
        isCanceled=false;
    }
    void init(vector<double>&x){
        isFinished=false;
        f=0;
        mpi=0;
        isCanceled=false;
        id=Task::idcounter++;
        this->x=x;
    }
    double getFx(){
        return fx;
    }
    void doTask(){
        assert(f!=0);
        assert(x.size()!=0);
        fx=f->evaluate(&x[0]);
    }

};
int Task::idcounter=0;

void * clientThread(void *arg){
    //	Task tmp=*(Task*)arg;//save the arg into temporal var.
    //	Task *t=&tmp;
    Task *t=(Task*)arg;
    //printf("ClientThread(%d)> start(pid:%d,tid:%d)rid:%d\n",t->debugid,t->mpi->getID(),t->id,t->refreshID);
    //printf("ClientThread> start(pid:%d,tid:%d)\n",t->mpi->getID(),t->id);
    t->doTask();
    //send with TaskId,ProcessID,fx.
    //Warning::int->double->int may cause error.
    if(t->isCanceled){
        t->isFinished=true;
        return 0;
    }
    //printArr(t->x,t->x.size());
    //Trace(t->fx);
    //	pthread_testcancel();
    double mess[5]={t->id,t->mpi->getID(),t->fx,t->refreshID,t->debugid};
    t->mpi->send(mess,5,0);
    //printf("end pid:%1.0f,tid:%1.0f\n",mess[1],mess[0]);
    t->isFinished=true;
    //((Task*)arg)->isFinished=true;
    //	pthread_testcancel();
    //assert(!t->isCanceled); Warning::this line should be contained.
    //BUG::some clientThreads are still sending, while the main program exits, which will cause segment faults. (139).
    return 0;
}
;

class TaskScheduler{
    public:
    virtual const char *getName(){return "TaskScheduler";}
    virtual void init(MPIHelper*mpi,Function*f,int numDim){}
    virtual void addTask(const vector<Task> &t){}
    virtual const vector<Task> getTasks()const{return vector<Task>();}
    virtual void end(){}
    virtual void start(){}
};

class BasicTaskScheduler:public TaskScheduler{
    #define MESS_QUIT 0
    #define MESS_ASSIGN 1
    #define MESS_CANCEL 2
    #define MESS_REFRESH 3
    #define MESS_EVAL_ARRAY 4

    MPIHelper *mpi;
    vector<Task> tasks;
    int numSlaves;
    int numDim;
    Function*f;
    public:
    virtual const char *getName(){return "BasicTaskScheduler";}
    void init(MPIHelper*mpi,Function*f,int numDim){
        this->mpi=mpi;
        this->f=f;
        this->numDim=numDim;
        numSlaves=mpi->getNumProcesses()-1;
        ASSERT(numSlaves>=1);
    }
    void addTask(const vector<Task> &t){
        //only in master:
        tasks=t;
    }
    const vector<Task> getTasks()const{
        return  this->tasks;
    }
    void end(){
        if(mpi->isMaster()){
            for(int i=1;i<mpi->getNumProcesses();i++){
                mpi->send(MESS_QUIT,i);
            }
        }else{
        }
    }

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
void start(){
    //return only if all task is finished.
    if(mpi->isMaster()){
        vector<int>solu;
        generateSplitTask(tasks.size(),numSlaves,solu);
        for(int i=1;i<=numSlaves;i++){
            int dest=i;
            int len=solu[i]-solu[i-1];
            if(len>=1){
                mpi->send(MESS_EVAL_ARRAY,dest);
                mpi->send(len,dest);
                for(int j=solu[i-1];j<solu[i];j++){
                    mpi->send(&tasks[j].x[0],numDim,dest);
                }
            }
        }
        for(int i=1;i<=numSlaves;i++){
            int dest=i;
            for(int j=solu[i-1];j<solu[i];j++){
                mpi->recv(tasks[j].fx,dest);
            }

        }
    }else{
        bool isEnd=false;
        int len;
        bool isDebug=false;
        int type;
        assert(numDim>0);
        if(isDebug)
        cout<<"client("<<mpi->getName()<<") start."<<endl;
        while(!isEnd){
            mpi->recv(type,0);
            Task *t=NULL;
            //cout<<mpi->getName()<<">recv:";
            switch(type){
                case MESS_QUIT:
                if(isDebug)
                cout<<"MESS_QUIT";
                isEnd=true;
                break;

                case MESS_EVAL_ARRAY:
                #ifdef DEBUG
                cout<<mpi->getName()<<":MESS_EVAL_ARRAY"<<endl;
                #endif
                mpi->recv(len,0);
                assert(len>0);
                tasks.resize(len);
                #ifdef DEBUG
                cout<<mpi->getName()<<" recv len:"<<len<<endl;
                #endif
                for(int i=0;i<len;i++){
                    tasks[i].x.resize(numDim);
                    mpi->recv(&tasks[i].x[0],numDim,0);
                }
                #ifdef DEBUG
                cout<<mpi->getName()<<" end recv"<<len<<endl;
                #endif
                for(int i=0;i<len;i++){
                    tasks[i].fx=f->evaluate(&tasks[i].x[0]);
                }
                #ifdef DEBUG
                cout<<mpi->getName()<<" end evaluation"<<len<<endl;
                #endif
                for(int i=0;i<len;i++){
                    mpi->send(tasks[i].fx,0);
                }
                #ifdef DEBUG
                cout<<mpi->getName()<<" end send"<<len<<endl;
                #endif
                break;
                default:
                cerr<<"Error:Unknow type"<<endl;
                break;
            }
            if(isDebug)
            cout<<endl;
        }
#ifdef INFO 
printf("client(%s) exit.\n",mpi->getName());
#endif
}
}
};
class AutoTaskScheduler:public TaskScheduler{
    #define MESS_QUIT 0
    #define MESS_ASSIGN 1
    #define MESS_CANCEL 2
    #define MESS_REFRESH 3
    MPIHelper *mpi;
    vector<Task> tasks;
    int numSlaves;
    int numDim;
    Function*f;
    int refreshID;
    struct TwoValue{
        double usedTime;
        double priority;
        //
        int v[2]; double vv[2];
        TwoValue(){
            v[0]=-1;
            v[1]=-1;
            usedTime=-1;
        }
        private:
        int getIndex(int value){

            if(v[0]==value)return 0;
            else if(v[1]==value)return 1;
            Trace(value);
            Trace(v[0]);
            Trace(v[1]);
            assert(false);
        }

        public:
        bool hasKey(int k){
            if(v[0]==k||v[1]==k)return true;
            return false;
        }
        void delKey(int k){
            assert(v[0]==k||v[1]==k);
            v[getIndex(k)]=-1;
        }
        void calPriority(){
            //it doesn't matter if usedTime==-1.
            priority=fabs(usedTime-(Tic::getTimeSec()-getValue(getKey(0))));
        }
        bool operator<(const TwoValue&p)const{
            return priority<p.priority;
        }
        void addKeyValue(int k,double value){
            assert(v[0]==-1||v[1]==-1);
            int i=getIndex(-1);
            v[i]=k;
            vv[i]=value;
        }
        double getValue(int k){
            return vv[getIndex(k)];
        }
        //
        int getKey(int index){
            //can be accessed by indexing... just like a array.
            if(v[0]!=-1&&index!=1)return v[0];
            assert(v[1]!=-1&&(v[0]==-1||index==1));
            return v[1];
        }
int getNumValue(){
    int a=(v[0]!=-1);
    int b=(v[1]!=-1);
    return a+b;
}
};
private:
    vector<TwoValue>slavesTasks;//0 is not used.
    //master data:
        private:
            int getNumTasks(){
                return tasks.size();
            }
            bool isEmpty(){
                return tasks.empty();
            }

private:
    Task* getTask(int tid){
        for(int i=0;i<tasks.size();i++){
            if(tasks[i].id==tid)return &tasks[i];
        }
        return 0;
    }
    //void assignTaskToProcess(int taskIndex,int pid){
    void assignTaskToProcess(int tid,int pid){
        //tasks[tid].debugid=DEBUGID++;
        //stupid.enter(DEBUGID-1);
        mpi->send(MESS_ASSIGN,pid);
        //assert(tid==tasks[tid].id);
        mpi->send(tid,pid);
        /*
        if(taskIndex>13){
        cout<<"*************T1{***********"<<endl;
        Trace(taskIndex);
        Trace(tasks.size());
        Trace(tasks[taskIndex].x.size());
        cout<<"**************}***********"<<endl;
    }
        */
        Task *t=getTask(tid);
        assert(t!=0);
        mpi->send(&t->x[0],numDim,pid);
        //mpi->send(tasks[tid].debugid,pid);
        //
        //assert(tasks[tid].id==tid);
        #ifdef DEBUG1
        printf("master> assignTask(pid:%d,tid:%d)\n",pid,tid);
        #endif
        slavesTasks[pid].addKeyValue(tid,Tic::getTimeSec());
    }
    int acceptTask(){
        //return pid.
        double res[5];
        mpi->recv(res,5,MPI_ANY_SOURCE);
        //mpi->recv(res,4,MPI_ANY_SOURCE);
        //if((int)res[3]!=refreshID)return -1;
        int tid=(int)res[0];
        int pid=(int)res[1];
        double fx=res[2]; 
        //int did=(int)res[4];
        //printf("master(%d)> end(pid:%d,tid:%d)\n",(int)res[4],pid,tid);
        Task *t=getTask(tid);
        #ifdef DEBUG1
        printf("master > end(pid:%d,tid:%d)\n",pid,tid);
        #endif
        if(t==0){
            return -1;
        }
        if(!t->isFinished){
            t->fx=fx;
            t->isFinished=true;
        }else{
            return -1;
        }
        if(slavesTasks[pid].hasKey(tid)){
            //
            if(fabs(slavesTasks[pid].usedTime+1)==0){
                //it's the first time.
                slavesTasks[pid].usedTime=0;
            }
            //cout<<"getValue(pid:"<<pid<<",tid:"<<tid<<")"<<endl;
            slavesTasks[pid].usedTime+=(Tic::getTimeSec()-slavesTasks[pid].getValue(tid));
            slavesTasks[pid].usedTime/=2.0;
            slavesTasks[pid].delKey(tid);
        }
        //
        for(int i=1;i<=numSlaves;i++){
            if(slavesTasks[i].hasKey(tid)){
                masterCancelTask(i,tid);
            }
        }
        return pid;
    }
    bool isAllTaskFinished(){
        for(int i=0;i<tasks.size();i++){
            if(!tasks[i].isFinished)return false;
        }
        return true;
    }
    int getNumUnfinishedTasks(){
        int r=0;
        for(int i=0;i<tasks.size();i++){
            if(!tasks[i].isFinished)r++;
        }
        return r;
    }
    vector<int>getSortedTask(const vector<int>&pids){
        vector<TwoValue>ps;
        for(int i=0;i<pids.size();i++){
            ps.push_back(slavesTasks[pids[i]]);
            ps.back().calPriority();
        }
        sort(ps.begin(),ps.end());
        vector<int>tids;
        for(int i=0;i<ps.size();i++){
            tids.push_back(ps[i].getKey(0));
        }
        reverse(tids.begin(),tids.end());
        return tids;
    }
    void masterCancelTask(int pid,int tid){
        mpi->send(MESS_CANCEL,pid);
        mpi->send(tid,pid);
        slavesTasks[pid].delKey(tid);
    }
    void refreshSlave(){
        for(int i=1;i<=numSlaves;i++){
            //	mpi->send(MESS_REFRESH,i);
            while(slavesTasks[i].getNumValue()>0){
                masterCancelTask(i,slavesTasks[i].getKey(0));
            }
        }
        //this->refreshID++;
    }

    bool adjusts(){
        vector<int>zeroPids,onePids,twoPids;//slave's pid list.
        zeroPids.clear();
        onePids.clear();
        twoPids.clear();
        for(int i=1;i<=numSlaves;i++){
            int tmp=slavesTasks[i].getNumValue();
            if(tmp==0){
                zeroPids.push_back(i);
            }else if(tmp==1){
                onePids.push_back(i);
            }else{
                twoPids.push_back(i);
            }
        }
        if(!zeroPids.empty()){//has zeroPids's
                              if(!twoPids.empty()){//has twoPids's
                                                   int min_s=min(zeroPids.size(),twoPids.size());
                                                   for(int i=0;i<min_s;i++){
                                                       int tid=slavesTasks[twoPids[i]].getKey(1);
                                                       int pid=zeroPids[i];
                                                       masterCancelTask(twoPids[i],tid);
                                                       //if(slavesTasks[pid].getNumValue()==0||slavesTasks[pid].getKey(0)!=tid){
                                                       assignTaskToProcess(tid,pid);
                                                       //}
                                                   }
                                                  }else{//has onePids's
                                                        //rebalances some xs.
                                                        vector<int> tids=getSortedTask(onePids);
                                                        int min_s=min(zeroPids.size(),tids.size());
                                                        for(int i=0;i<min_s;i++){
                                                            int tid=tids[i];
                                                            int pid=zeroPids[i];
                                                            //if(slavesTasks[pid].getNumValue()==0||slavesTasks[pid].getKey(0)!=tid){
                                                            assignTaskToProcess(tid,pid);
                                                            //}
                                                        }
                                                       }
                             }else{
                                 //all processes are filled with 1 to 2 tasks.
                                 return false;
                             }
        return true;
    }

    public:
        void init(MPIHelper*mpi,Function*f,int numDim){
            this->mpi=mpi;
            this->f=f;
            this->numDim=numDim;
            refreshID=0;

            numSlaves=mpi->getNumProcesses()-1;
            slavesTasks.resize(numSlaves+1);
            ASSERT(numSlaves>=1);
        }

    void addTask(const vector<Task> &t){
        //only in master:
        tasks=t;
    }
    const vector<Task> getTasks()const{
        return  this->tasks;
    }
    void end(){
        if(mpi->isMaster()){
            for(int i=1;i<mpi->getNumProcesses();i++){
                mpi->send(MESS_QUIT,i);
            }
        }else{
        }
    }

        virtual const char *getName(){return "AutoTaskScheduler";}
        void start(){
            //return only if all task is finished.
            if(mpi->isMaster()){
                int numTasks=getNumTasks();
                assert(numTasks==tasks.size());
                #ifdef DEBUG1
                Trace(numTasks);
                Trace(numSlaves);
                #endif
                int nextTask=0;
                for(int j=0;j<2;j++){
                    for(int i=1;i<=numSlaves;i++){
                        if(nextTask>=numTasks)break;
                        assert(nextTask<numTasks);
                        assignTaskToProcess(tasks[nextTask].id,i);
                        nextTask++;
                    }
                }
                #ifdef DEBUG1
                cout<<"##end init."<<endl;
                #endif
                while(nextTask<numTasks){
                    int pid=acceptTask();
                    if(pid!=-1){
                        //assign a new task for it.
                        assert(nextTask<numTasks);
                        assignTaskToProcess(tasks[nextTask].id,pid);
                        nextTask++;
                    }
                }
                #ifdef DEBUG1
                cout<<"###end middle."<<endl;
                #endif
                while(!isAllTaskFinished()){
                    //has sth not finished.
                    if(adjusts()){
                    }else{
                        acceptTask();
                        //Trace(getNumUnfinishedTasks());
                    }
                }
                refreshSlave();
                //clear all things.
            }else{
                bool isEnd=false;
                int type;
                bool isDebug=false;
                int refreshID=0;
                int tid;
                assert(numDim>0);
                if(isDebug)
                cout<<"client("<<mpi->getName()<<") start."<<endl;
                while(!isEnd){
                    mpi->recv(type,0);
                    Task *t=NULL;
                    //cout<<mpi->getName()<<">recv:";
                    switch(type){
                        case MESS_QUIT:
                        if(isDebug)
                        cout<<"MESS_QUIT";
                        isEnd=true;
                        break;
                        case MESS_ASSIGN:
                        if(isDebug)
                        cout<<"MESS_ASSIGN";
                        //taskID,x,
                        mpi->recv(tid,0);

                        assert(ctasks.find(tid)==ctasks.end() || ctasks.find(tid)->second.isCanceled);
                        if(ctasks.find(tid)!=ctasks.end()){
                            t=&ctasks.find(tid)->second;
                            if(t->isCanceled){
                                //Warning: data race.
                                t->isCanceled=false;
                                t->isFinished=false;
                                mpi->recv(&(t->x[0]),numDim,0);//useless.
                                startTask(t);

                            }else{
                                printf("Error(pid:%d,tid:%d)\n",mpi->getID(),tid);
                            }
                        }else{
                            ctasks[tid]=Task();
                            t=&(ctasks[tid]);
                            t->id=tid;
                            t->x.resize(numDim);
                            mpi->recv(&(t->x[0]),numDim,0);
                            t->f=f;
                            t->mpi=mpi;
                            startTask(t);
                        }
                        break;
                        case MESS_CANCEL:
                        if(isDebug)
                        cout<<"MESS_CANCEL";
                        //taskid.
                        int taskID;
                        mpi->recv(taskID,0);
                        cancelTask(taskID);
                        break;
                        default:
                        cerr<<"Error:Unknow type"<<endl;
                        break;
                    }
        if(isDebug)
        cout<<endl;
        }
        #ifdef INFO 
        printf("client(%s) exit.\n",mpi->getName());
        #endif
        }
        }
        private:
            map<int,Task> ctasks;
            void startTask(Task*t){
                //clienThread.
                //printf("Create thread(%d) :(pid:%d,tid:%d)\n",t.debugid,t.mpi->getID(),t.id);
                //printf("Create thread :(pid:%d,tid:%d)\n",t->mpi->getID(),t->id);
                int res=pthread_create(&(t->thread),NULL,clientThread,(void*)t);//pthread_exit(t1);
                ASSERT(!res);
            }
            //BUG::There maybe a case that some last generation tasks are finished in this generation, and mix with the current tid.
            void cancelTask(int taskid){
                //BUG:: cancelTask can't erase task, or the pointer will be invalid.
                ctasks[taskid].isCanceled=true;
                map<int,Task>::iterator i;
                for(i=ctasks.begin();i!=ctasks.end();++i){
                    if(i->second.isFinished){
                        ctasks.erase(i);
                    }
                }
                /*
                for(int i=0;i<tasks.size();i++){
                if(tasks[i].id==taskid){
                //tasks[i].isCanceled=true;
                //pthread_cancel(tasks[i].thread);
                //pthread_join(tasks[i].thread,NULL);
                tasks.erase(tasks.begin()+i);
                //	break;
            }
                //deleting useless record..
                if(tasks[i].isFinished){
                tasks.erase(tasks.begin()+i);
                i--;
            }
            }
                */
            }
            };

class ParallelThreadDE:public EA
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
    TaskScheduler *scheduler;
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
        cout<<"0:"<<getBestFx()<<endl;
        for(int g=1;g<=maxGeneration;g++){
            //	if(maxGeneration<30||g%(maxGeneration/30)==0){
            //	cout<<g/(maxGeneration/30)<<":F(g="<<g<<")="<<getBestFx()<<endl;
            //}
            cout<<g<<":"<<getBestFx()<<endl;
            updateX();
            SaveData;
        }
    }
    public:
        ParallelThreadDE(){
            scheduler=0;
            save=0;
        }
        ParallelThreadDE(MPIHelper *h){
            mpi=h;
            save=0;
            scheduler=0;
        }
        ParallelThreadDE(MPIHelper *h,SearchParam*p){
            scheduler=0;
            setParam(h,p);
        }
        ~ParallelThreadDE(){
        }
        void setScheduler(TaskScheduler*s){
            scheduler=s;
            if(param!=0&&scheduler!=0){
                char buff[100];
                sprintf(buff,"%s(%s,%s)","ParallelThreadDE",param->getString("Name"),scheduler->getName());
                //setName(param->getString("Name"));
                setName(buff);
            }else{
                setName("Warning:UnsetNameDE");
            }
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
            if(mpi==0||mpi->isMaster()){
                cout<<"Range:";
                for(int i=0;i<range.size();i++){
                    printf("[%g,%g],",range[i][0],range[i][1]);
                }
                cout<<endl;
            }
            #endif
            if(param!=0&&scheduler!=0){
                char buff[100];
                sprintf(buff,"%s(%s,%s)","ParallelThreadDE",param->getString("Name"),scheduler->getName());
                //setName(param->getString("Name"));
                setName(buff);
            }else{
                setName("Warning:UnsetNameDE");
            }
        }

        void calulateBestI(){
            bestI=0;
            for(int i=0;i<PopSize;i++){
                if(fx[i]<fx[bestI]){ 
                    bestI=i;
                }
            }
        }

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
            vector<Task>tasks;
            tasks.resize(xs.size());
            for(int i=0;i<xs.size();i++){
                tasks[i].init(xs[i]);
                assert(tasks[0].id+i==tasks[i].id);
            }
            //
            scheduler->addTask(tasks);
            scheduler->start();
            tasks=scheduler->getTasks();
            //
            fx.resize(xs.size());
            for(int i=0;i<xs.size();i++){
                fx[i]=tasks[i].getFx();
            }
            //
            for(int i=0;i<xs.size();i++){
                cout<<"f(";	printArr(xs[i],xs[i].size());
                cout<<")="<<fx[i]<<endl;
                //Trace(fx[i]);
            }
        }
        virtual double getMin(Function *f,int MaxFEs,vector<double>&out_x,double &out_fx){
            //TaskScheduler*myts=new AutoTaskScheduler();

            TaskScheduler*myts=0;
            if(scheduler==0){
                myts=new BasicTaskScheduler();
                setScheduler(myts);
            }
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
            scheduler->init(mpi,f,numDim);
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
                scheduler->end();//stop evaluating....
                #ifdef INFO 
                cout<<"*******************end Start update per generation"<<endl;
                #endif
                calulateBestI();
                out_x=x[bestI];
                out_fx=fx[bestI];
                if(myts!=0){
                    delete myts;
                }
                return out_fx;
            }else{//slavery processes,only evaluate the f(x).
                  scheduler->start();
                  scheduler->end();
                  if(myts!=0){
                      delete myts;
                  }
                  return 0;
                 }
        }
        };

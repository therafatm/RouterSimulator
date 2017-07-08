#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdbool.h>
#include <sys/types.h>

// - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Structs, global variables, #defines
// - - - - - - - - - - - - - - - - - - - - - - - - - - - -

#define DEBUG false 

typedef struct _flow
{
    bool isInitialized;
    float arrivalTime;
    float transTime;
    int priority;
    int flowNo;
    //flowNo is flow id as read from input
    int id;
    //unique id given to each flow to determine
    //order of flows. flowNo might not be in sequence.
} flow;

pthread_mutex_t trans_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t trans_cvar = PTHREAD_COND_INITIALIZER;
struct timeval base;
flow ** queueList;
bool transBusy = false;
int currentTransmittingFlowNo;
int queueListIndex = 0;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Parsing utility functions
// - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void fillFlows(flow * _flow, char * token, int j){
    
    //  Typical flow file
    //  3
    //  1:3,60,3
    //  2:6,70,1
    //  3:3,50,3
    
    int value = atoi(token);
    switch(j){
        case 0:
            _flow->flowNo = value;
            break;

        case 1:
            _flow->arrivalTime = value;
            break;

        case 2:
            _flow->transTime = value;
            break;

        case 3:
            _flow->priority = value;
            break;      
    }

    return;
}

void readFlows(FILE * fp, flow * _flow, int numFlows){

    char * line = NULL;
    size_t len = 0;
    int i = 0;
    char * token;
    int bytesRead;
    int flowID = 0;

    while(i < numFlows){
        bytesRead = getline(&line, &len, fp);
        if(bytesRead == -1){
            perror("Error reading from file, initial getline failed.\n");
        }

        else{

            int j = 0;
            token = strtok(line, ":");
            _flow->id = flowID;
            while (j <= 3)
            {
                fillFlows(_flow, token, j);
                j++;

                if(j<4){
                    token = strtok(NULL, ",\n");                
                }
            }
            _flow++;
            flowID++;
        }

        i++;
    }

    if(line){
        free(line);
    }

    if(bytesRead == -1){
        perror("Error reading from file, initial getline failed.\n");
    }

    return;
}

void printReadFlows(flow flowList[], int numFlows){

    int k = 0;
    for(k ; k < numFlows; k++){
        printf("ID:%d:", flowList[k].id);
        printf("FlowNo:%d:", flowList[k].flowNo);
        printf("ArrivalTime:%.0f,", flowList[k].arrivalTime);
        printf("TransTime:%.0f,", flowList[k].transTime);
        printf("Priority:%d\n", flowList[k].priority);                
    }
}

void printFlow(flow * f){
    printf("ID:\n%d:", f->id);
    printf("FlowNo:\n%d:", f->flowNo);   
    printf("ArrivalTime:%.0f,", f->arrivalTime);
    printf("TransTime:%.0f,", f->transTime);
    printf("Priority:%d\n", f->priority);    
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Other utility functions
// - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void printQueueList(queueListIndex){
    int k = 0;
    for(k ; k < queueListIndex; k++){
        printf("ID:%d:", queueList[k]->id);
        printf("FlowNo:%d:", queueList[k]->flowNo);      
        printf("ArrivalTime:%.0f,", queueList[k]->arrivalTime);
        printf("TransTime:%.0f,", queueList[k]->transTime);
        printf("Priority:%d\n", queueList[k]->priority);                
    }
}

int comparator(const void * A, const void * B){

    flow ** a = (flow **) A;
    flow ** b = (flow **) B;

    if(!(*a)->isInitialized || !(*b)->isInitialized){
        if(!(*a)->isInitialized && !(*b)->isInitialized){
            return 1;
        }

        // b is initialized, return b
        if(!(*a)->isInitialized){
            return 1;
        }

        // a is initialized, return a
        else{
            return -1;
        }
    }

    int priority = (*a)->priority - (*b)->priority;
    if(priority == 0){
        float arrivalTime = (*a)->arrivalTime - (*b)->arrivalTime;
        if(arrivalTime == 0){
            float transTime = (*a)->transTime - (*b)->transTime;
            if(transTime == 0){
                int id = (*a)->id - (*b)->id;
                return id;
            }
            return transTime;
        }
        return arrivalTime;
    }
    else{
        return priority;
    }
}

void deque(){

    long i = 0;
    if(DEBUG){
        printf("Deleted flow %d from queue:\n", queueList[0]->flowNo);
    }
    
    //if there's atleast two things, pop queue
    if(queueListIndex > 1){
        for(i; i < queueListIndex - 1; i++){
            //copy next item to previous
            queueList[i]->flowNo = queueList[i+1]->flowNo;            
            queueList[i]->id = queueList[i+1]->id;
            queueList[i]->priority = queueList[i+1]->priority;
            queueList[i]->transTime = queueList[i+1]->transTime;
            queueList[i]->arrivalTime = queueList[i+1]->arrivalTime;
            queueList[i]->isInitialized = queueList[i+1]->isInitialized;            
        }
    }
    
    queueList[queueListIndex - 1]->isInitialized = false;
    queueListIndex--;

    if(DEBUG){
        printQueueList(queueListIndex);  
        fflush(stdout);
    }

    return;
}

long getMicroTime(float a){
    long time = (a/10) * 1000000;
    return time;
}

double getTimeToDecimals(struct timeval time, struct timeval base){

    double t = (double)(time.tv_usec - base.tv_usec)/1000000 + (double)(time.tv_sec - base.tv_sec);
    return t;
}

// parameter passed to usleep() should be under 10^6
void uSleepMultiple(long t){

    int million = 1000000;
    int v = million - 1;

    if(t < million){
        if(usleep(t) != 0){
            perror("Error sleeping:");
        }
        return;
    }

    while(t >= million){
        t = t - v;
        if(usleep(v) != 0){
            perror("Error sleeping:");
        }
    }

    if(t > 0){
        if(usleep(t) != 0){
            perror("Error sleeping:");
        }
    }
    return;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Thread functions
// - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void releasePipe() {

    transBusy = false;
    pthread_cond_broadcast(&trans_cvar);
    return;
}

void requestPipe(flow *item) {
    // lock mutex;
    pthread_mutex_lock(&trans_mtx);
    // if transmission pipe available && queue is empty
    if(queueList[0]->isInitialized == false && !transBusy){
        //do some stuff
        transBusy = true;
        //unlock mutex;
        pthread_mutex_unlock(&trans_mtx);
        return;
    }
        
    // add item in queue
    queueList[queueListIndex]->flowNo = item->flowNo;    
    queueList[queueListIndex]->id = item->id;
    queueList[queueListIndex]->priority = item->priority;
    queueList[queueListIndex]->transTime = item->transTime;
    queueList[queueListIndex]->arrivalTime = item->arrivalTime;
    queueList[queueListIndex]->isInitialized = true;
    queueListIndex++;

    //sort the queue
    //**Could potentially sort during release, but that would 
    //**require another mutex/convar. Didn't want to do that.
    if(queueList[0]->isInitialized && queueListIndex > 1){
        qsort(queueList, queueListIndex, sizeof(flow *), comparator);        
    }

    if(DEBUG){
        printf("Added and updated flow %d to queue.\n", item->flowNo);
        printQueueList(queueListIndex);  
        fflush(stdout);        
    }

    // wait till pipe is available and current flow is at the top of the queue
    printf("Flow %2d waits for flow %2d to finish transmission.\n", item->flowNo, currentTransmittingFlowNo);      
    while( (!transBusy && queueList[0]->id == item->id) != true){   
        pthread_cond_wait(&trans_cvar, &trans_mtx); // release mutex(lock), wait on convar, until it is signaled
    }

    // set transBusy, dequeue
    transBusy = true;
    deque();
    
    //unlock mutex;
    pthread_mutex_unlock(&trans_mtx);
    return;
}

// entry point for each thread created
void *thrFunction(void *flowItem) {

    flow *item = (flow *)flowItem;
    struct timeval time;

    // wait for arrival 
    long t = getMicroTime(item->arrivalTime);
    uSleepMultiple(t);

    gettimeofday(&time, NULL);
    double currentTime = getTimeToDecimals(time, base);
    printf("Flow %2d arrives: arrival time (%.2f), transmission time (%.1f), priority (%d). \n", item->flowNo, currentTime, item->transTime/10, item->priority);

    requestPipe(item);

    gettimeofday(&time, NULL);
    currentTime = getTimeToDecimals(time, base);
    currentTransmittingFlowNo = item->flowNo;
    // convert time from tenth of a second to microseconds
    t = getMicroTime(item->transTime);    
    printf("Flow %2d starts its transmission at time %.2f. \n", item->flowNo, currentTime);

    // sleep for transmission time
    uSleepMultiple(t);
    
    gettimeofday(&time, NULL);
    currentTime = getTimeToDecimals(time, base);
    printf("Flow %2d finishes its transmission at time %.2f. \n", item->flowNo, currentTime);
    releasePipe();

    pthread_exit(NULL);
}

void createFlowThreads(pthread_t * flowThreadListItem, void * threadFunction, flow * flowListItem, int numFlows){

    int i = 0;
    for(i; i < numFlows; i++){
    // create a thread for each flow
        if(pthread_create(flowThreadListItem, NULL, thrFunction, flowListItem) != 0){
            printf("Error when creating thread for flow with id %2d\n", flowListItem->flowNo);
        }
        else{
            // increment counter for next thread
            flowThreadListItem++;
        }
        // increment to next flow regardless of success or failure of thread creation
        flowListItem++;
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - -

int main(int argc, char * args[]) {

    if(argc < 2){
        printf("Please try again with a filename\n");
        return -1;
    }    

    char * fileName = args[1];    
    if(fileName == NULL){
        printf("Error when reading from args[]\n");
        return -1;
    }

    char * line = NULL;
    size_t len = 0;
    int numFlows = 0;

    FILE * fp = fopen(fileName, "r");
    if(fp == NULL){
        perror("Error reading given input file");
        printf("MFS will now exit.\n");
        return -1;
    }

    if(getline(&line, &len, fp) == -1){
        perror("Error reading input file:");
        printf("MFS will now exit.\n");        
        return -1;
    }
    
    numFlows = atoi(line);
    if(line){
        free(line);
    }

    if(numFlows == 0){
        printf("This input file contains no flows to simulate.\n");
        printf("MFS will now exit.\n");                
        return 1;
    }

    flow flowList[numFlows]; // parse input in an array of flow
    queueList = malloc(sizeof(flow *) * numFlows); // store waiting flows while transmission pipe is occupied.
    
    // allocate space for queue
    int j = 0;
    for(j ; j < numFlows; j++){
        queueList[j] = (flow *) malloc(sizeof(flow));
        queueList[j]->isInitialized = false;
    }

    pthread_t threadList[numFlows]; // each thread simulates one flow

    readFlows(fp, &flowList[0], numFlows);
    fclose(fp);

    if(DEBUG){
        printReadFlows(flowList, numFlows);
    }

    gettimeofday(&base, NULL);
    if (pthread_mutex_init(&trans_mtx, NULL) != 0){ // mutex initialization
        perror("Mutex init failed\n");
        printf("MFS will now exit.\n");                        
        return -1;
    }

    createFlowThreads(&threadList[0], thrFunction, &flowList[0], numFlows);

    // wait for all threads to terminate
    int i = 0;
    for(i; i < numFlows; i++){
        if(pthread_join(threadList[i], NULL) != 0){
            char errormsg[200];
            sprintf(errormsg, "Error when waiting for thread for flow id: %2d\n", flowList[i].flowNo);
            perror(errormsg);
        }
    }

    // free dynamically allocated memory
    j = 0;
    for(j; j < numFlows; j++){
        free(queueList[j]);
    }    
    free(queueList);

    // destroy mutex & condition variable
    pthread_mutex_destroy(&trans_mtx);
    pthread_cond_destroy(&trans_cvar);

    return 0;
}
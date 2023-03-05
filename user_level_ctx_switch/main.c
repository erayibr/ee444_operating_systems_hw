// Compiles with "gcc main.c -o main" on Ubuntu 20.04 LTS

// Include neccesary libraries
#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

// Define ncccesary constants
#define STACK_SIZE 4096
#define empty 0
#define ready 1
#define running 2
#define finished 3
#define SRTF 1
#define PandWF 0

// Declare some variables which are used inside the program
ucontext_t context_array[6]; // Array of ucontext_t structs
int running_thread, running_thread_temp, randint, sum;
int * current_value;
int flag = 0, flag_firstRunScheduler = 1, flag_firstRunSelect = 1, finishedNum = 0, counter, common_denominator, min, schedulerType;
int i, j;
int thresholds[5], remainingTime[5];

// Declare the struct to store the neccesary data for each ucontext struct
struct ThreadInfo {
    ucontext_t context; // ucontext_t object
    int state; // state (i.e finished, ready...)
    int count_val; // Value to count up to (namely, "n")
    int current_val; // The current value of the ucontext object
};

struct ThreadInfo thread_array[6]; // Array of ThreadInfo structs, keeps all the state information

// printSpace() function prints given number of tab characters
void printSpace(int arg){
    for (i = 1; i < arg; ++i){ printf(" \t");}
}

// increment() function increments the value of the running thread by two
void increment(void) { 
    // get the address of current value into current value pointer for readability
    current_value = &thread_array[running_thread].current_val; 
    
    // Increment, print and sleep 2 times in a row
    sleep(1);
    printSpace(running_thread);
    printf("%d\n", ++(*current_value));
    sleep(1);
    printSpace(running_thread);
    printf("%d\n", ++(*current_value));

    // Check whether the current thread is done
    if(thread_array[running_thread].count_val == *current_value){
        thread_array[running_thread].state = finished; // Set status to finished if done
    }
    else{
        thread_array[running_thread].state = ready; // Leave it as ready if not finished
    }
    setcontext(&context_array[0]); // Return back to main()
}

// initializeThread() initializes the neccesary variables for given thread number
void initializeThread (int index, int count_val, ucontext_t *arg){
    thread_array[index].context = *arg; // pass ucontext pointer into the thread array
    thread_array[index].count_val = count_val; // initiliaze value to count up to
    thread_array[index].current_val = 0; // set current val to zero
}

// createThread() creates a new user Thread
void createThread (ucontext_t *arg, struct ThreadInfo * thread){
    getcontext(arg); // initialize ucontext structure
    thread->state = ready; // set state to ready
    arg->uc_link = &context_array[0]; // set link pointer to return back to main (namely c0)
    arg->uc_stack.ss_sp = malloc(STACK_SIZE); // allocate ucontext steack
    arg->uc_stack.ss_size = STACK_SIZE; // set stack size
    makecontext(arg, (void (*)(void))increment, 0); // define the func to return to (increment())
}

// runThread() calls swapcontext() and passes a ucontext struct to the swap function
void runThread (ucontext_t *arg){
    swapcontext(&context_array[0], arg);
}

// exitThread() frees the memory used by the user thread
void exitThread (ucontext_t *arg){
    free(arg->uc_stack.ss_sp);
}

// printThisStatus() prints a given status (i.e. print(Ready) prints threads which has status Ready)
void printThisStatus(int arg){
    i = 1;
    flag = 0;
    counter = 0;
    while(i<6){
        if(thread_array[i].state == arg){
            if(!flag){ flag = 1; }
            else{ printf(","); }
            printf("T%d", i); // Print Tx where x has status given by arg (i.e. T3)
            counter ++;
        }
        i ++;
    }

    // Print a certain number of empty characters for proper formatting
    for (i = 0; i < 6-counter; ++i){ printf("  ");} 
}

// printStatus() prints the current status
void printStatus (void){
    printf("running>");
    printThisStatus(running); // Print running process
    printf("\tready>");
    printThisStatus(ready); // Print ready processes
    printf("\tfinished>");
    printThisStatus(finished); // Print ready processes
    printf("\n");
}

// selectThread() selects a thread to run based on arg 
// (i.e. arg = 1 means SRTF, arg = 0 means P&WF with weighted lottery)
int selectThread (int arg){
    if(arg == PandWF){ // P&WF selection
        if(flag_firstRunSelect){ // if running for the first time
            sum = 0;
            // find threashold values for lottery (namely, num of tickets for each thread)
            for (i = 1; i < 6; ++i){ 
                sum += thread_array[i].count_val;
                thresholds[i-1] = sum;
            }      
            flag_firstRunSelect = 0;
        }
        randint = rand() % sum; //randomly select a number
        for (i = 1; i < 6; ++i){ // determine the winner
            if(randint < thresholds[i-1]){
                if(thread_array[i].state != finished){
                    running_thread = i;
                    break;
                }
                else{
                    selectThread(arg); // if the winner is finished run again
                    break;
                }
            }
        }
    }
    else{ // SRTF Selection
        
        // Find the remaining time for each thread
        for(i = 1; i < 6; ++i){
            remainingTime[i-1] = thread_array[i].count_val - thread_array[i].current_val;
            if(remainingTime[i-1] != 0){
                running_thread = i;
            }
        }

        // Select the one with the shrotest remaining time
        for(i = 1; i < 6; ++i){
            if((remainingTime[running_thread - 1] > remainingTime [i-1]) && (thread_array[i].state != finished)){
                running_thread = i;
            }
        }
    }
}
    

// printShares prints the CPU time required for each thread
// These values are used in P&WF selection method as well
int printShares (void){
    min = thread_array[1].count_val;
    sum = 0;
    // Find total cpu time rquired
    for (i = 1; i < 6; ++i){ 
            sum += thread_array[i].count_val;
        }  
    
    // Find the minimum
    for (i = 2; i < 6; ++i){ 
        if(min > thread_array[i].count_val){ min = thread_array[i].count_val;}
    }

    // Find the largest common denominator for simplification
    for (i = 1; i <= min ; ++i){ 
        for(j = 1; j<6; ++j){
            if(thread_array[j].count_val % i == 0 ){continue;}
            else{break;}
        }
        if(j == 6){ common_denominator = i;}
    }

    // Print the shares in a simplified form
    printf("Share:\n");
    for (i = 1; i < 6; ++i){ 
        printf("%d/%d\t", thread_array[i].count_val/common_denominator, sum/common_denominator);
    }
    printf("\n\n");
}


// scheduler() function selects the thread to run and manages the threads
// takes argument based on scheduling type (i.e. SRTF or P&WF)
void scheduler (int arg){
    // Print the threads in the first run
    if(flag_firstRunScheduler == 0){ running_thread_temp = running_thread; }
    else { flag_firstRunScheduler = 0;
        printShares();
        printf("Threads:\nT1\tT2\tT3\tT4\tT5\n");
        }
    
    // Keep track of processes that are finished
    if(thread_array[running_thread].state == finished){finishedNum ++;}
    if(finishedNum != 5){ // If all the processes not finished yet
        selectThread(arg); // Select a thread based on preferred scheduling algorithm
        thread_array[running_thread].state = running; // set the selected threads state to running
        if(!(running_thread_temp == running_thread)){ printStatus(); } //Print status unless the same thread is running again
        runThread(&context_array[running_thread]); // Run the selected thread
        // If the thread is finished, free the memory by calling the exitThread func
        if(thread_array[running_thread].state == finished){
            exitThread(&thread_array[running_thread].context);
            // printf("finished state: %d\n", running_thread);
        }
    }
    else { // If all the threads are finished 
        running_thread = sizeof(thread_array) + 10; // Set the running thread to a non existent thread number
        printStatus(); // Print the status for a final time
    }
    
}

// PWF_scheduler() calls the scheduler and Passes argument for neccesary scheduking algorithm type (P&WF with weighted lottary)
void PWF_scheduler (void){
    scheduler(PandWF);
}

// PWF_scheduler() calls the scheduler and Passes argument for neccesary scheduking algorithm type (SRTF)
void SRTF_scheduler (void){
    scheduler(SRTF);
}

int main() // main function
{    

    // Get input from user about the type of scheduling algorithm  
    printf("Select P&WF(0) or SRTF(1): ");
    scanf("%d", &schedulerType);
    if(schedulerType != 0 && schedulerType != 1){
        printf("Invalid input\n");
        return 0;
    }
    printf("\n");
   

    // Get the n values from the user (the values to count up to)
    int nArray[5];
    printf("Enter 'n' values below for each thread (Even positive integers only):\n");    
    for(i=0; i<5; i++){
        printf("n%d: ", i+1);
        scanf("%d", &nArray[i]);
        if(nArray[i]%2 != 0 || nArray[i] < 2 ){
            printf("Invalid Input\n");
            return 0;
        }
    }
    printf("\n");
    

    srand(time(NULL)); // seed the random function
    getcontext(&context_array[0]); // Initialize the context struct for main context
    i = 1;
    
    while(i < 6){ //initialize and create the threads with provided user input
        initializeThread(i, nArray[i-1], &context_array[i]);
        createThread(&context_array[i], &thread_array[i]);
        i++;
    }
    
    while(finishedNum != 5){ // Call the required scheduler based on user input untill all processes are finished
        if(schedulerType == PandWF) {PWF_scheduler();}
        else if( schedulerType == SRTF) {SRTF_scheduler();}
    }
    return 0;
}

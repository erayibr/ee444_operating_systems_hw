// compiles with "gcc -pthread -o main main.c -lm" on ubuntu 20.04 LTS

// Import neccesarry libraries
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <math.h>
#include <unistd.h>

int main(int argc, char *argv[])
{   
    // Define bank client struct
    struct BankClient { 
        int id; // Client ID 
        double duration; // banking transaction duration 
    };

    // Exponential Distribution function
    // Reference: https://stackoverflow.com/questions/34558230/generating-random-numbers-of-exponential-distribution
    double ran_expo(double lambda){
        double u;
        u = rand() / (RAND_MAX + 1.0);
        return -log(1- u) / lambda;
    } 

    // Declare and assign default parameters//
    int c_val = 20, n_val = 4, q_val = 3;
    double g_val = 100;
    double d_val = 10;

    // Get parameters from terminal with getopt()
    // Reference1: https://www.geeksforgeeks.org/getopt-function-in-c-to-parse-command-line-arguments/#:~:text=The%20getopt()%20function%20is,representing%20a%20single%20character%20option.
    // Reference2: http://www2.phys.canterbury.ac.nz/dept/docs/manuals/unix/DEC_4.0e_Docs/HTML/MAN/MAN3/0971____.HTM 
    int opt;  
    while((opt = getopt(argc, argv, "c:n:q:g:d:")) != -1) 
    { 
        switch(opt) 
        { 
            case 'c':
                c_val = atoi(optarg);
                break; 
            case 'n':
                n_val = atoi(optarg);
                break;  
            case 'q': 
                q_val = atoi(optarg);
                break; 
            case 'g': 
                g_val = atoi(optarg);
                break;  
            case 'd': 
                d_val = atoi(optarg);
                break;  
        } 
    } 

    // Print the values of parameters
    printf("NUM_CLIENTS       : %d\n",c_val);
    printf("NUM_DESKS         : %d\n",n_val);
    printf("QUEUE_SIZE        : %d\n",q_val);
    printf("DURATION_RATE     : %f\n",d_val);
    printf("GENERATION_RATE   : %f\n",g_val);
    
    struct BankClient clients[c_val];       // create array of BankClient Structs
    pthread_t generateClient_thread;        // Create pthread variable
    pthread_t generateDesk_thread[n_val];   // Create pthread variable array
    pthread_mutex_t lock;                   // Create mutex lock variable
    int num_of_clients = 0;                 // declare variable to hold number of clients generated
    int served_clients = 0;                 // declare variable to hold number of clients served
    int i = 0;                              // counter variable

    // Function to generate clients
    void *generateClient(void *arg)
    {
        while(num_of_clients < c_val){                      // as long as clients generated are less than total clients keep generating
            pthread_mutex_lock(&lock);                      // lock
            struct BankClient client;                       // declare BankClient struct 
            client.id = num_of_clients;                     // set client id as number of total clients generated
            client.duration = ran_expo(d_val);              // set random client service duration
            clients[num_of_clients] = client;               // push the struct into an array of structs
            printf("Client %d arrived.\n", num_of_clients); // print that new client arrived
            num_of_clients = num_of_clients + 1;            // increment number of clients generated
            pthread_mutex_unlock(&lock);                    // unlock
            double client_sleep_time = ran_expo(g_val);     // generated a random amount of time for sleep
            sleep(client_sleep_time);                       // sleep
        }
        return NULL;
    }

    // Function that generate Desks and Serves Clients
    void *generateDesk(void *arg)
    {   
        int deskNo = *(int*)arg;                            // get desk number passed from main thread
        free(arg);                                          // free memory          
        int client_num;                                     // declare client_num to hold the cleint number being served
        double sleep_duration;                              // declare sleep_duration to hold the duration for client
        while(served_clients < c_val){                      // continue serving as long as served clients are less than total clients
            pthread_mutex_lock(&lock);                      // lock
            int check = (served_clients < num_of_clients);  // check is 1 if served clients are less than currently arrived clients
            if(check){  // serve a client if check = 1    
                sleep_duration = clients[served_clients].duration;  // get sleep duration
                client_num = clients[served_clients].id;            // get id
                served_clients = served_clients + 1;                // increment the number of served clients
            }
            pthread_mutex_unlock(&lock);                    // unlock
            if(check){ // sleep if check = 1
                sleep(sleep_duration); // sleep
                // print message indicating that a client is served in a certain amount of time
                printf("Desk %d served Client %d in %f seconds.\n", deskNo, client_num, sleep_duration); 
            }
        }
        return NULL;
    }

    // Create Desk threads
    while (i < n_val){ // number of n threads
        // dynamically create a pointer to pass desk number to the thread
        int * a = malloc(sizeof(int));
        *a = i;
        // create thread
        pthread_create(&generateDesk_thread[i], NULL, &generateDesk, a);   
        i++;
    }
    
    // Create and join Client generator thread
    pthread_create(&generateClient_thread, NULL, &generateClient, NULL);
    pthread_join(generateClient_thread, NULL);

    // Join Desk threads
    i = 0;
    while (i < n_val){
        pthread_join(generateDesk_thread[i], NULL);   
        i++;
    }

    return 0;
}

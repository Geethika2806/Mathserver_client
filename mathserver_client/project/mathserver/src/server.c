#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <math.h>
#include <stdbool.h>
#include <limits.h>
#include <pthread.h>
#include <sys/wait.h>
#include <assert.h>
#include <fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>


#define EOF_CHAR '#'
#define SERVER_RESULTS_DIR "mathserver/computed_results/"

#define MAX_BUFFER_SIZE 128 
#define MAX_SIZE 4096
#define NUM_THREADSM 4  

#define NUM_THREADS 8
#define MAX_POINTS 4096
#define MAX_CLUSTERS 32

int dp;
int server_socket;
int k;

int send_ack(int client_socket) {
    char ack[] = "ACK";
    if (send(client_socket, ack, strlen(ack), 0) == -1) {
        perror("Error sending ACK to client");
        return 0;  
    }
    return 1;  
}


int receive_ack(int client_socket) {
    char ack[MAX_BUFFER_SIZE];
    ssize_t bytes_received = recv(client_socket, ack, sizeof(ack), 0);
    if (bytes_received <= 0) {
        perror("Error receiving ACK from server");
        return 0;
    }
    return 1;
}



// MATRIX INVERSE PARALLELISED CODE

typedef double matrix[MAX_SIZE][MAX_SIZE];

int N;
int maxnum;
char *Init;
int PRINT;
matrix A;
matrix I ;
pthread_barrier_t barrier;

void find_inverse(void);
void *parallel_inverse(void *arg);
void Init_Matrix(void);


void Write_Matrix_To_File(FILE *file, matrix M, char name[])
{
    int row, col;

    fprintf(file, "%s Matrix:\n", name);
    for (row = 0; row < N; row++) {
        for (col = 0; col < N; col++) {
            fprintf(file, " %5.2f", M[row][col]);
        }
        fprintf(file, "\n");
    }
    fprintf(file, "\n\n");
}


void find_inverse()
{
    int row, col, p; 
    double pivalue; 

    
    for (p = 0; p < N; p++) { 
        pivalue = A[p][p];
        for (col = 0; col < N; col++)
        {
            A[p][col] = A[p][col] / pivalue; 
            I[p][col] = I[p][col] / pivalue; 
        }
        assert(A[p][p] == 1.0);

        double multiplier;
        for (row = 0; row < N; row++) {
            multiplier = A[row][p];
            if (row != p) 
            {
                for (col = 0; col < N; col++)
                {
                    A[row][col] = A[row][col] - A[p][col] * multiplier; 
                    I[row][col] = I[row][col] - I[p][col] * multiplier; 
                }      
                assert(A[row][p] == 0.0);
            }
        }
    }
}

void *parallel_inverse(void *arg)
{
    int row, col, p, start, end;
    int tid = (intptr_t)arg;

    start = (N / NUM_THREADSM) * tid;
    end = (tid == NUM_THREADSM - 1) ? N : start + (N / NUM_THREADSM);

    for (p = 0; p < N; p++)
    {
        pthread_barrier_wait(&barrier);

        if (p >= start && p < end)
        {
            double pivalue = A[p][p];

            for (col = 0; col < N; col++)
            {
                A[p][col] = A[p][col] / pivalue;
                I[p][col] = I[p][col] / pivalue;
            }
            assert(A[p][p] == 1.0);
         }
            
	  pthread_barrier_wait(&barrier);

	    double multiplier;
	    for (row = start; row < end; row++)
	    {
		multiplier = A[row][p];
		if (row != p)
		{
		    for (col = 0; col < N; col++)
		    {
		        A[row][col] = A[row][col] - A[p][col] * multiplier;
		        I[row][col] = I[row][col] - I[p][col] * multiplier;
		    }
		    assert(A[row][p] == 0.0);
		}
	    }

  
    }

    return NULL;
}

void Init_Matrix()
{
    int row, col;

    for (row = 0; row < N; row++) {
        for (col = 0; col < N; col++) {
            if (row == col)
                I[row][col] = 1.0;
            else
                I[row][col] = 0.0;  
        }
    }
    //printf("\nsize      = %dx%d ", N, N);
    //printf("\nmaxnum    = %d \n", maxnum);
    //printf("Init      = %s \n", Init);
    //printf("Initializing matrix...");

    if (strcmp(Init, "rand") == 0)
    {
        for (row = 0; row < N; row++)
        {
            for (col = 0; col < N; col++)
            {
                if (row == col)
                    A[row][col] = (double)(rand() % maxnum) + 5.0;
                else
                    A[row][col] = (double)(rand() % maxnum) + 1.0;
            }
        }
    }
    if (strcmp(Init, "fast") == 0)
    {
        for (row = 0; row < N; row++)
        {
            for (col = 0; col < N; col++)
            {
                if (row == col)
                    A[row][col] = 5.0;
                else
                    A[row][col] = 2.0;
            }
        }
    }

    //printf("done \n\n");
    if (PRINT == 1)
    {
        //Print_Matrix(A, "Begin: Input");
        //Print_Matrix(I, "Begin: Inverse");
    }
}


void Print_Matrix(matrix M, char name[])
{
    int row, col;

    printf("%s Matrix:\n", name);
    for (row = 0; row < N; row++) {
        for (col = 0; col < N; col++)
            printf(" %5.2f", M[row][col]);
        printf("\n");
    }
    printf("\n\n");
}

// K-MEANS PARALLELISED CODE

typedef struct point {
    float x; 
    float y; 
    int cluster; 
} point;

typedef struct thread_args {
    int start;
    int end;
    int k;
} thread_args;

  
point data[MAX_POINTS];     
point cluster[MAX_CLUSTERS]; 

void read_data(const char *file_path, int num_clusters) {
   
    k = num_clusters;
    FILE* fp = fopen(file_path, "r");
    if (fp == NULL) {
        perror("Cannot open the file");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < dp; i++) {
        if (fscanf(fp, "%f %f", &data[i].x, &data[i].y) != 2) {
            fprintf(stderr, "Error reading data point %d\n", i);
            exit(EXIT_FAILURE);
        }
        data[i].cluster = -1;
    }
    //printf("Read the problem data!\n");

    srand(0);
    for (int i = 0; i < k; i++) {
        int r = rand() % dp;
        cluster[i].x = data[r].x;
        cluster[i].y = data[r].y;
    }

    fclose(fp);
}


int get_closest_centroid(int i, int k) {
    int nearest_cluster = -1;
    double xdist, ydist, dist, min_dist;
    min_dist = dist = INT_MAX;
    for (int c = 0; c < k; c++) {
        xdist = data[i].x - cluster[c].x;
        ydist = data[i].y - cluster[c].y;
        dist = xdist * xdist + ydist * ydist;
        if (dist <= min_dist) {
            min_dist = dist;
            nearest_cluster = c;
        }
    }
    return nearest_cluster;
}

bool assign_clusters_to_points() {
    bool something_changed = false;
    int old_cluster = -1, new_cluster = -1;
    for (int i = 0; i < dp; i++) {
        old_cluster = data[i].cluster;
        new_cluster = get_closest_centroid(i, k);
        data[i].cluster = new_cluster;
        if (old_cluster != new_cluster) {
            something_changed = true;
        }
    }
    return something_changed;
}

void update_cluster_centers() {
    int c;
    int count[MAX_CLUSTERS] = { 0 };
    point temp[MAX_CLUSTERS] = { 0.0 };

    for (int i = 0; i < dp; i++) {
        c = data[i].cluster;
        count[c]++;
        temp[c].x += data[i].x;
        temp[c].y += data[i].y;
    }
    for (int i = 0; i < k; i++) {
        cluster[i].x = temp[i].x / count[i];
        cluster[i].y = temp[i].y / count[i];
    }
}

void* assign_clusters_to_points_parallel(void* arg) {
    thread_args* args = (thread_args*)arg;
    int start = args->start;
    int end = args->end;
    int k = args->k;

    bool something_changed = false;
    int old_cluster = -1, new_cluster = -1;
    for (int i = start; i < end; i++) {
        old_cluster = data[i].cluster;
        new_cluster = get_closest_centroid(i, k);
        data[i].cluster = new_cluster;
        if (old_cluster != new_cluster) {
            something_changed = true;
        }
    }

    return (void*)something_changed;
}

int kmeans(int k, const char *file_path) {
    bool somechange;
    int iter = 0;
    do {
        iter++;
        somechange = false;

        pthread_t threads[NUM_THREADS];
        thread_args threadArgs[NUM_THREADS];
        int chunk_size = dp / NUM_THREADS;
        int start = 0;

        for (int i = 0; i < NUM_THREADS; i++) {
            threadArgs[i].start = start;
            threadArgs[i].end = (i == NUM_THREADS - 1) ? dp : start + chunk_size;
            threadArgs[i].k = k;

            pthread_create(&threads[i], NULL, assign_clusters_to_points_parallel, (void*)&threadArgs[i]);

            start += chunk_size;
        }

        for (int i = 0; i < NUM_THREADS; i++) {
            void* result;
            pthread_join(threads[i], &result);
            somechange = somechange || (bool)result;
        }

        update_cluster_centers();
    } while (somechange);

    //printf("Number of iterations taken = %d\n", iter);
    //printf("Computed cluster numbers successfully!\n");
      
    FILE *file = fopen(file_path, "w");
    if (file == NULL) {
        perror("Cannot open the file");
        exit(EXIT_FAILURE);
    }
    
    for (int i = 0; i < dp; i++) {
        fprintf(file, "%.2f %.2f %d\n", data[i].x, data[i].y, data[i].cluster);
    }
    fclose(file);
}

// A FUNCTION TO HANDLE THE CLIENT'S REQUEST

void handle_client(int client_socket, int current_client) {
    char user_input[MAX_BUFFER_SIZE];
    ssize_t bytes_received;
    int kmeanspar_count = 1; 
    int matinvpar_count = 1; 
    int file_number = 1;
    char result_file_path[MAX_BUFFER_SIZE];
    char result_file_name[MAX_BUFFER_SIZE];
    while ((bytes_received = recv(client_socket, user_input, sizeof(user_input), 0)) > 0) {
        user_input[bytes_received] = '\0';
        printf("Client %d commanded : %s",current_client,user_input);
        char *token = strtok(user_input, " ");
        char resultk_file_name[MAX_BUFFER_SIZE];
        char resultm_file_name[MAX_BUFFER_SIZE];
                
        if (strcmp(user_input, "kmeanspar") == 0) {
            
            snprintf(resultk_file_name, sizeof(resultk_file_name), "mathserver/computed_results/client%d_kmeanspar_result%d", current_client, kmeanspar_count);
            snprintf(result_file_name, sizeof(result_file_name), "client%d_kmeanspar_result%d", current_client, kmeanspar_count);
            strncpy(result_file_path, resultk_file_name, sizeof(result_file_path));
            char *result_file_name = basename(result_file_path);
            kmeanspar_count++;
            char recdata_filename[MAX_BUFFER_SIZE];
            int num_clusters=-1;
            snprintf(recdata_filename, sizeof(recdata_filename), "mathserver/recv_files/recdata%d", file_number);
            while (token != NULL) {
                if (strcmp(token, "-f") == 0) {
                    token = strtok(NULL, " ");
                   
                } else if (strcmp(token, "-k") == 0) {
                    token = strtok(NULL, " ");
                    num_clusters = atoi(token);
                }
                token = strtok(NULL, " ");
                
            }
            
            
         if (send_ack(client_socket)) {
                
                int file_fd = open(recdata_filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (file_fd == -1) {
                    perror("Error opening received file for writing");
                    close(client_socket);
                    return;
                }
                  
                char file_buffer[MAX_BUFFER_SIZE];
                ssize_t sent_bytes;
                int line_count=0;
                while (1) {
                    sent_bytes = recv(client_socket, file_buffer, sizeof(file_buffer), 0);
                    if (sent_bytes <= 0) {
                        if (sent_bytes < 0) {
                            perror("Error receiving data from client");
                        }
                        break;
                    }

                    write(file_fd, file_buffer, sent_bytes);
                    
                    for (int i = 0; i < sent_bytes; i++){
                       if (file_buffer[i] == '\n') {
                                line_count++ ;
                          }
                         }

                    if (strstr(file_buffer, "END_OF_DATA") != NULL) {
                        break;
                    }
                }
                
                dp = line_count-1;

       
                close(file_fd);

                // printf("Received file saved to: %s\n", );
                //printf("Number of clusters received: %d\n", num_clusters);
                read_data(recdata_filename, num_clusters);
                kmeans(num_clusters, resultk_file_name);
                file_number++;
                dp = 0;
            } 
        
        
            else {
            fprintf(stderr, "Failed to send ACK to the client.\n");
               }	           
        }
        
        else if (strcmp(token, "matinvpar") == 0) {
        snprintf(resultm_file_name, sizeof(resultm_file_name), "mathserver/computed_results/client%d_matinvpar%d_result", current_client, matinvpar_count);
        snprintf(result_file_name, sizeof(result_file_name), "client%d_matinvpar%d_result", current_client, matinvpar_count);
        matinvpar_count++; // Increment the matinvpar counter
        strncpy(result_file_path, resultm_file_name, sizeof(result_file_path));
        // Default values
         N = 5;       
         maxnum=15;    
         Init = malloc(strlen("fast") + 1); 
         if (Init != NULL) {strcpy(Init, "fast");}
         PRINT=1;      
                    

                    while (token != NULL) {
                        if (strcmp(token, "-n") == 0) {
                            token = strtok(NULL, " ");
                            N = atoi(token);
                        } else if (strcmp(token, "-P") == 0) {
                            token = strtok(NULL, " ");
                            PRINT = atoi(token);
                        } else if (strcmp(token, "-m") == 0) {
                            token = strtok(NULL, " ");
                            maxnum = atoi(token); }
                        else if (strcmp(token, "-I") == 0) {
                            token = strtok(NULL, " ");
                             Init = (char*)malloc(strlen(token) + 1);
                             if (Init != NULL) {
			     strcpy(Init, token);
			     size_t len = strcspn(Init, "\n");
				    if (Init[len] == '\n') {
					Init[len] = '\0'; 
				    }
			  	}                        
                        }
                        token = strtok(NULL, " ");
                    }

                    
                    
    //printf("Matrix Inverse\n");
    Init_Matrix();
    pthread_barrier_init(&barrier, NULL, NUM_THREADSM);

    pthread_t threads[NUM_THREADSM];

    for (int i = 0; i < NUM_THREADSM; ++i)
    {
        if (pthread_create(&threads[i], NULL, parallel_inverse, (void *)(intptr_t)i) != 0)
        {
            fprintf(stderr, "Error creating thread %d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < NUM_THREADSM; ++i)
    {
        if (pthread_join(threads[i], NULL) != 0)
        {
            fprintf(stderr, "Error joining thread %d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    
    pthread_barrier_destroy(&barrier);
        
    FILE *outputFile;
    outputFile = fopen(resultm_file_name, "w");

    Write_Matrix_To_File(outputFile, A, "Initialized");  

    fprintf(outputFile, "\n\n");  // Add 2 lines space

    Write_Matrix_To_File(outputFile, I, "Inversed");  

    fclose(outputFile);  // Close the file
    free(Init);
    
        } 
        else {
            printf("Unknown command: %s\n", user_input);
        }
     
     printf("Sending solution: %s.txt\n", result_file_name);
     
     
    //Sending file back to the client       
    send(client_socket,result_file_name, strlen(result_file_name), 0);
    if (receive_ack(client_socket)) {       
    int file_fd2 = open(result_file_path, O_RDONLY);
    if (file_fd2 == -1) {
        perror("Error opening file");
        close(client_socket);
        return 1;
    }

    off_t offset = 0;
    struct stat file_info;
    
    if (fstat(file_fd2, &file_info) < 0) {
        perror("Error getting file information");
        close(file_fd2);
        close(client_socket);
        return 1;
    }

    off_t total_bytes_to_send = file_info.st_size;
    
    
    while (total_bytes_to_send > 0) {
        ssize_t sent_bytes = sendfile(client_socket, file_fd2, &offset, MAX_BUFFER_SIZE);
        if (sent_bytes == -1) {
            perror("Error sending file");
            close(file_fd2);
            close(client_socket);
            return 1;
        }
        
        total_bytes_to_send -= sent_bytes;
    }
    
   
    const char *end_of_data = "\nEND_OF_DATA";
    send(client_socket, end_of_data, strlen(end_of_data), 0);
     close(file_fd2);
     }
              
             }
    
   
    close(client_socket);
    exit(0);
}



void clean_up() {
    
    kill(0, SIGKILL);  
    close(server_socket);
    exit(0);
}

int main(int argc, char *argv[]) {
   
    signal(SIGINT, clean_up);
    char base_directory[MAX_BUFFER_SIZE];
    int server_port;
    bool daemon_mode = false;
    int option;
    while ((option = getopt(argc, argv, "p:hd")) != -1) {
        switch (option) {
            case 'p':
                server_port = atoi(optarg);
                break;
            case 'h':
                printf("Usage: %s -p <server_port> -h -d\n", argv[0]);
                printf("-p <server_port>  : Specify the server port\n");
                printf("-h                : Display help\n");
                printf("-d                : Run as a daemon\n");
                return 0;
            case 'd':
                daemon_mode = true;
                
                break;
            default:
                fprintf(stderr, "Usage: %s -p <server_port> -h -d \n", argv[0]);
                return 1;
        }
    }


void daemon(){

    umask(0);

    int pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        return 1;
    }

    if (pid > 0) {
        exit(0);
    }
 
    setsid();
  
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
  
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_WRONLY);
    open("/dev/null", O_WRONLY);


} 

   if( daemon_mode == true) {
  
       daemon();
     
     }
     
     
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error creating socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error binding");
        return 1;
    }

    if (listen(server_socket, 5) == -1) {
        perror("Error listening");
        return 1;
    }

    printf("Listening to clients on port %d\n", server_port);

    int client_count = 1;
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_size = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_size);
        if (client_socket == -1) {
            perror("Error accepting connection");
            return 1;
        }

        pid_t child_pid = fork();

        if (child_pid < 0) {
            perror("Fork failed");
            return 1;
        }

        if (child_pid == 0) {
           
            printf("Client %d connected from %s:%d\n", client_count, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            handle_client(client_socket,client_count);

            
        } else {
       
            close(client_socket);
            client_count++;
            
            int status;
            pid_t terminated_child_pid;
            while ((terminated_child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
                printf("Child process %d terminated.\n", terminated_child_pid);
            }
        }
    }

    close(server_socket);
    return 0;
    
    
}

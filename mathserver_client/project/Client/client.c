#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define EOF_CHAR '#'
#define MAX_BUFFER_SIZE 128

int receive_ack(int client_socket) {
    char ack[MAX_BUFFER_SIZE];
    ssize_t bytes_received = recv(client_socket, ack, sizeof(ack), 0);
    if (bytes_received <= 0) {
        perror("Error receiving ACK from server");
        return 0;
    }
    return 1;
    
}

int send_ack(int client_socket) {
    char ack[] = "ACK";
    if (send(client_socket, ack, strlen(ack), 0) == -1) {
        perror("Error sending ACK to client");
        return 0;  
    }
    return 1;
}


int main(int argc, char *argv[]) {
    if (argc != 5 || strcmp(argv[1], "-ip") != 0 || strcmp(argv[3], "-p") != 0) {
        fprintf(stderr, "Usage: %s -ip <server_ip> -p <server_port>\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[2];
    int server_port = atoi(argv[4]);
    
        int client_socket = socket(AF_INET, SOCK_STREAM, 0);
        int server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket == -1) {
            perror("Error creating socket");
            return 1;
        }

        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);
        inet_pton(AF_INET, server_ip, &(server_addr.sin_addr));

        if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
            perror("Error connecting to the server");
            return 1;
        }

        printf("Connected to server at %s:%d\n", server_ip, server_port);

        while (1) {
 
        start: 
        char user_input[4096];  
        char user_input_copy[4096];
        memset(user_input, 0, sizeof(user_input));

        printf("Enter a command for the server: ");
        fgets(user_input, sizeof(user_input), stdin);
	strcpy(user_input_copy, user_input);
        
        char *token = strtok(user_input, " ");

        if (token != NULL && strcmp(token, "kmeanspar") == 0) {
        
            char *file_path = NULL;
            int num_clusters = -1;
            token = strtok(NULL, " ");
            
            while (token != NULL) {
                if (strcmp(token, "-f") == 0) {
                    token = strtok(NULL, " ");
                    file_path = token;
                } else if (strcmp(token, "-k") == 0) {
                    token = strtok(NULL, " ");
                    num_clusters = atoi(token);
                }
                
                else {
                printf("Invalid command!\nEnter:\n\tkmeanspar -f <data.txt> -k <cluster number>\n\n");
                goto start; 
                }
                
               
                token = strtok(NULL, " ");
            }

            if (num_clusters == -1 || file_path == NULL) {
                printf("Invalid command!\nEnter:\n\tkmeanspar -f <data.txt> -k <cluster number>\n\n");
                goto start;      
            }
            
            send(client_socket, user_input_copy, strlen(user_input_copy), 0);

            if (receive_ack(client_socket)) {
            int file_fd = open(file_path, O_RDONLY);
	    if (file_fd == -1) {
		perror("Error opening file");
		close(client_socket);
		return 1;
	    }

	    off_t offset = 0;
	    struct stat file_info;
	    
	    if (fstat(file_fd, &file_info) < 0) {
		perror("Error getting file information");
		close(file_fd);
		close(client_socket);
		return 1;
	    }

	    off_t total_bytes_to_send = file_info.st_size;

	    while (total_bytes_to_send > 0) {
		ssize_t sent_bytes = sendfile(client_socket, file_fd, &offset, MAX_BUFFER_SIZE);
		if (sent_bytes == -1) {
		    perror("Error sending file");
		    close(file_fd);
		    close(client_socket);
		    return 1;
		}
		
		total_bytes_to_send -= sent_bytes;
	    }

	    const char *end_of_data = "END_OF_DATA\n";
	    send(client_socket, end_of_data, strlen(end_of_data), 0);

	    close(file_fd);

		    } else {
		        fprintf(stderr, "Failed to receive ACK from the server.\n");
		    }
		
	   }    
		
	    else if (token != NULL && strcmp(token, "matinvpar") == 0)
	    {
	        char *Init;
	    	int n_flag = 0, I_flag = 0, P_flag = 0, m_flag = 0;
	    	token = strtok(NULL, " ");
	    	        while (token != NULL) {
		                if (strcmp(token, "-n") == 0) {
		                    token = strtok(NULL, " ");
		                    n_flag=1;
		                } else if (strcmp(token, "-P") == 0) {
		                    token = strtok(NULL, " ");
		                    P_flag=1;
		                } else if (strcmp(token, "-m") == 0) {
		                    token = strtok(NULL, " ");
		                    m_flag=1;}
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
				 
		                 if (strcmp(Init, "rand") != 0 && strcmp(Init, "fast") != 0) {
				    printf("Invalid value for -I parameter. Please use 'rand' or 'fast'.\n");
				    goto start;
				}
				  
				  	I_flag =1;                       
		                }
		                
		                else {printf("Invalid arguments!\nEnter:\n\tmatinvpar -n <size> -I <rand/fast> -P <0/1> -m <maxnum>\n\n");
		                goto start;
		                }
		                
		                
		                token = strtok(NULL, " ");
		            }
	    	        
		send(client_socket, user_input_copy, strlen(user_input_copy), 0);
	    
	    }
		
	    else 
	    {
	       
	       printf("Invalid command.\nEnter any:\n\tmatinvpar -n <size> -I <rand/fast> -P <0/1> -m <maxnum>.\n\tkmeanspar -f <data.txt> -k <cluster number>\n\n");
	       goto start;
	    
	    }
		  
	     
	    //Receiving the file from server  
	     
	    char file_name[MAX_BUFFER_SIZE];
	    char file_path[MAX_BUFFER_SIZE];
	    ssize_t bytes_received = recv(client_socket, file_name, sizeof(file_name), 0); 
	    if (bytes_received == -1) {
		perror("Error receiving file name");
		close(client_socket);
		exit(EXIT_FAILURE);
	    } 
	    file_name[bytes_received] = '\0';
	    
	    if (send_ack(client_socket))
	    {
	    snprintf(file_path, sizeof(file_path), "Client/results/%s", file_name);
	    int file_fd2 = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	    if (file_fd2 == -1) {
		perror("Error creating file on client");
		close(client_socket);
		exit(EXIT_FAILURE);
	    }

	    char file_buffer[MAX_BUFFER_SIZE];
	    ssize_t received_bytes;

	 while ((received_bytes = recv(client_socket, file_buffer, sizeof(file_buffer), 0)) > 0) {
	    
	    if (received_bytes >= sizeof("\nEND_OF_DATA") - 1 &&
		strncmp(file_buffer + received_bytes - sizeof("\nEND_OF_DATA") + 1, "\nEND_OF_DATA", sizeof("\nEND_OF_DATA") - 1) == 0) {
		write(file_fd2, file_buffer, received_bytes - sizeof("\nEND_OF_DATA") + 1);
		break;
	    }
	    
	    write(file_fd2, file_buffer, received_bytes);
	}
	    
	  printf("Received the solution: %s.txt\n", file_name);
	  
	  close(file_fd2);  
	}
		   
	   }   
	   
	    close(client_socket);

	    return 0;
	}


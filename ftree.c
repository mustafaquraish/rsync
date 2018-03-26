#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <libgen.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>

#include "ftree.h"
#include "hash.h"

// ---------- FOR DEBUGGING, FROM A1 ----------- //
// void show_hash(char *hash_val, long block_size) {
//     for(int i = 0; i < block_size; i++) {
//         printf("%.2hhx ", hash_val[i]);
//     }
//     printf("\n");
// }

// Function Prototypes
int filetree(char*src, char*dest, int socketfd);
int receive_file(struct fileinfo file, int clientfd);

// Main Client Code
int fcopy_client(char *src_path, char *dest_path, char *host, int port){

	// Inititalizing int, creating struct and clearing memory
	int socketfd;
	struct sockaddr_in server_address;
	memset(&server_address, '\0', sizeof(server_address));

	// Creating socket
	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("socket");
		return -1;
	}

	// Adding server address info. to struct
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(PORT);

	// Assigning IP address to struct
	if (inet_pton(AF_INET, host, &server_address.sin_addr) < 0) {
	    perror("fcopy_cient: inet_pton");
	    close(socketfd);
	    return -1;
  	}
		// Connecting to the socket
  	if (connect(socketfd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
	    perror("fcopy_client: connect");
	    return -1;
	 }

	// Calling recursive file-tracing function
	// with socket file descritor
	 filetree(src_path, dest_path, socketfd);

	return 0;
}

// Function to traverse file structure / copy
int filetree(char*src, char*dest, int socketfd){

	// Inititalizing variables
	struct stat src_buf;
	char path[MAXPATH];
	char *hash_val;
	char *store = malloc(MAXPATH);
	uint32_t filemode, filesize;
	FILE * f;

	// If the source doesn't exist
	if (lstat(src, &src_buf)!=0){
		perror("lstat");
		return -1;

	// The source exists, Initialize variables
	} else {

		printf("path: %s\n", src );
		realpath(dest, path);
		realpath(src, store);
		strcat(path, "/");
		strcat(path, basename((char*)store));
		filemode = htonl(src_buf.st_mode);
		filesize = htonl(src_buf.st_size);
	}

	// It's a File
	if (S_ISREG(src_buf.st_mode)){

		printf("Regular file\n" );
		// Generating hash for file
		f = fopen(src, "r");
		hash_val = hash(f);

		// Writing info to the socket
		write(socketfd, path, MAXPATH);
		write(socketfd, &filemode, sizeof(filemode));
		write(socketfd, &filesize, sizeof(filesize));
		write(socketfd, hash_val, HASH_SIZE);

		// Getting response
		uint16_t response;
		read(socketfd, &response, sizeof(response));
		response = ntohs(response);

		// If response is MISMATCH
		if (response == MISMATCH){

			printf("Sending file, %ld bytes\n",src_buf.st_size);

			// Copy over file
			char c;
			rewind(f);
			for (int i = 0; i <src_buf.st_size; i++){
				fread(&c,sizeof(char),1,f);
				write(socketfd, &c, sizeof(char));
			}

			fclose(f);

			printf("File Sent\n");

		// MATCH_ERROR signals type mismatch
		} else if (response == MATCH_ERROR){


			fclose(f);
			printf("Type Mismatch\n");
			return -1;

		// File already exists
		} else {

			fclose(f);

		}

		free (store);

	// If it's a directory
	} else if (S_ISDIR(src_buf.st_mode)){

		// No hash for directory, so make empty string
		char emptyhash[8] = "";

		// Write into to socket
		write(socketfd, path, MAXPATH);
		write(socketfd, &filemode, sizeof(filemode));
		write(socketfd, &filesize, sizeof(filesize));
		write(socketfd, emptyhash, HASH_SIZE);

		// Get response
		uint16_t response;
		read(socketfd, &response, sizeof(response));
		response = ntohs(response);

		// Directory exists, or was created
		if (response == MATCH){

			// Open local directory
			DIR *dir = opendir(src);
			struct dirent* entry;

			// Loop through
			while ((entry = readdir(dir)) != NULL){

				// Ignore hidden, current and parent
				if (entry->d_name[0]=='.'){
					continue;
				}

				// Create full path
				strcpy(store, src);
				strcat(store,"/");
				strcat(store,entry->d_name);

				// Make recursive call
				filetree(store, path, socketfd);

			}

			closedir(dir);

		// Else there's an error
		} else {

		// Error! Do error handling.

		}
	}

	return 0;
}

// Main Server code
void fcopy_server(int port) {

	// Inititalizing Variables
	int socketfd, clientfd, nbytes;
	struct sockaddr_in self, client;
	struct fileinfo file;
	char * hash_val;

	// Clear memory in struct
	memset(&self, '\0', sizeof(self));

	// Create socket
	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("socket");
		// return -1;
	}

	// Adding server address info to struct
	self.sin_family = AF_INET;
	self.sin_addr.s_addr = INADDR_ANY;
	self.sin_port = htons(PORT);
	printf("Listening on %d\n", PORT);

	// Bind the socket to the port
	if (bind(socketfd, (struct sockaddr *) &self, sizeof(self)) == -1) {
		perror("bind"); // probably means port is in use
		exit(1);
	}

	// Listen on the PORT, max of 5 connections backlogged
	if (listen(socketfd, 5) < 0) {
    perror("server: listen");
    close(socketfd);
    exit(1);
  }

	// Server runs on loop
	while (1) {

    // Note that we're passing in valid pointers for the second and third
    // arguments to accept here, so we can actually store and use client
    // information.

		// Accept client and store ID
    clientfd = accept(socketfd, NULL, NULL);

		// If there's an error connecting
    if (clientfd < 0) {
      perror("accept");

		// Otherwise we are connected
    } else {

      printf("New connection on port %d\n", ntohs(client.sin_port));

			// Inititalizing variables for loop
			uint32_t recmode, recsize;
			struct stat * file_buf = malloc(sizeof(struct stat));

      // Receive Struct in parts, path first
      while ((nbytes = read(clientfd, file.path, MAXPATH)) > 0) {

				// Reading from Socket
        read(clientfd, &recmode, sizeof(recmode));
				read(clientfd, &recsize, sizeof(recsize));
				read(clientfd, file.hash, 8);

				// Converting back numbers
				file.size = ntohl(recsize);
				file.mode = ntohl(recmode);

				// printf("Path: %s \n", file.path);
        // printf("Hash: %s \n", file.hash);
        // printf("Size: %ld \n", file.size);
        // printf("Mode: %d \n", file.mode);


				// If the path does not exist
        if (lstat(file.path, file_buf)<0){

					// If there is a hash, it's a file
        	if (strcmp(file.hash,"")!=0){

						// Sending MISMATCH and receive file
        		uint16_t ret = htons(MISMATCH);
        		write(clientfd, &ret, sizeof(uint16_t));
						printf("Requesting Copy\n");
        		receive_file(file, clientfd);

					// If there isn't a hash, it's a directory
        	} else if (strcmp(file.hash,"")==0){

						// Create the directory and send a MATCH
        		mkdir(file.path,file.mode & 0777);
        		uint16_t ret = htons(MATCH);
        		write(clientfd, &ret, sizeof(uint16_t));

        	}

				// Else if path points to a file
        } else if (S_ISREG(file_buf->st_mode)){

					// Check for type mismatch
					if (S_ISDIR(file.mode)) {

						uint16_t ret = htons(MATCH_ERROR);
        		write(clientfd, &ret, sizeof(uint16_t));

					// Else if file sizes are different
					} else if (file_buf->st_size != file.size) {

						// Send MISMATCH and receive file
        		uint16_t ret = htons(MISMATCH);
        		write(clientfd, &ret, sizeof(uint16_t));
        		printf("Requesting Copy\n");
        		receive_file(file, clientfd);

					// File sizes are the same
        	} else {

						// Generate hash for file on server
        		hash_val = hash(fopen(file.path,"r"));

						// If the hashes are different
        		if (memcmp(hash_val, file.hash, HASH_SIZE)!=0){

        			// Send MISMATCH and receive file
        			uint16_t ret = htons(MISMATCH);
        			write(clientfd, &ret, sizeof(uint16_t));
        			printf("Requesting Copy\n");
        			receive_file(file, clientfd);

						// Hashes are same, File exists already
        		} else {

							printf("FILE ALREADY EXISTS\n");

							// send MATCH and change permissions
        			uint16_t ret = htons(MATCH);
        			chmod(file.path, file.mode & 0777);
        			write(clientfd, &ret, sizeof(uint16_t));

        		}
        	}

				// Else if the path is a directory
        } else if (S_ISDIR(file_buf->st_mode)){

					// Check for type mismatch
					if (S_ISREG(file.mode)) {

						uint16_t ret = htons(MATCH_ERROR);
	        	write(clientfd, &ret, sizeof(uint16_t));

					// Directory exists already
					} else {

						// Send MATCH and change permissions
	        	uint16_t ret = htons(MATCH);
	        	write(clientfd, &ret, sizeof(uint16_t));
	        	chmod(file.path, file.mode & 0777);

					}
        }
      }

			// Free memory and close client connection
			free (file_buf);
      close(clientfd);

    }

	// Main while loop keeps running
  }
}

// Helper function to receive files
int receive_file(struct fileinfo file, int clientfd){
	printf("%ld\n", file.size);

	// Open / create file
	FILE *f = fopen(file.path, "w+");

	// Get file and write
	char buf;
	for (int i = 0; i < file.size;i++){
		read(clientfd, &buf, sizeof(char));
		fwrite(&buf,1,1,f);
	}

	// Close and change permissions
	fclose(f);
	chmod(file.path, file.mode&0777);

	return 0;
}

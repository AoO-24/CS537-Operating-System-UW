#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
    Helper func that used to loacte the file at directory and open the file, print all the string in that file;
    return 0 if successfully open the file 
    return -1 if can't find the filehttps://stackoverflow.com/questions/8149569/scan-a-directory-to-find-files-in-c?noredirect=1&lq=1
    reference: 
*/
/* no longer using
int open_file(char *file_name, char *path){
    DIR *dp;
    //rd is the pionter to the return val of readdir
    struct dirent *rd;
    //statbuf is the 
    struct stat statbuf;
    char content[1000];
    fprintf(stderr,"now open: %s\n", path);
    //keep reading all file in the path
    if((dp = opendir(path)) == NULL) {
        fprintf(stderr,"cannot open directory: %s\n", path);
        return -1;
    }
    //change working directory to the directory we open
    chdir(path);
    //When file is a regular file. and name are same with file_name break the loop. find the file.
    FILE *fp = fopen(file_name, "r");
            if(fp != NULL){
                // Loop to read and print the entire content of the file
                while (fgets(content, sizeof(content), fp) != NULL) {
                    printf("%s", content);
                }
                fclose(fp);  // Close the file after reading
                return 0;  // File found and opened successfully
            }
    while((rd = readdir(dp)) != NULL ){
        fprintf(stderr,"now checking: %s\n", rd->d_name);
        lstat(rd->d_name,&statbuf);
        if(S_ISDIR(statbuf.st_mode)){
            //avoid to search through the "." and ".." directory
            if(strcmp(".",rd->d_name) == 0 || 
            strcmp("..",rd->d_name)==0){
                continue;
            }else{
                open_file(file_name, rd->d_name);
            }
            //TODO open next dir to find the file if return = 0. end the loop
            // // If file is found in a subdirectory, return immediately
            // if(open_file(file_name, rd->d_name) == 0) {
            //     return 0;
            // }
        }
        else{
            continue;
        }   
    }
    chdir("..");
    closedir(dp);
    return -1;
}
*/
//func check the file is in the directory if it is there, print the file and return 1 if not return 0
int wman(char *file_name, char *path){
    char buf[100];
    snprintf(buf, 100,"%s/%s.1", path, file_name);//creat the buf that combine the file name with the path
    //printf("%s\n", buf); test output
    FILE *fp = fopen(buf, "r");
    if (fp == NULL) {//can't find the file
        //fclose(fp);
        return 0;//can't ipen the file
    }
    char outbuf[512];
    while(fgets(outbuf, 512, fp)){//print out the hole file
        printf("%s", outbuf);
    }
    fclose(fp);
    return 1;
}

int main(int argc, char *argv[]){   
    //inital the file name str pointer
    char *file_name;
    //one arguemnt need to find the file at the hole directory
    if(argc == 2){
        //path that starts search
        file_name = argv[1];
        int found = 0;//indicator that file is found or not
        for(int i = 1; i<10; i++){
            char buf[100];//create a buffer for the file path
            snprintf(buf, 100, "./man_pages/man%d", i);
            found = wman(file_name, buf);
            if(found){//if found return if not continue the loop
                return 0;
            }
        }
        //can't find the file in the directory
        printf("No manual entry for %s\n", file_name);
    }
    //two arguement need to loate a directory
    else if(argc > 2){
        //take the page number from the user input
        int pageNumber;
        pageNumber = atoi(argv[1]);
        file_name = argv[2];
        //if pagenumber is wrong return 1
        if(pageNumber < 1 || pageNumber > 9){
            printf("invalid section\n");
            exit(1);
        }
        //path that need to get fixed
        char path[100] = "./man_pages/man";
        //add the arguement from user input to the path
        sprintf(path, "./man_pages/man%i", pageNumber);
        //get the name of the file
        //if return value = 0 found the file
        if(wman(file_name, path)){
        
        }else{
            printf("No manual entry for %s in section %i\n", file_name, pageNumber);
            return 0;
        }
    }
    //no arguement given by user
    else if(argc < 2){
        printf("What manual page do you want?\nFor example, try 'wman wman'\n" );
    }
    return 0;
}
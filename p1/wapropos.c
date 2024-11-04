#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

// Helper function to search the given term at the specific path, if founded the term print the NAME part
// reference: https://stackoverflow.com/questions/8149569/scan-a-directory-to-find-files-in-c?noredirect=1&lq=1
int open_file(const char *keyword, const char *path, const int section)
{
    DIR *dp;
    // rd is the pionter to the return val of readdir
    struct dirent *rd;
    char content[1000];
    int found_dir = 0;
    //fprintf(stderr, "now open: %s\n", path);
    // keep reading all file in the path
    if ((dp = opendir(path)) == NULL)
    {
        fprintf(stderr, "cannot open directory: %s\n", path);
        return 0;
    }

    while ((rd = readdir(dp)) != NULL)
    {
        //fprintf(stderr, "now checking: %s\n", rd->d_name);
        if (DT_REG == rd->d_type)
        { // It is a reg file start to read
            char buf[518];
            int found_file = 0;
            snprintf(buf, sizeof(buf), "%s%s", path, rd->d_name);
            // When file is a regular file. and name are same with file_name break the loop. find the file.
            FILE *fp = fopen(buf, "r");
            char *name_section = malloc(1000);
            // char des_section[5000] = "";
            //  Loop to read and print the entire content of the file
            while (fgets(content, sizeof(content), fp) != NULL)
            {
                // printf("%s\n",content);
                if (strstr(content, "[1mNAME[0m"))
                {
                    char line[518];
                    while (fgets(line, sizeof(line), fp) != NULL)
                    {
                        if (line[0] == '\n')
                        {
                            break;
                        }
                        if (strstr(line, keyword))
                        {
                            found_file = 1;
                            // printf("%s (%d) - %s", page, section, strchr(line, '-') + 2);
                        }
                        strcat(name_section, line);
                    }
                }
                if (strstr(content, "[1mDESCRIPTION[0m"))
                {
                    char line[518];
                    while (fgets(line, sizeof(line), fp) != NULL)
                    {
                        if (line[0] == '\n')
                        {
                            break;
                        }
                        if (strstr(line, keyword))
                        {
                            found_file = 1;
                        }
                        // printf(line);
                        // strcat(des_section, line);//not using
                    }
                }
            }
            fclose(fp); // Close the file after reading
            if (found_file)
            {
                // TODO print out the description
                found_dir = 1;
                char *trim_name_section = strchr(name_section, '-');
                char *file_name = strtok(rd->d_name, ".");
                //printf("file_name is: %s\n", file_name);
                //printf("section is: %i\n", section);
                //printf("name_section is: %s\n", trim_name_section);

                printf("%s (%i) %s", file_name, section, trim_name_section);
            }
            free(name_section);
        }
    }
    closedir(dp);
    return found_dir;
}
int main(int argc, char *argv[])
{
    if (argc < 2)
    { // no argv is given
        printf("wapropos what?\n");
        return 0;
    }
    char *keyword = argv[1]; // get keyword from argv(user input)
    int found = 0;
    for (int i = 1; i < 10; i++)
    {
        char path[518];
        sprintf(path, "./man_pages/man%i/", i);
        if(open_file(keyword, path, i)){
            found = 1;
        }
    }
    if (found)
    {}
    else
    {
        printf("nothing appropriate\n");
    }
    return 0;
}
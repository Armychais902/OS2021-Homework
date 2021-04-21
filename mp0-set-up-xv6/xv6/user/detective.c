#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
void findFile(char *path, char *commission,int *obtain){
    //fprintf(2,"test user trap: %s\n",path);
    char buf[512];
    char *fpath;
    int fd;
    struct dirent dir;
    struct stat st;
    /* struct dirent
    directory is a file containing a sequence of dirent structures.
    #define DIRSIZ 14
    struct dirent {
        ushort inum;
        char name[DIRSIZ];
    };
    */

    if((fd=open(path,0))<0){
        fprintf(2,"cannot open %s\n",path);
        return;
    }

    /* fstat(): info of the open file (by fd)
     * stat(): info of named file (by filename)
     */
    if(fstat(fd,&st)<0){
        fprintf(2,"cannot stat %s\n",path);
        close(fd);
        return;
    }

    /* consider T_DIR */
    if(st.type==T_DIR){
        if(strlen(path)+1+DIRSIZ+1 > sizeof(buf)){
            fprintf(2,"path too long\n");
            close(fd);
            return;
        }
        strcpy(buf,path);
        fpath=buf+strlen(buf);
        *fpath++='/';
        /* fpath move to after '/'
         * '/' being added to buf
         * explanation: https://tclin914.github.io/e9206a47/
         */
        int pid=getpid();
        while(read(fd,&dir,sizeof(dir))==sizeof(dir)){
            if(dir.inum==0)
                continue;
            if(!strcmp(dir.name,".") || !strcmp(dir.name,".."))
                continue;
            memmove(fpath,dir.name,DIRSIZ);
            fpath[DIRSIZ]=0;
            if(stat(buf, &st) < 0){
                fprintf(2,"read dir: cannot stat %s\n", buf);
                continue;
            }
            //printf("%s %d %d %d\n", buf, st.type, st.ino, st.size);
            findFile(buf,commission,obtain);
            if(strcmp(fpath,commission)==0){
                *obtain=1;
                printf("%d as Watson: %s\n",pid,buf);
            }
        }
    }   
    /* consider T_FILE or other:
     *  nothing to do, simply close fd and return.
     *  if T_FILE, has been compared while iterating through dir.
     * original approach:
     *  switch between case T_FILE and T_DIR, in T_FILE, directly return
     *  => didn't close fd, fds not enough
     */
    close(fd);
}
int main(int argc,char *argv[]){
    if(argc<2)
        fprintf(2,"wrong format");
    char commission[DIRSIZ+1];
    strcpy(commission,argv[1]);
    //fprintf(2,"commision: %s\n",commission);

    int pfd[2];
    pipe(pfd);
    int pid=fork();
    char yn[5];
    if(pid>0){
        close(pfd[1]);
        int n=read(pfd[0],yn,1);
        if(n==0)
            fprintf(2,"end of file\n");
        if(n<0)
            fprintf(2,"read error\n");
        int prid=getpid();
        if(yn[0]=='y')
            printf("%d as Holmes: This is the evidence\n",prid);
        else
            printf("%d as Holmes: This is the alibi\n",prid);
        pid=wait((int *)0);
        //fprintf(2,"child %d is done\n",pid);
    }else if(pid==0){
        close(pfd[0]);
        //fprintf(2,"child: exiting\n");
        // traverse, DFS
        int obtain=0;
        findFile(".",commission,&obtain);
        if(obtain)
            write(pfd[1],"y",1);
        else
            write(pfd[1],"n",1);
        close(pfd[1]);
        exit(0);
    }else{
        fprintf(2,"fork error\n");
    }
    exit(0);
}
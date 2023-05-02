﻿//和信号有关的函数放这里

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>    
#include <errno.h>    
#include <sys/wait.h>  

#include "ngx_global.h"
#include "ngx_macro.h"
#include "ngx_func.h" 

//一个信号有关的结构 ngx_signal_t
typedef struct 
{
    int           signo;       //信号对应的数字编号 ，每个信号都有对应的#define ，大家已经学过了 
    const  char   *signame;    //信号对应的中文名字 ，比如SIGHUP 

    void  (*handler)(int signo, siginfo_t *siginfo, void *ucontext);
} ngx_signal_t;

//声明一个信号处理函数
static void ngx_signal_handler(int signo, siginfo_t *siginfo, void *ucontext); 
static void ngx_process_get_status(void);                                      //获取子进程的结束状态，防止单独kill子进程时子进程变成僵尸进程


ngx_signal_t  signals[] = {
    // signo      signame             handler
    { SIGHUP,    "SIGHUP",           ngx_signal_handler },        //终端断开信号，对于守护进程常用于reload重载配置文件通知--标识1
    { SIGINT,    "SIGINT",           ngx_signal_handler },        //标识2   
	{ SIGTERM,   "SIGTERM",          ngx_signal_handler },        //标识15
    { SIGCHLD,   "SIGCHLD",          ngx_signal_handler },        //子进程退出时，父进程会收到这个信号--标识17
    { SIGQUIT,   "SIGQUIT",          ngx_signal_handler },        //标识3
    { SIGIO,     "SIGIO",            ngx_signal_handler },        //指示一个异步I/O事件【通用异步I/O信号】
    { SIGSYS,    "SIGSYS, SIG_IGN",  NULL               },        //我们想忽略这个信号，SIGSYS表示收到了一个无效系统调用，如果我们不忽略，进程会被操作系统杀死，--标识31
                                                                  //所以我们把handler设置为NULL，代表 我要求忽略这个信号，请求操作系统不要执行缺省的该信号处理动作（杀掉我）
    //...日后根据需要再继续增加
    { 0,         NULL,               NULL               }         //信号对应的数字至少是1，所以可以用0作为一个特殊标记
};


//返回值：0成功  ，-1失败
int ngx_init_signals()
{
    ngx_signal_t      *sig;   
    struct sigaction   sa;   

    for (sig = signals; sig->signo != 0; sig++)  
    {        
        //我们注意，现在要把一堆信息往 变量sa对应的结构里弄 ......
        memset(&sa,0,sizeof(struct sigaction));

        if (sig->handler)  
        {
            sa.sa_sigaction = sig->handler;  
            sa.sa_flags = SA_SIGINFO;  
        }
        else
        {
            sa.sa_handler = SIG_IGN; //sa_handler:这个标记SIG_IGN给到sa_handler成员，表示忽略信号的处理程序，否则操作系统的缺省信号处理程序很可能把这个进程杀掉；
                                      
        } //end if

        sigemptyset(&sa.sa_mask);   
                                    
        
        if (sigaction(sig->signo, &sa, NULL) == -1) 
        {   
            ngx_log_error_core(NGX_LOG_EMERG,errno,"sigaction(%s) failed",sig->signame); 
            return -1; 
        }	
        else
        {            
            //ngx_log_error_core(NGX_LOG_EMERG,errno,"sigaction(%s) succed!",sig->signame);     //成功不用写日志 
            //ngx_log_stderr(0,"sigaction(%s) succed!",sig->signame); //直接往屏幕上打印看看 ，不需要时可以去掉
        }
    } //end for
    return 0;    
}

//信号处理函数
static void ngx_signal_handler(int signo, siginfo_t *siginfo, void *ucontext)
{    
    //printf("来信号了\n");    
    ngx_signal_t    *sig;   
    char            *action; //一个字符串，用于记录一个动作字符串以往日志文件中写
    
    for (sig = signals; sig->signo != 0; sig++) //遍历信号数组    
    {         
        //找到对应信号，即可处理
        if (sig->signo == signo) 
        { 
            break;
        }
    } //end for

    action = (char *)"";  //目前还没有什么动作；

    if(ngx_process == NGX_PROCESS_MASTER)      //master进程，管理进程，处理的信号一般会比较多 
    {
        //master进程的往这里走
        switch (signo)
        {
        case SIGCHLD:  //一般子进程退出会收到该信号
            ngx_reap = 1;  //标记子进程状态变化，日后master主进程的for(;;)循环中可能会用到这个变量【比如重新产生一个子进程】
            break;

        //.....其他信号处理以后待增加

        default:
            break;
        } //end switch
    }
    else if(ngx_process == NGX_PROCESS_WORKER) //worker进程，具体干活的进程，处理的信号相对比较少
    {
        //worker进程的往这里走
        //......以后再增加
        //....
    }
    else
    {
        //非master非worker进程，先啥也不干
        //do nothing
    } //end if(ngx_process == NGX_PROCESS_MASTER)

    //这里记录一些日志信息
    //siginfo这个
    if(siginfo && siginfo->si_pid)  //si_pid = sending process ID【发送该信号的进程id】
    {
        ngx_log_error_core(NGX_LOG_NOTICE,0,"signal %d (%s) received from %P%s", signo, sig->signame, siginfo->si_pid, action); 
    }
    else
    {
        ngx_log_error_core(NGX_LOG_NOTICE,0,"signal %d (%s) received %s",signo, sig->signame, action);//没有发送该信号的进程id，所以不显示发送该信号的进程id
    }

    //.......其他需要扩展的将来再处理；
    if (signo == SIGCHLD) 
    {
        ngx_process_get_status(); //获取子进程的结束状态
    } //end if

    return;
}

//获取子进程的结束状态，防止单独kill子进程时子进程变成僵尸进程
static void ngx_process_get_status(void)
{
    pid_t            pid;
    int              status;
    int              err;
    int              one=0; //抄自官方nginx，应该是标记信号正常处理过一次

    //当你杀死一个子进程时，父进程会收到这个SIGCHLD信号。
    for ( ;; ) 
    {
        pid = waitpid(-1, &status, WNOHANG);    

        if(pid == 0) //子进程没结束，会立即返回这个数字，但这里应该不是这个数字【因为一般是子进程退出时会执行到这个函数】
        {
            return;
        } //end if(pid == 0)
        //-------------------------------
        if(pid == -1)//这表示这个waitpid调用有错误，有错误也理解返回出去，我们管不了这么多
        {
            err = errno;
            if(err == EINTR)           //调用被某个信号中断
            {
                continue;
            }

            if(err == ECHILD  && one)  //没有子进程
            {
                return;
            }

            if (err == ECHILD)         //没有子进程
            {
                ngx_log_error_core(NGX_LOG_INFO,err,"waitpid() failed!");
                return;
            }
            ngx_log_error_core(NGX_LOG_ALERT,err,"waitpid() failed!");
            return;
        }  //end if(pid == -1)
        //-------------------------------
        one = 1;  //标记waitpid()返回了正常的返回值
        if(WTERMSIG(status))  //获取使子进程终止的信号编号
        {
            ngx_log_error_core(NGX_LOG_ALERT,0,"pid = %P exited on signal %d!",pid,WTERMSIG(status)); //获取使子进程终止的信号编号
        }
        else
        {
            ngx_log_error_core(NGX_LOG_NOTICE,0,"pid = %P exited with code %d!",pid,WEXITSTATUS(status)); //WEXITSTATUS()获取子进程传递给exit或者_exit参数的低八位
        }
    } //end for
    return;
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>

/**
 * @brief 安全执行命令（不依赖 /bin/sh）
 * @param argv 命令参数数组，如 ["ls", "-l", NULL]
 * @param envp 自定义环境变量（NULL 表示继承父进程环境）
 * @param output 捕获子进程输出（NULL 表示不捕获）
 * @return 子进程退出状态（同 waitpid() 的返回值）
 */
int exec_command(const char *argv[], const char *envp[], char *output, size_t output_size) {
    if (!argv || !argv[0]) {
        errno = EINVAL;
        return -1;
    }

    int pipefd[2];
    if (output && pipe(pipefd) == -1) {
        return -1; // 管道创建失败
    }

    pid_t pid = fork();
    if (pid == -1) {
        return -1; // fork 失败
    }

    if (pid == 0) { // 子进程
        if (output) {
            close(pipefd[0]);          // 关闭读端
            dup2(pipefd[1], STDOUT_FILENO); // 重定向 stdout 到管道
            dup2(pipefd[1], STDERR_FILENO); // 可选：重定向 stderr
        }

        if (envp) {
            execve(argv[0], (char *const *)argv, (char *const *)envp);
        } else {
            execvp(argv[0], (char *const *)argv);
        }
        _exit(127); // exec 失败
    }

    // 父进程
    if (output) {
        close(pipefd[1]); // 关闭写端
        ssize_t n = read(pipefd[0], output, output_size - 1);
        if (n > 0) output[n] = '\0';
        close(pipefd[0]);
    }

    int status;
    waitpid(pid, &status, 0);
    return status;
}


int main(int argc, char **argv)
{
#if 0
	const char *argvs[] = {"ls", "-l", NULL};
	int status = exec_command(argvs, NULL, NULL, 0);
	if (WIFEXITED(status)) {
    	printf("Exit code: %d\n", WEXITSTATUS(status));
	}
#else
	char output[4096];
	const char *argvs[] = {"ls", "-l", NULL};
	int status = exec_command(argvs, NULL, output, sizeof(output));
	printf("Output:\n%s\n", output);
#endif

	return 0;
}
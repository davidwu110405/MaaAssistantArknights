#if __has_include(<unistd.h>)
#include "AsstPlatformPosix.h"
#include "AsstUtils.hpp"
std::string asst::utils::callcmd(const std::string& cmdline)
{
    constexpr int PipeBuffSize = 4096;
    std::string pipe_str;
    auto pipe_buffer = std::make_unique<char[]>(PipeBuffSize);

    constexpr static int PIPE_READ = 0;
    constexpr static int PIPE_WRITE = 1;
    int pipe_in[2] = { 0 };
    int pipe_out[2] = { 0 };
    int pipe_in_ret = pipe(pipe_in);
    int pipe_out_ret = pipe(pipe_out);
    if (pipe_in_ret != 0 || pipe_out_ret != 0) {
        return {};
    }
    fcntl(pipe_out[PIPE_READ], F_SETFL, O_NONBLOCK);
    int exit_ret = 0;
    int child = fork();
    if (child == 0) {
        // child process
        dup2(pipe_in[PIPE_READ], STDIN_FILENO);
        dup2(pipe_out[PIPE_WRITE], STDOUT_FILENO);
        dup2(pipe_out[PIPE_WRITE], STDERR_FILENO);

        // all these are for use by parent only
        close(pipe_in[PIPE_READ]);
        close(pipe_in[PIPE_WRITE]);
        close(pipe_out[PIPE_READ]);
        close(pipe_out[PIPE_WRITE]);

        exit_ret = execlp("sh", "sh", "-c", cmdline.c_str(), nullptr);
        exit(exit_ret);
    }
    else if (child > 0) {
        // parent process

        // close unused file descriptors, these are for child only
        close(pipe_in[PIPE_READ]);
        close(pipe_out[PIPE_WRITE]);

        do {
            ssize_t read_num = read(pipe_out[PIPE_READ], pipe_buffer.get(), PipeBuffSize);

            while (read_num > 0) {
                pipe_str.append(pipe_buffer.get(), pipe_buffer.get() + read_num);
                read_num = read(pipe_out[PIPE_READ], pipe_buffer.get(), PipeBuffSize);
            };
        } while (::waitpid(child, &exit_ret, WNOHANG) == 0);

        close(pipe_in[PIPE_WRITE]);
        close(pipe_out[PIPE_READ]);
    }
    else {
        // failed to create child process
        close(pipe_in[PIPE_READ]);
        close(pipe_in[PIPE_WRITE]);
        close(pipe_out[PIPE_READ]);
        close(pipe_out[PIPE_WRITE]);
    }
    return pipe_str;
}

#endif

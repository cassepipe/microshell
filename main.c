#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

size_t get_len(char *str)
{
	int i;

	if (str)
	{
		for (i = 0; str[i]; ++i)
			;
	}

	return i;
}

bool str_equal(char *s1, char *s2)
{
	if (s1 == s2)
		return true;
	if (!s1 || !s2)
		return false;
	if (!strcmp(s1, s2))
		return true;
	else
		return false;
}

void fatal_error()
{
	const char *msg ="error: fatal\n";

	write(STDERR_FILENO, msg, sizeof(msg));
}

void fatal_error_exit()
{
	fatal_error();
	exit(EXIT_FAILURE);
}

void exec_error_exit(char *cmd)
{
	const char msg[] = "error: cannot execute ";

	write(STDERR_FILENO, msg, sizeof(msg));
	write(STDERR_FILENO, cmd, get_len(cmd));
	write(STDERR_FILENO, "\n", 1);

	exit(127);
}

char **get_next_simple_command(char ***cmds)
{
	char **it = *cmds;
	char **simple_cmd = *cmds;

	while ( *it && !str_equal(*it, "|") && !str_equal(*it, ";") )
		it++;

	if (str_equal(*it, "|"))
		*cmds = it + 1;

	*it = NULL;

	return (simple_cmd);
}

bool is_delim(char *token)
{
	if ( token == NULL )
		return true;
	if ( str_equal(token, ";") )
		return true;
	if ( str_equal(token, "|") )
		return true;
	return false;
}

int exec_cd(char **cmds)
{
	const char bad_args[] = "error: cd: bad arguments\n";
	const char cannot[]   = "error: cd: cannot change directory to ";

	if ( is_delim(cmds[1]) || !is_delim(cmds[2]) )
	{
		write(STDERR_FILENO, bad_args, sizeof(bad_args));
		return (1);
	}
	else if (chdir(cmds[1]))
	{
		write(STDERR_FILENO, cannot, sizeof(cannot));
		write(STDERR_FILENO, cmds[1], get_len(cmds[1]));
		write(STDERR_FILENO, "\n", 1);
		return (1);
	}
	else
	{
		char buf[100];
		getcwd(buf, 100);
		dprintf(2, "Hello from %s\n", buf);
		return (0);
	}
}

#define OPIPE_RD pipe_fds[0]
#define OPIPE_WR pipe_fds[1]
#define IPIPE_RD pipe_fds[2]

int exec_pipeline(char **cmds, char **envp)
{
	int  cpid;
	int  wstatus;
	bool done = false;
	int  waits = 0;
	int	 tty_in = dup(STDIN_FILENO);
	int	 tty_out = dup(STDOUT_FILENO);
	int	 pipe_fds[3];
	char **simple_cmd;

	// Backup in order to have fds that points to the tty
	if (tty_in == -1 || tty_out == -1)
		fatal_error_exit();

	if ( ( OPIPE_RD = dup(STDIN_FILENO) ) == -1 )
		fatal_error_exit();

	while (!done)
	{
		waits++;

		IPIPE_RD = OPIPE_RD;

		if ( dup2(IPIPE_RD, STDIN_FILENO) == -1) // Now 0 points to our input pipe's read end
			fatal_error_exit();
		close(IPIPE_RD);

		// Getting a new pipe for output
		if ( pipe(pipe_fds) == -1 )
			fatal_error_exit();

		if ( dup2(OPIPE_WR, STDOUT_FILENO ) == -1) // Now 1 points to our output pipe's write end
			fatal_error_exit();
		close(OPIPE_WR);

		// Getting next simple command
		simple_cmd = get_next_simple_command(&cmds);
		if ( simple_cmd == cmds ) // End of pipeline special case
		{
			close(OPIPE_RD);
			if ( dup2(tty_out, STDOUT_FILENO) == -1 ) // End of pipeline, 1 must point back to our tty,
				fatal_error_exit();
			close(tty_out);
			done = true;
		}

		if ( ( cpid = fork() ) == -1 )
			fatal_error_exit();

		if (cpid == 0)
		{
			close(OPIPE_RD); // We don't need the output pipe's read end
			close(tty_in);
			close(tty_out);
			if ( !str_equal(*simple_cmd, "cd") )
			{
				execve(*simple_cmd, simple_cmd, envp);
				exec_error_exit(*simple_cmd);
			}
			else
				exit(exec_cd(simple_cmd));
		}
	}

	if ( dup2(tty_in, STDIN_FILENO) == -1 ) //Cleaning up : Have 0 point back to our tty
		fatal_error();
	close(tty_in);

	while (--waits)
		waitpid(-1, NULL, 0);
	waitpid(-1, &wstatus, 0);
	return (WEXITSTATUS(wstatus));
}

int main( int ac, char **av, char **envp )
{
	--ac;
	++av;

	for ( int i = 0 ; /*loop forever*/ ; av += i + 1, i = 0)
	{
		bool is_pipeline = false;

		for ( ; av[i] != NULL && !str_equal(av[i], ";") ; ++i)
		{
			if ( str_equal(av[i], "|"))
				is_pipeline = true;
		}

		if ( (av[i] == NULL) || ( str_equal(av[i], ";") && av[i + 1] == NULL ) )
		{
			if ( str_equal(*av, "cd") && !is_pipeline )
				return exec_cd(av);
			else
				return exec_pipeline(av, envp);
		}
		if ( str_equal(*av, "cd") && !is_pipeline )
			exec_cd(av);
		else
			exec_pipeline(av, envp);
	}
}

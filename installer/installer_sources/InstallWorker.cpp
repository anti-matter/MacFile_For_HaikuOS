#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <Entry.h>
#include <Message.h>

#include "InstallWorker.h"

struct worker_args
{
	BString		releaseDir;
	BString		subcommand;
	BMessenger	target;
};

//
//Post a single protocol line from the backend script as a BMessage.
//
static void post_line(BMessenger& target, const char* line)
{
	const char* str = line;

	if (strncmp(str, "PROGRESS ", 9) == 0)
	{
		str += 9;
		int32 percent = atoi(str);
		BString label;
		const char* space = strchr(str, ' ');
		if (space != NULL)
			label.SetTo(space + 1);

		BMessage message(INSTALL_M_PROGRESS);
		message.AddInt32("percent", percent);
		message.AddString("label", label.String());
		target.SendMessage(&message);
		return;
	}

	if (strncmp(str, "STATUS ", 7) == 0)
	{
		BMessage message(INSTALL_M_STATUS);
		message.AddString("state", str + 7);
		target.SendMessage(&message);
		return;
	}

	if (strncmp(str, "DONE ", 5) == 0)
	{
		BMessage message(INSTALL_M_DONE);
		message.AddString("result", str + 5);
		target.SendMessage(&message);
		return;
	}

	if (strncmp(str, "INFO ", 5) == 0)
	{
		BMessage message(INSTALL_M_LOG);
		message.AddString("line", str + 5);
		target.SendMessage(&message);
		return;
	}

	if (strncmp(str, "ERROR ", 6) == 0)
	{
		BMessage message(INSTALL_M_LOG);
		message.AddString("line", str + 6);
		target.SendMessage(&message);
		return;
	}

	//
	//Fallback: log the line verbatim.
	//
	BMessage message(INSTALL_M_LOG);
	message.AddString("line", line);
	target.SendMessage(&message);
}

//
//Post a log line followed by a DONE failure message.
//
static void post_failure(BMessenger& target, const char* line)
{
	BMessage message(INSTALL_M_LOG);
	message.AddString("line", line);
	target.SendMessage(&message);

	BMessage done(INSTALL_M_DONE);
	done.AddString("result", "failure");
	target.SendMessage(&done);
}

//
//worker_loop()
//
//Description:
//		Run the backend install script non-interactively, read its
//		protocol output line by line, and post it to the window.
//
//Returns:
//
static int32 worker_loop(void* arg)
{
	worker_args* wargs = (worker_args*)arg;
	BMessenger target = wargs->target;
	BString releaseDir = wargs->releaseDir;
	BString subcommand = wargs->subcommand;
	delete wargs;

	BString script = releaseDir;
	script += "/install-macfile.sh";

	//
	//The backend script must sit next to the installer binary.
	//
	BEntry entry(script.String());
	if (!entry.Exists() || !entry.IsFile())
	{
		post_failure(target,
			"ERROR: install-macfile.sh not found in the release directory.");
		return B_OK;
	}

	int pipefd[2];
	if (pipe(pipefd) < 0)
	{
		post_failure(target, "ERROR: could not create pipe for backend script.");
		return B_OK;
	}

	pid_t pid = fork();
	if (pid < 0)
	{
		close(pipefd[0]);
		close(pipefd[1]);

		post_failure(target, "ERROR: could not fork backend script.");
		return B_OK;
	}

	if (pid == 0)
	{
		//
		//Child: run the script with stdout/stderr on the pipe.
		//
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		dup2(pipefd[1], STDERR_FILENO);
		close(pipefd[1]);

		execl("/bin/sh", "sh", script.String(), subcommand.String(), (char*)NULL);
		_exit(127);
	}

	//
	//Parent: read protocol lines until the script closes the pipe.
	//
	close(pipefd[1]);

	FILE* stream = fdopen(pipefd[0], "r");
	if (stream != NULL)
	{
		char* line = NULL;
		size_t size = 0;
		ssize_t len;

		while ((len = getline(&line, &size, stream)) != -1)
		{
			line[len] = '\0';
			//
			//Strip the trailing newline.
			//
			while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
				line[--len] = '\0';

			post_line(target, line);
		}

		free(line);
		fclose(stream);
	}
	else
	{
		close(pipefd[0]);
	}

	int status = 0;
	waitpid(pid, &status, 0);
	bool success = WIFEXITED(status) && (WEXITSTATUS(status) == 0);

	BMessage message(INSTALL_M_DONE);
	message.AddString("result", success ? "success" : "failure");
	target.SendMessage(&message);

	return B_OK;
}

//
//Spawn()
//
//Description:
//		Start the worker thread.
//
//Returns:
//
void InstallWorker::Spawn(
	const BString&	releaseDir,
	const char*		subcommand,
	BMessenger*		target
	)
{
	worker_args* wargs = new (std::nothrow) worker_args;
	if (wargs == NULL)
		return;

	wargs->releaseDir = releaseDir;
	wargs->subcommand = subcommand;
	wargs->target = *target;

	int32 thread = spawn_thread(worker_loop, "install_worker", B_NORMAL_PRIORITY, wargs);
	if (thread < B_OK)
	{
		delete wargs;
		return;
	}

	resume_thread(thread);
}

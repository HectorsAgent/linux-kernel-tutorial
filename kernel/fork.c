#include <linux/sched.h>
#include <linux/fdtable.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/nsproxy.h>
#include <tutorial/lab.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Labbet har en enda exekveringstråd och ingen riktig scheduler/fork. */
struct task_struct *current;

int task_init(struct task_struct *task, int pid, const char *name, struct net *net)
{
	memset(task, 0, sizeof(*task));
	task->pid = pid;
	snprintf(task->comm, sizeof(task->comm), "%s", name);
	task->files = calloc(1, sizeof(*task->files));
	task->nsproxy = calloc(1, sizeof(*task->nsproxy));
	if (!task->files || !task->nsproxy) {
		task_cleanup(task);
		return -ENOMEM;
	}
	task->files->fdt.max_fds = MAX_FDS;
	task->nsproxy->net_ns = net;
	return 0;
}

void task_cleanup(struct task_struct *task)
{
	if (current == task)
		current = NULL;
	if (task->files)
		for (unsigned int i = 0; i < task->files->fdt.max_fds; i++)
			fput(task->files->fdt.fd[i]);
	free(task->files);
	free(task->nsproxy);
	task->files = NULL;
	task->nsproxy = NULL;
}

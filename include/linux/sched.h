#ifndef TUTORIAL_LINUX_SCHED_H
#define TUTORIAL_LINUX_SCHED_H

struct files_struct;
struct nsproxy;

struct task_struct {
	int pid;
	char comm[16];
	struct files_struct *files;
	struct nsproxy *nsproxy;
};

/* Riktig Linux hämtar current från CPU-/arkitekturkontext, inte en global. */
extern struct task_struct *current;

#endif

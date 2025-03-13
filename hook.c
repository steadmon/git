#define USE_THE_REPOSITORY_VARIABLE

#include "git-compat-util.h"
#include "abspath.h"
#include "advice.h"
#include "gettext.h"
#include "hook.h"
#include "path.h"
#include "run-command.h"
#include "config.h"
#include "strbuf.h"
#include "environment.h"
#include "setup.h"

static void free_hook(struct hook *ptr)
{
	free(ptr);
}

static void remove_hook(struct list_head *to_remove)
{
	struct hook *hook_to_remove = list_entry(to_remove, struct hook, list);
	list_del(to_remove);
	free_hook(hook_to_remove);
}

void clear_hook_list(struct list_head *head)
{
	struct list_head *pos, *tmp;
	list_for_each_safe(pos, tmp, head)
		remove_hook(pos);
	free(head);
}

const char *find_hook(struct repository *r, const char *name)
{
	static struct strbuf path = STRBUF_INIT;

	int found_hook;

	repo_git_path_replace(r, &path, "hooks/%s", name);
	found_hook = access(path.buf, X_OK) >= 0;
#ifdef STRIP_EXTENSION
	if (!found_hook) {
		int err = errno;

		strbuf_addstr(&path, STRIP_EXTENSION);
		found_hook = access(path.buf, X_OK) >= 0;
		if (!found_hook)
			errno = err;
	}
#endif

	if (!found_hook) {
		if (errno == EACCES && advice_enabled(ADVICE_IGNORED_HOOK)) {
			static struct string_list advise_given = STRING_LIST_INIT_DUP;

			if (!string_list_lookup(&advise_given, name)) {
				string_list_insert(&advise_given, name);
				advise(_("The '%s' hook was ignored because "
					 "it's not set as executable.\n"
					 "You can disable this warning with "
					 "`git config set advice.ignoredHook false`."),
				       path.buf);
			}
		}
		return NULL;
	}
	return path.buf;
}

int hook_exists(struct repository *r, const char *name)
{
	int exists = 0;
	struct list_head *hooks = list_hooks(r, name);

	exists = !list_empty(hooks);

	clear_hook_list(hooks);
	return exists;
}

struct list_head *list_hooks(struct repository *r, const char *hookname)
{
	struct list_head *hook_head = xmalloc(sizeof(struct list_head));

	INIT_LIST_HEAD(hook_head);

	if (!hookname)
		BUG("null hookname was provided to hook_list()!");

	if (have_git_dir()) {
		const char *hook_path = find_hook(r, hookname);

		/* Add the hook from the hookdir */
		if (hook_path) {
			struct hook *to_add = xmalloc(sizeof(*to_add));
			to_add->hook_path = hook_path;
			to_add->feed_pipe_cb_data = NULL;
			list_add_tail(&to_add->list, hook_head);
		}
	}

	return hook_head;
}

int pipe_from_string_list(struct strbuf *pipe, void *pp_cb, void *pp_task_cb)
{
	struct hook *hook = pp_task_cb;
	struct hook_cb_data *hook_cb = pp_cb;
	struct string_list *to_pipe = hook_cb->options->feed_pipe_ctx;
	unsigned int *item_idx;

	/* Bootstrap the state manager if necessary. */
	if (!hook->feed_pipe_cb_data) {
		hook->feed_pipe_cb_data = xmalloc(sizeof(unsigned int));
		*(unsigned int*)hook->feed_pipe_cb_data = 0;
	}
	item_idx = hook->feed_pipe_cb_data;

	if (*item_idx < to_pipe->nr) {
		strbuf_addf(pipe, "%s\n", to_pipe->items[*item_idx].string);
		(*item_idx)++;
		return 0;
	} else {
		free(item_idx);
	}

	return 1;
}

static int pick_next_hook(struct child_process *cp,
			  struct strbuf *out UNUSED,
			  void *pp_cb,
			  void **pp_task_cb)
{
	struct hook_cb_data *hook_cb = pp_cb;
	struct hook *to_run = hook_cb->run_me;

	if (!to_run)
		return 0;

	cp->no_stdin = 1;
	strvec_pushv(&cp->env, hook_cb->options->env.v);
	/* reopen the file for stdin; run_command closes it. */
	if (hook_cb->options->path_to_stdin) {
		cp->no_stdin = 0;
		cp->in = xopen(hook_cb->options->path_to_stdin, O_RDONLY);
	} else if (hook_cb->options->feed_pipe) {
		/* ask for start_command() to make a pipe for us */
		cp->in = -1;
		cp->no_stdin = 0;
	} else {
		cp->no_stdin = 1;
	}
	cp->stdout_to_stderr = 1;
	cp->trace2_hook_name = hook_cb->hook_name;
	cp->dir = hook_cb->options->dir;

	if (hook_cb->options->dir) {
		struct strbuf abs_path = STRBUF_INIT;
		strbuf_add_absolute_path(&abs_path, to_run->hook_path);
		strvec_push(&cp->args, abs_path.buf);
		strbuf_release(&abs_path);
	} else {
		strvec_push(&cp->args, to_run->hook_path);
	}

	/*
	 * add passed-in argv, without expanding - let the user get back
	 * exactly what they put in
	 */
	strvec_pushv(&cp->args, hook_cb->options->args.v);

	/* Provide context for errors if necessary */
	*pp_task_cb = to_run;

	/* Get the next entry ready */
	if (hook_cb->run_me->list.next == hook_cb->head)
		hook_cb->run_me = NULL;
	else
		hook_cb->run_me = list_entry(hook_cb->run_me->list.next,
					     struct hook, list);

	return 1;
}

static int notify_start_failure(struct strbuf *out UNUSED,
				void *pp_cb,
				void *pp_task_cb UNUSED)
{
	struct hook_cb_data *hook_cb = pp_cb;

	hook_cb->rc |= 1;

	return 1;
}

static int notify_hook_finished(int result,
				struct strbuf *out UNUSED,
				void *pp_cb,
				void *pp_task_cb UNUSED)
{
	struct hook_cb_data *hook_cb = pp_cb;
	struct run_hooks_opt *opt = hook_cb->options;

	hook_cb->rc |= result;

	if (opt->invoked_hook)
		*opt->invoked_hook = 1;

	return 0;
}

static void run_hooks_opt_clear(struct run_hooks_opt *options)
{
	strvec_clear(&options->env);
	strvec_clear(&options->args);
}

/*
 * Determines how many jobs to use after we know we want to parallelize. First
 * priority is the config 'hook.jobs' and second priority is the number of CPUs.
 */
static int configured_hook_jobs(struct repository *r)
{
	/*
	 * The config and the CPU count probably won't change during the process
	 * lifetime, so cache the result in case we invoke multiple hooks during
	 * one process.
	 */
	static int jobs = 0;
	if (jobs)
		return jobs;

	if (repo_config_get_int(r, "hook.jobs", &jobs))
		/* if the config isn't set, fall back to CPU count. */
		jobs = online_cpus();

	return jobs;
}

int run_hooks_opt(struct repository *r, const char *hook_name,
		   struct run_hooks_opt *options)
{
	struct strbuf abs_path = STRBUF_INIT;
	int ret = 0;
	struct hook_cb_data cb_data = {
		.rc = 0,
		.hook_name = hook_name,
		.options = options,
	};
	struct run_process_parallel_opts opts = {
		.tr2_category = "hook",
		.tr2_label = hook_name,

		.processes = 1,
		.ungroup = 1,

		.get_next_task = pick_next_hook,
		.start_failure = notify_start_failure,
		.feed_pipe = options->feed_pipe,
		.consume_sideband = options->consume_sideband,
		.task_finished = notify_hook_finished,

		.data = &cb_data,
	};

	cb_data.head = list_hooks(r, hook_name);
	cb_data.run_me = list_first_entry(cb_data.head, struct hook, list);

	if (!options)
		BUG("a struct run_hooks_opt must be provided to run_hooks");

	if (options->invoked_hook)
		*options->invoked_hook = 0;

	if (options->path_to_stdin && options->feed_pipe)
		BUG("choose only one method to populate stdin");

	if (list_empty(cb_data.head) && !options->error_if_missing)
		goto cleanup;

	if (list_empty(cb_data.head)) {
		ret = error("cannot find a hook named %s", hook_name);
		goto cleanup;
	}

	/* INIT_PARALLEL sets jobs to 0, so go look up how many to use. */
	if (!options->jobs)
		options->jobs = configured_hook_jobs(r);

	/*
	 * If it's single-threaded, or if there's only one hook to run, then we
	 * can ungroup the output.
	 */
	opts.ungroup = options->jobs == 1 ||
		       cb_data.run_me->list.next == cb_data.head;

	run_processes_parallel(&opts);
	ret = cb_data.rc;
cleanup:
	clear_hook_list(cb_data.head);
	strbuf_release(&abs_path);
	run_hooks_opt_clear(options);
	return ret;
}

int run_hooks(struct repository *r, const char *hook_name)
{
	struct run_hooks_opt opt = RUN_HOOKS_OPT_INIT_PARALLEL;

	return run_hooks_opt(r, hook_name, &opt);
}

int run_hooks_l(struct repository *r, const char *hook_name, ...)
{
	struct run_hooks_opt opt = RUN_HOOKS_OPT_INIT_PARALLEL;
	va_list ap;
	const char *arg;

	va_start(ap, hook_name);
	while ((arg = va_arg(ap, const char *)))
		strvec_push(&opt.args, arg);
	va_end(ap);

	return run_hooks_opt(r, hook_name, &opt);
}

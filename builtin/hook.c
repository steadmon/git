#define USE_THE_REPOSITORY_VARIABLE
#include "builtin.h"
#include "config.h"
#include "gettext.h"
#include "hook.h"
#include "parse-options.h"
#include "strvec.h"

#define BUILTIN_HOOK_RUN_USAGE \
	N_("git hook run [--ignore-missing] [--to-stdin=<path>] [(-j|--jobs) <n>]\n"\
	   "<hook-name> [-- <hook-args>]")
#define BUILTIN_HOOK_LIST_USAGE \
	N_("git hook list <hook-name>")

static const char * const builtin_hook_usage[] = {
	BUILTIN_HOOK_RUN_USAGE,
	BUILTIN_HOOK_LIST_USAGE,
	NULL
};

static const char * const builtin_hook_run_usage[] = {
	BUILTIN_HOOK_RUN_USAGE,
	NULL
};

static const char *const builtin_hook_list_usage[] = {
	BUILTIN_HOOK_LIST_USAGE,
	NULL
};

static int list(int argc, const char **argv, const char *prefix,
		 struct repository *repo UNUSED)
{
	struct list_head *head, *pos;
	const char *hookname = NULL;

	struct option list_options[] = {
		OPT_END(),
	};

	argc = parse_options(argc, argv, prefix, list_options,
			     builtin_hook_list_usage, 0);

	/*
	 * The only unnamed argument provided should be the hook-name; if we add
	 * arguments later they probably should be caught by parse_options.
	 */
	if (argc != 1)
		usage_msg_opt(_("You must specify a hook event name to list."),
			      builtin_hook_list_usage, list_options);

	hookname = argv[0];

	head = list_hooks(the_repository, hookname);

	if (list_empty(head))
		return 1;

	list_for_each(pos, head) {
		struct hook *item = list_entry(pos, struct hook, list);
		item = list_entry(pos, struct hook, list);
		if (item)
			printf("%s\n", item->hook_path);
	}

	clear_hook_list(head);

	return 0;
}
static int run(int argc, const char **argv, const char *prefix,
	       struct repository *repo UNUSED)
{
	int i;
	struct run_hooks_opt opt = RUN_HOOKS_OPT_INIT_SERIAL;
	int ignore_missing = 0;
	const char *hook_name;
	struct option run_options[] = {
		OPT_BOOL(0, "ignore-missing", &ignore_missing,
			 N_("silently ignore missing requested <hook-name>")),
		OPT_STRING(0, "to-stdin", &opt.path_to_stdin, N_("path"),
			   N_("file to read into hooks' stdin")),
		OPT_INTEGER('j', "jobs", &opt.jobs,
			    N_("run up to <n> hooks simultaneously")),
		OPT_END(),
	};
	int ret;

	argc = parse_options(argc, argv, prefix, run_options,
			     builtin_hook_run_usage,
			     PARSE_OPT_KEEP_DASHDASH);

	if (!argc)
		goto usage;

	/*
	 * Having a -- for "run" when providing <hook-args> is
	 * mandatory.
	 */
	if (argc > 1 && strcmp(argv[1], "--") &&
	    strcmp(argv[1], "--end-of-options"))
		goto usage;

	/* Add our arguments, start after -- */
	for (i = 2 ; i < argc; i++)
		strvec_push(&opt.args, argv[i]);

	/* Need to take into account core.hooksPath */
	git_config(git_default_config, NULL);

	hook_name = argv[0];
	if (!ignore_missing)
		opt.error_if_missing = 1;
	ret = run_hooks_opt(the_repository, hook_name, &opt);
	if (ret < 0) /* error() return */
		ret = 1;
	return ret;
usage:
	usage_with_options(builtin_hook_run_usage, run_options);
}

int cmd_hook(int argc,
	     const char **argv,
	     const char *prefix,
	     struct repository *repo)
{
	parse_opt_subcommand_fn *fn = NULL;
	struct option builtin_hook_options[] = {
		OPT_SUBCOMMAND("run", &fn, run),
		OPT_SUBCOMMAND("list", &fn, list),
		OPT_END(),
	};

	argc = parse_options(argc, argv, NULL, builtin_hook_options,
			     builtin_hook_usage, 0);

	return fn(argc, argv, prefix, repo);
}

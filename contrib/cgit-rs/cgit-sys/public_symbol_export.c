// Shim to publicly export Git symbols. These must be renamed so that the
// original symbols can be hidden. Renaming these with a "libgit_" prefix also
// avoid conflicts with other libraries such as libgit2.

#include "git-compat-util.h"
#include "contrib/cgit-rs/cgit-sys/public_symbol_export.h"
#include "common-init.h"
#include "config.h"
#include "setup.h"
#include "version.h"

#pragma GCC visibility push(default)

const char *libgit_setup_git_directory(void)
{
	return setup_git_directory();
}

int libgit_config_get_int(const char *key, int *dest)
{
	return git_config_get_int(key, dest);
}

void libgit_init_git(const char **argv)
{
	init_git(argv);
}

int libgit_parse_maybe_bool(const char *val)
{
	return git_parse_maybe_bool(val);
}

const char *libgit_user_agent(void)
{
	return git_user_agent();
}

const char *libgit_user_agent_sanitized(void)
{
	return git_user_agent_sanitized();
}

#pragma GCC visibility pop

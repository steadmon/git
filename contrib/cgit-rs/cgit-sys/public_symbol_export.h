#ifndef PUBLIC_SYMBOL_EXPORT_H
#define PUBLIC_SYMBOL_EXPORT_H

const char *libgit_setup_git_directory(void);

int libgit_config_get_int(const char *key, int *dest);

void libgit_init_git(const char **argv);

int libgit_parse_maybe_bool(const char *val);

const char *libgit_user_agent(void);

const char *libgit_user_agent_sanitized(void);

#endif /* PUBLIC_SYMBOL_EXPORT_H */

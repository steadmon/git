use std::ffi::{c_char, c_int};

extern "C" {
    pub fn libgit_setup_git_directory() -> *const c_char;

    // From config.c
    pub fn libgit_config_get_int(key: *const c_char, dest: *mut c_int) -> c_int;

    // From common-init.c
    pub fn libgit_init_git(argv: *const *const c_char);

    // From parse.c
    pub fn libgit_parse_maybe_bool(val: *const c_char) -> c_int;

    // From version.c
    pub fn libgit_user_agent() -> *const c_char;
    pub fn libgit_user_agent_sanitized() -> *const c_char;
}

#[cfg(test)]
mod tests {
    use std::ffi::{CStr, CString};

    use super::*;

    #[test]
    fn user_agent_starts_with_git() {
        let c_str = unsafe { CStr::from_ptr(libgit_user_agent()) };
        let agent = c_str
            .to_str()
            .expect("User agent contains invalid UTF-8 data");
        assert!(
            agent.starts_with("git/"),
            r#"Expected user agent to start with "git/", got: {}"#,
            agent
        );
    }

    #[test]
    fn sanitized_user_agent_starts_with_git() {
        let c_str = unsafe { CStr::from_ptr(libgit_user_agent_sanitized()) };
        let agent = c_str
            .to_str()
            .expect("Sanitized user agent contains invalid UTF-8 data");
        assert!(
            agent.starts_with("git/"),
            r#"Expected user agent to start with "git/", got: {}"#,
            agent
        );
    }

    #[test]
    fn parse_bools_from_strings() {
        let arg = CString::new("true").unwrap();
        assert_eq!(unsafe { libgit_parse_maybe_bool(arg.as_ptr()) }, 1);

        let arg = CString::new("yes").unwrap();
        assert_eq!(unsafe { libgit_parse_maybe_bool(arg.as_ptr()) }, 1);

        let arg = CString::new("false").unwrap();
        assert_eq!(unsafe { libgit_parse_maybe_bool(arg.as_ptr()) }, 0);

        let arg = CString::new("no").unwrap();
        assert_eq!(unsafe { libgit_parse_maybe_bool(arg.as_ptr()) }, 0);

        let arg = CString::new("maybe").unwrap();
        assert_eq!(unsafe { libgit_parse_maybe_bool(arg.as_ptr()) }, -1);
    }

    #[test]
    fn access_configs() {
        // NEEDSWORK: we need to supply a testdata config
        let fake_argv = [std::ptr::null::<c_char>()];
        unsafe {
            libgit_init_git(fake_argv.as_ptr());
            libgit_setup_git_directory();
        }
        let mut val: c_int = 0;
        let key = CString::new("trace2.eventNesting").unwrap();
        unsafe { libgit_config_get_int(key.as_ptr(), &mut val as *mut i32) };
        assert_eq!(val, 5);
    }
}

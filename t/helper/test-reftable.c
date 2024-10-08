#include "reftable/system.h"
#include "reftable/reftable-tests.h"
#include "test-tool.h"

int cmd__reftable(int argc, const char **argv)
{
	/* test from simple to complex. */
	block_test_main(argc, argv);
	stack_test_main(argc, argv);
	return 0;
}

int cmd__dump_reftable(int argc, const char **argv)
{
	return reftable_dump_main(argc, (char *const *)argv);
}

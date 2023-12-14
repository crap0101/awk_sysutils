
# NOTE:
# uses external shell commands to perform some tests:
# pwd, which, whoami

@load "sysutils"

@include "awkpot"
# https://github.com/crap0101/awkpot
@include "testing"
# https://github.com/crap0101/awk_testing

BEGIN {
    if (awk::SYS_DEBUG) {
	dprint = "awkpot::dprint_real"
	# to set dprint in awkpot functions also (defaults to dprint_fake)
	awkpot::set_dprint(dprint)
    } else {
	dprint = "awkpot::dprint_fake"
    }

    testing::start_test_report()
    
    # TEST check_path
    t1 = sys::mktemp()
    testing::assert_true(sys::check_path(t1), 1, "check_path tempfile")
    testing::assert_equal(sys::check_path(t1), sys::check_path(t1, "r"), 1, "check_path [no args] eq [r]")
    testing::assert_equal(sys::check_path(t1), sys::check_path(t1, ""), 1, "check_path [empty] eq [r]")
    testing::assert_true(sys::check_path(t1, "w"), 1, "check_path tempfile [w]")
    testing::assert_true(sys::check_path(t1, "rw"), 1, "check_path tempfile [rw]")
    testing::assert_false(sys::check_path(t1, "x"), 1, "! check_path tempfile [x]")
    testing::assert_false(sys::check_path(t1, "rx"), 1, "! check_path tempfile [rx]")

    cmd = sprintf("%s -l sysutils 'BEGIN { sys::check_path("", \"b\")}'", ARGV[0])
    testing::assert_false(awkpot::exec_command(cmd), 1, "! check_path: wrong mask [b]")

    @dprint("* rm t1")
    sys::rm(t1)
    testing::assert_false(sys::check_path(t1), 1, "! check_path tempfile [deleted]")
    testing::assert_false(sys::check_path(1+0), 1, "! check_path [fake path]")
    cmd = sprintf("%s -l sysutils 'BEGIN { sys::check_path(1,2,3)}'", ARGV[0])
    testing::assert_false(awkpot::exec_command(cmd), 1, "! check_path: [too many args]")
    cmd = sprintf("%s -l sysutils 'BEGIN { sys::check_path()}'", ARGV[0])
    testing::assert_false(awkpot::exec_command(cmd), 1, "! check_path [no args]")

    # TEST get_pathsep fatal
    cmd = sprintf("%s -l sysutils 'BEGIN { sys::get_pathsep(1)}'", ARGV[0])
    r = awkpot::exec_command(cmd, 0)
    testing::assert_false(r, 1, "! get_pathsep(1) [too many args]")
    
    # TEST mktemp and getcwd too
    t1 = sys::mktemp()
    cwd = sys::getcwd()
    # using external command to compare getcwd value
    ("pwd" | getline _base)
    close("pwd")
    sys::rm(t1)
    testing::assert_true(sys::check_path(_base), 1, "checkpath basedir")
    testing::assert_equal(_base, cwd, 1, "_base == cwd")

    # TEST mktemp, rm
    cmd = sprintf("%s -l sysutils 'BEGIN { sys::mktemp(1, 2)}'", ARGV[0])
    testing::assert_false(awkpot::exec_command(cmd), 1, "! mktemp [too many args]")
    # unix only
    if (sys::get_pathsep() == "/") {
	("whoami" | getline iam)
	close("whoami")
	if (iam != "root")
	    testing::assert_equal(sys::mktemp("/bin"), "", 1, "! mktemp [no perm]")
    }
    testing::assert_equal(sys::mktemp("thisshouldbeanotexistentfilename"), "",
			  1, "! mktemp [not existent dir]")
    
    t1 = sys::mktemp()
    testing::assert_true(sys::check_path(t1), 1, "checkpath tempfile")
    sys::rm(t1)
    testing::assert_false(sys::check_path(t1), 1, "! checkpath tempfile")

    t1 = sys::mktemp()
    # ...get the tempfile name without leading directories :D
    _t1x = _t1_arr2[split(_t1_arr[split(t1, _t1_arr, cwd)], _t1_arr2, "/")]
    testing::assert_true(sys::check_path(_t1x), 1, "checkpath _t1x")
    sys::rm(_t1x)
    testing::assert_false(sys::check_path(t1), 1, "! checkpath t1")

    tmp_dir = "/tmp"
    t1 = sys::mktemp(tmp_dir)
    testing::assert_true(sys::check_path(t1), 1, "checkpath tempfile")
    _t1x = _t1_arr2[split(_t1_arr[split(t1, _t1_arr, cwd)], _t1_arr2, "/")]
    testing::assert_true(sys::check_path(tmp_dir "/" _t1x), 1, "checkpath _t1x")
    sys::rm(t1)
    testing::assert_false(sys::check_path(t1), 1, "! checkpath tempfile")

    # others rm tests
    t1 = sys::mktemp(tmp_dir)
    testing::assert_true(sys::rm(t1), 1, "check rm t1")
    testing::assert_false(sys::rm(t1), 1, "! check rm t1")
    ("which sed" | getline binf)
    close("which sed")
    ("whoami" | getline iam)
    close("whoami")
    if (binf && iam != "root")
	# ^L^
	if (! sys::check_path(binf))
	    testing::assert_false(sys::rm(binf), 1, "! rm [no perm]")
    # rm fatal
    cmd = sprintf("%s -l sysutils 'BEGIN { sys::rm()}'", ARGV[0])
    testing::assert_false(awkpot::exec_command(cmd), 1, "! rm [no args]")    
    cmd = sprintf("%s -l sysutils 'BEGIN { sys::rm(1, 2)}'", ARGV[0])
    testing::assert_false(awkpot::exec_command(cmd), 1, "! rm [too many args]")
    cmd = sprintf("%s -l sysutils 'BEGIN { x[0];sys::rm(x)}'", ARGV[0])
    testing::assert_false(awkpot::exec_command(cmd), 1, "! rm [wrong type arg]")
    # rm not existent file:
    testing::assert_false(sys::rm("thisshouldbeanotexistentfilename"), 1, "! rm [not existent file]")

    testing::end_test_report()
    testing::report()

}

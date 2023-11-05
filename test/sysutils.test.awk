
# NOTE:
# uses external shell commands to perform some tests:
# pwd, which

@load "sysutils"

@include "awkpot"
# https://github.com/crap0101/awkpot
@include "awk_testing.awk"
# https://github.com/crap0101/laundry_basket/blob/master/awk_testing.awk

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
    testing::assert_true(sys::check_path(t1), 1, "> check_path tempfile")
    testing::assert_equal(sys::check_path(t1), sys::check_path(t1, "r"), 1, "> check_path [no args] eq [r]")
    testing::assert_equal(sys::check_path(t1), sys::check_path(t1, ""), 1, "> check_path [empty] eq [r]")
    testing::assert_true(sys::check_path(t1, "w"), 1, "> check_path tempfile [w]")
    testing::assert_true(sys::check_path(t1, "rw"), 1, "> check_path tempfile [rw]")
    testing::assert_false(sys::check_path(t1, "x"), 1, "> ! check_path tempfile [x]")
    testing::assert_false(sys::check_path(t1, "rx"), 1, "> ! check_path tempfile [rx]")
    testing::assert_false(sys::check_path(t1, "b"), 1, "> ! check_path tempfile [b]")
    @dprint("* rm t1")
    sys::rm(t1)
    testing::assert_false(sys::check_path(t1), 1, "> ! check_path tempfile")
    testing::assert_false(sys::check_path(1+0), 1, "> ! check_path [fake path]")
    testing::assert_false(sys::check_path(1, 2, 3), 1, "> ! check_path [wrong args number]")
    #testing::assert_false(sys::check_path(), 1, "> ! checkpath [no args]") # brutal fail: exits

    # TEST mktemp and getcwd too
    t1 = sys::mktemp()
    cwd = sys::getcwd()
    # using external command to compare getcwd value
    ("pwd" | getline _base)
    close("pwd")
    sys::rm(t1)
    testing::assert_true(sys::check_path(_base), 1, "> checkpath basedir")
    testing::assert_equal(_base, cwd, 1, "> _base == cwd")

    # TEST mktemp, rm
    testing::assert_false(sys::mktemp(1, 2), 1, "> ! mktemp [wrong args number]")

    t1 = sys::mktemp()
    testing::assert_true(sys::check_path(t1), 1, "> checkpath tempfile")
    sys::rm(t1)
    testing::assert_false(sys::check_path(t1), 1, "> ! checkpath tempfile")

    t1 = sys::mktemp()
    # get the tempfile name without leading directories :D
    _t1x = _t1_arr2[split(_t1_arr[split(t1, _t1_arr, cwd)], _t1_arr2, "/")]
    testing::assert_true(sys::check_path(_t1x), 1, "> checkpath _t1x")
    sys::rm(_t1x)
    testing::assert_false(sys::check_path(t1), 1, "> ! checkpath t1")

    tmp_dir = "/tmp"
    t1 = sys::mktemp(tmp_dir)
    testing::assert_true(sys::check_path(t1), 1, "> checkpath tempfile")
    _t1x = _t1_arr2[split(_t1_arr[split(t1, _t1_arr, cwd)], _t1_arr2, "/")]
    testing::assert_true(sys::check_path(tmp_dir "/" _t1x), 1, "> checkpath _t1x")
    sys::rm(t1)
    testing::assert_false(sys::check_path(t1), 1, "> ! checkpath tempfile")

    # others rm tests
    t1 = sys::mktemp(tmp_dir)
    testing::assert_true(sys::rm(t1), 1, "> check rm t1")
    testing::assert_false(sys::rm(t1), 1, "> ! check rm t1")
    ("which sed" | getline binf)
    close("which sed")
    if (binf)
	# ^L^
	if (! sys::check_path(binf))
	    testing::assert_false(sys::rm(binf), 1, "> check rm [perm]")
    
    #testing::assert_false(sys::rm(), 1, "> ! check rm [no args]") # brutal fail: exits
    testing::assert_false(sys::rm(1,2), 1, "> ! check rm [too many args]")

    testing::end_test_report()
    testing::report()

}

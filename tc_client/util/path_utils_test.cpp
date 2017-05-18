/**
 * Copyright (C) Stony Brook University 2016
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <string.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <utility>
#include <string>
#include <vector>
#include <initializer_list>

#include "path_utils.h"

using std::string;
using std::vector;

TEST(PathUtilsTest, NormalizeTest) {
	char path[1024];
	auto Expect = [&path](const char *input, const char *expected) {
		for (int inplace = 0; inplace < 2; ++ inplace) {
			int ret;
			if (inplace) {
				strcpy(path, input);
				ret = tc_path_normalize(path, path, 1024);
			} else {
				ret = tc_path_normalize(input, path, 1024);
			}
			ASSERT_GT(ret, 0) << "failed to nomalize " << input;
			EXPECT_STREQ(expected, path)
			    << input << " is normalized to " << path
			    << " instead of " << expected;
		}
	};

	Expect("a", "a");
	Expect(".foo-foo-foo", ".foo-foo-foo");
	Expect("/", "/");
	Expect("//", "/");
	Expect("/foo/bar/", "/foo/bar");
	Expect("/foo/../bar/", "/bar");
	Expect("/foo/../../../", "/");
	Expect("foo/..", ".");
	Expect("./foo", "foo");
	Expect("/a/b/d/./../c/d/e", "/a/b/c/d/e");
	Expect("/foo/bar/../../baz", "/baz");
	Expect("//a//bb//ccc//dddd", "/a/bb/ccc/dddd");
	Expect("foo/../../", "..");
	Expect("./a/../b/../c/../../", "..");
}

TEST(PathUtilsTest, TokenizeTest) {
	char path[1024];
	auto Expect = [&path](const char *input,
			      const vector<string> &expected) {
		slice_t *results;
		int ret = tc_path_tokenize(input, &results);
		EXPECT_EQ(expected.size(), (size_t)ret);
		for (int i = 0; i < ret; ++i) {
			EXPECT_EQ(expected[i],
				  string(results[i].data, results[i].size));
		}
		free(results);
	};

	Expect(".././a", vector<string>{ "..", "a" });
	Expect("/a/b/c/d", vector<string>{ "/a", "b", "c", "d" });
	Expect("/", vector<string>{"/"});
	Expect("///a", vector<string>{"/a"});
	Expect("///a/../b", vector<string>{"/b"});
	Expect(".", vector<string>{});
}

TEST(PathUtilsTest, DepthTest) {
	EXPECT_EQ(0, tc_path_depth("/"));
	EXPECT_EQ(1, tc_path_depth("/foo"));
	EXPECT_EQ(1, tc_path_depth("foo"));
	EXPECT_EQ(0, tc_path_depth("."));
	EXPECT_EQ(1, tc_path_depth("/foo/bar/./.."));
	EXPECT_EQ(5, tc_path_depth("/a/b/c/d/e"));
	EXPECT_EQ(10, tc_path_depth("/a/b/c/d/e/../f/g/h/i/j/k"));
}

TEST(PathUtilsTest, DistanceTest) {
	EXPECT_EQ(0, tc_path_distance("/", "/"));
	EXPECT_EQ(0, tc_path_distance("/foo", "/foo"));
	EXPECT_EQ(1, tc_path_distance("/", "/foo"));
	EXPECT_EQ(2, tc_path_distance("/", "/a/b"));
	EXPECT_EQ(3, tc_path_distance("/", "/a/b/c"));
	EXPECT_EQ(5, tc_path_distance("/d", "/a/b/c/./d"));
	EXPECT_EQ(2, tc_path_distance("/a/b", "/a/b/c/d"));
	EXPECT_EQ(2, tc_path_distance("/a/b/c/d", "/a/b/c/.."));

	EXPECT_EQ(0, tc_path_distance(nullptr, "."));
	EXPECT_EQ(0, tc_path_distance(nullptr, "./."));
	EXPECT_EQ(0, tc_path_distance(nullptr, "foo/.."));
	EXPECT_EQ(1, tc_path_distance(nullptr, ".."));
	EXPECT_EQ(1, tc_path_distance(nullptr, "./foo"));
	EXPECT_EQ(2, tc_path_distance(nullptr, "../.."));
	EXPECT_EQ(3, tc_path_distance(nullptr, "../../foo"));
	EXPECT_EQ(5, tc_path_distance(nullptr, "a/b/c/d/e"));
	EXPECT_EQ(5, tc_path_distance(nullptr, "a/b/c/d/e/../../f/g"));
}

TEST(PathUtilsTest, JoinTest) {
	char path[1024];
	auto Expect = [&path](const char *p1, const char *p2, const char *exp) {
		for (int inplace = 0; inplace < 2; ++inplace) {
			int ret;
			if (inplace) {
				strcpy(path, p1);
				ret = tc_path_join(path, p2, path, 1024);
			} else {
				ret = tc_path_join(p1, p2, path, 1024);
			}
			EXPECT_EQ(strlen(exp), (size_t)ret);
			EXPECT_STREQ(exp, path) << p1 << " + " << p2
						<< " should be " << exp
						<< " instead of " << path;
		}
	};

	Expect("", "/a", "/a");
	Expect(".", "a", "a");
	Expect("a", "b", "a/b");
	Expect("..", "b", "../b");
	Expect("a", "..", ".");
	Expect("a", "../..", "..");
	Expect("a/b/.", "./../c/d", "a/c/d");
	Expect("/a/a/../b", "c/c/../d/.", "/a/b/c/d");
}

TEST(PathUtilsTest, JoinallTest) {
	char path[1024];

	tc_path_joinall(path, 1024, "a", "b", "b", "..", "c");
	EXPECT_STREQ(path, "a/b/c");
}

TEST(PathUtilsTest, RebaseTest) {
	char path[1024];
	auto Expect = [&path](const char *base, const char *p,
			      const char *exp) {
		int ret = tc_path_rebase(base, p, path, 1024);
		EXPECT_EQ(strlen(exp), ret);
		EXPECT_STREQ(exp, path) << p << " relative to " << base
					<< " should be " << exp
					<< " instead of " << path;
	};
	Expect("a", "a", ".");
	Expect("a", "b", "../b");
	Expect("a", "a/b", "b");
	Expect("/a/b/", "/a/b/c/d", "c/d");
	Expect("/a/b/c/d/e", "/a/b/c", "../..");
	Expect("/a/b/c/d/e", "/a/b/c/d/f", "../f");
}

TEST(PathUtilsTest, AppendTest) {
	buf_t *pbuf = new_auto_buf(1024);
	tc_path_append(pbuf, toslice("a"));
	EXPECT_STREQ(asstr(pbuf), "a");

	buf_reset(pbuf);
	tc_path_append(pbuf, toslice("a"));
	tc_path_append(pbuf, toslice("b"));
	tc_path_append(pbuf, toslice("c"));
	EXPECT_STREQ(asstr(pbuf), "a/b/c");

	buf_reset(pbuf);
	tc_path_append(pbuf, toslice("/a"));
	EXPECT_STREQ(asstr(pbuf), "/a");
	tc_path_append(pbuf, toslice("//b"));
	EXPECT_STREQ(asstr(pbuf), "/a/b");
}

TEST(PathUtilsTest, DirAndBaseName) {
	const char *P = "a/b/c";
	EXPECT_EQ(0, cmpslice(tc_path_dirname(P), mkslice(P, 3)));
	EXPECT_EQ(0, cmpslice(tc_path_basename(P), mkslice(P + 4, 1)));

	P = "noslash";
	EXPECT_EQ(0, cmpslice(tc_path_dirname(P), toslice("")));
	EXPECT_EQ(0, cmpslice(tc_path_basename(P), toslice("noslash")));

	P = "/a/b/c/";
	EXPECT_EQ(0, cmpslice(tc_path_dirname(P), toslice("/a/b")));
	EXPECT_EQ(0, cmpslice(tc_path_basename(P), toslice("c")));

	P = "/foo";
	EXPECT_EQ(0, cmpslice(tc_path_dirname(P), toslice("/")));
	EXPECT_EQ(0, cmpslice(tc_path_basename(P), toslice("foo")));

	P = "/";
	EXPECT_EQ(0, cmpslice(tc_path_dirname(P), toslice("/")));
	EXPECT_EQ(0, cmpslice(tc_path_basename(P), toslice("")));

	P = "///";
	EXPECT_EQ(0, cmpslice(tc_path_dirname(P), toslice("/")));
	EXPECT_EQ(0, cmpslice(tc_path_basename(P), toslice("")));

	P = "";
	EXPECT_EQ((size_t)0, tc_path_dirname(P).size);
	EXPECT_EQ(0, tc_path_basename(P).size);

	P = "/a/b//";
	EXPECT_EQ(0, cmpslice(tc_path_dirname(P), toslice("/a")));
	EXPECT_EQ(0, cmpslice(tc_path_basename(P), toslice("b")));

	P = "../a";
	EXPECT_EQ(0, cmpslice(tc_path_dirname(P), toslice("..")));
	EXPECT_EQ(0, cmpslice(tc_path_basename(P), toslice("a")));
}

TEST(PathUtilsTest, DirAndBaseTogether) {
	slice_t dir, base;

	EXPECT_FALSE(tc_path_dir_base("", &dir, &base));
	EXPECT_EQ(0, cmpslice(dir, toslice("")));
	EXPECT_EQ(0, cmpslice(base, toslice("")));

	EXPECT_FALSE(tc_path_dir_base("a", &dir, &base));
	EXPECT_EQ(0, cmpslice(dir, toslice("")));
	EXPECT_EQ(0, cmpslice(base, toslice("a")));

	EXPECT_TRUE(tc_path_dir_base("/", &dir, &base));
	EXPECT_EQ(0, cmpslice(dir, toslice("/")));
	EXPECT_EQ(0, cmpslice(base, toslice("")));

	EXPECT_TRUE(tc_path_dir_base("/a", &dir, &base));
	EXPECT_EQ(0, cmpslice(dir, toslice("/")));
	EXPECT_EQ(0, cmpslice(base, toslice("a")));

	EXPECT_TRUE(tc_path_dir_base("/a/b", &dir, &base));
	EXPECT_EQ(0, cmpslice(dir, toslice("/a")));
	EXPECT_EQ(0, cmpslice(base, toslice("b")));

	EXPECT_TRUE(tc_path_dir_base("/a/b/c/", &dir, &base));
	EXPECT_EQ(0, cmpslice(dir, toslice("/a/b")));
	EXPECT_EQ(0, cmpslice(base, toslice("c")));

	EXPECT_TRUE(tc_path_dir_base("/a/b/c/d", &dir, &base));
	EXPECT_EQ(0, cmpslice(dir, toslice("/a/b/c")));
	EXPECT_EQ(0, cmpslice(base, toslice("d")));
}

/*
 *   moocpp/moocpp-tests.cpp
 *
 *   Copyright (C) 2026 by Anton Batenev <antonbatenev@yandex.ru>
 *
 *   This file is part of medit.  medit is free software; you can
 *   redistribute it and/or modify it under the terms of the GNU Lesser
 *   General Public License as published by the Free Software Foundation;
 *   either version 2.1 of the License, or (at your option) any later version.
 */

#include "moocpp/moocpp-tests.h"

#ifdef MOO_ENABLE_UNIT_TESTS

#include "moocpp/array.h"

static void
test_array_foreach (int *value,
                    gpointer data)
{
    *(int *) data += *value;
}


static int
test_array_compare (gconstpointer a,
                     gconstpointer b)
{
    return *(const int *) a - *(const int *) b;
}


static void
test_array (void)
{
    MooArray<int> array;
    MooArray<int> other;
    MooArray<int> *copy;
    int one = 1;
    int two = 2;
    int three = 3;
    int four = 4;
    int sum = 0;

    array.append (&two);
    array.take (&three);
    other.append (&four);
    array.append_array (&other);
    g_assert_cmpuint (array.size (), ==, 3);
    g_assert_true (array[0] == &two);
    g_assert_cmpint (array.find (&three), ==, 1);
    g_assert_cmpint (array.find (&one), ==, -1);
    g_assert_false (array.empty ());

    g_assert_cmpuint (array.insert_sorted (&one, test_array_compare), ==, 0);
    array.sort (test_array_compare);
    array.foreach (test_array_foreach, &sum);
    g_assert_cmpint (sum, ==, 10);
    g_assert_true (array[0] == &one);
    g_assert_true (array[3] == &four);

    copy = array.copy ();
    g_assert_cmpuint (copy->size (), ==, 4);
    array.remove (&two);
    g_assert_cmpuint (array.size (), ==, 3);
    array.clear ();
    g_assert_true (array.empty ());
    delete copy;
}


struct TestArrayItem
{
    int value;
};

static int test_array_copies;
static int test_array_frees;

struct TestArrayCopy
{
    TestArrayItem *operator() (TestArrayItem *item) const
    {
        ++test_array_copies;
        return new TestArrayItem (*item);
    }
};

struct TestArrayFree
{
    void operator() (TestArrayItem *item) const
    {
        ++test_array_frees;
        delete item;
    }
};


static void
test_array_ownership (void)
{
    using OwnedArray = MooArray<TestArrayItem, TestArrayCopy, TestArrayFree>;
    TestArrayItem original = { 1 };
    TestArrayItem missing = { 3 };
    OwnedArray array;

    test_array_copies = 0;
    test_array_frees = 0;

    array.append (&original);
    array.take (new TestArrayItem { 2 });
    g_assert_cmpint (test_array_copies, ==, 1);
    g_assert_cmpint (test_array_frees, ==, 0);
    g_assert_cmpint (array[0]->value, ==, 1);
    g_assert_cmpint (array[1]->value, ==, 2);

    g_assert_cmpint (array.find (&missing), ==, -1);
    g_assert_cmpint (test_array_frees, ==, 0);

    OwnedArray copied (array);
    g_assert_cmpint (test_array_copies, ==, 3);
    g_assert_cmpint (copied[0]->value, ==, 1);
    g_assert_true (copied[0] != array[0]);

    array.clear ();
    g_assert_cmpint (test_array_frees, ==, 2);
    g_assert_true (array.empty ());

    OwnedArray moved (std::move (copied));
    g_assert_true (copied.empty ());
    g_assert_cmpuint (moved.size (), ==, 2);

    OwnedArray assigned;
    assigned.take (new TestArrayItem { 4 });
    assigned = std::move (moved);
    g_assert_cmpint (test_array_frees, ==, 3);
    g_assert_true (moved.empty ());
    g_assert_cmpuint (assigned.size (), ==, 2);
}


void
_moo_add_moocpp_unit_tests (void)
{
    g_test_add_func ("/moocpp/array", test_array);
    g_test_add_func ("/moocpp/array/ownership", test_array_ownership);
}

#endif /* MOO_ENABLE_UNIT_TESTS */

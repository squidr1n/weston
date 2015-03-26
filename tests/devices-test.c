/*
 * Copyright © 2015 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"

#include <string.h>
#include "weston-test-client-helper.h"

/**
 * Test (un)plugging devices
 *
 * At the end of each test we must return Weston to the previous state
 * (add all removed devices and remove extra devices), so that
 * the environment is prepared for the other tests too
 */

#define WL_SEAT_CAPABILITY_ALL (WL_SEAT_CAPABILITY_KEYBOARD |\
				WL_SEAT_CAPABILITY_POINTER  |\
				WL_SEAT_CAPABILITY_TOUCH)

/* simply test if weston sends the right capabilities when
 * some devices are removed */
TEST(seat_capabilities_test)
{
	struct client *cl = create_client_and_test_surface(100, 100, 100, 100);
	assert(cl->input->caps == WL_SEAT_CAPABILITY_ALL);

	assert(cl->input->pointer);
	weston_test_device_release(cl->test->weston_test, "pointer");
	client_roundtrip(cl);
	assert(!cl->input->pointer);
	assert(!(cl->input->caps & WL_SEAT_CAPABILITY_POINTER));

	assert(cl->input->keyboard);
	weston_test_device_release(cl->test->weston_test, "keyboard");
	client_roundtrip(cl);
	assert(!cl->input->keyboard);
	assert(!(cl->input->caps & WL_SEAT_CAPABILITY_KEYBOARD));

	assert(cl->input->touch);
	weston_test_device_release(cl->test->weston_test, "touch");
	client_roundtrip(cl);
	assert(!cl->input->touch);
	assert(!(cl->input->caps & WL_SEAT_CAPABILITY_TOUCH));

	/* restore previous state */
	weston_test_device_add(cl->test->weston_test, "keyboard");
	weston_test_device_add(cl->test->weston_test, "pointer");
	weston_test_device_add(cl->test->weston_test, "touch");
	client_roundtrip(cl);

	assert(cl->input->pointer);
	assert(cl->input->keyboard);
	assert(cl->input->touch);

	/* add extra devices */
	weston_test_device_add(cl->test->weston_test, "keyboard");
	weston_test_device_add(cl->test->weston_test, "pointer");
	weston_test_device_add(cl->test->weston_test, "touch");
	client_roundtrip(cl);

	/* remove extra devices */
	weston_test_device_release(cl->test->weston_test, "keyboard");
	weston_test_device_release(cl->test->weston_test, "pointer");
	weston_test_device_release(cl->test->weston_test, "touch");
	client_roundtrip(cl);

	/* we still should have all the capabilities, since the devices
	 * were doubled */
	assert(cl->input->caps == WL_SEAT_CAPABILITY_ALL);

	assert(cl->input->pointer);
	assert(cl->input->keyboard);
	assert(cl->input->touch);
}

#define COUNT 15
TEST(multiple_device_add_and_remove)
{
	int i;
	struct client *cl = create_client_and_test_surface(100, 100, 100, 100);

	/* add device a lot of times */
	for (i = 0; i < COUNT; ++i) {
		weston_test_device_add(cl->test->weston_test, "keyboard");
		weston_test_device_add(cl->test->weston_test, "pointer");
		weston_test_device_add(cl->test->weston_test, "touch");
	}

	client_roundtrip(cl);

	assert(cl->input->pointer);
	assert(cl->input->keyboard);
	assert(cl->input->touch);

	assert(cl->input->caps == WL_SEAT_CAPABILITY_ALL);

	/* release all new devices */
	for (i = 0; i < COUNT; ++i) {
		weston_test_device_release(cl->test->weston_test, "keyboard");
		weston_test_device_release(cl->test->weston_test, "pointer");
		weston_test_device_release(cl->test->weston_test, "touch");
	}

	client_roundtrip(cl);

	/* there is still one from each device left */
	assert(cl->input->caps == WL_SEAT_CAPABILITY_ALL);

	assert(cl->input->pointer);
	assert(cl->input->keyboard);
	assert(cl->input->touch);
}

TEST(device_release_before_destroy)
{
	struct client *cl = create_client_and_test_surface(100, 100, 100, 100);

	/* we can release pointer when we won't be using it anymore.
	 * Do it and see what happens if the device is destroyed
	 * right after that */
	wl_pointer_release(cl->input->pointer->wl_pointer);
	/* we must free and set to NULL the structures, otherwise
	 * seat capabilities will double-free them */
	free(cl->input->pointer);
	cl->input->pointer = NULL;

	wl_keyboard_release(cl->input->keyboard->wl_keyboard);
	free(cl->input->keyboard);
	cl->input->keyboard = NULL;

	wl_touch_release(cl->input->touch->wl_touch);
	free(cl->input->touch);
	cl->input->touch = NULL;

	weston_test_device_release(cl->test->weston_test, "pointer");
	weston_test_device_release(cl->test->weston_test, "keyboard");
	weston_test_device_release(cl->test->weston_test, "touch");
	client_roundtrip(cl);

	assert(cl->input->caps == 0);

	/* restore previous state */
	weston_test_device_add(cl->test->weston_test, "pointer");
	weston_test_device_add(cl->test->weston_test, "keyboard");
	weston_test_device_add(cl->test->weston_test, "touch");
	client_roundtrip(cl);

	assert(cl->input->caps == WL_SEAT_CAPABILITY_ALL);
}

TEST(device_release_before_destroy_multiple)
{
	int i;

	/* if weston crashed during this test, then there is
	 * some inconsistency */
	for (i = 0; i < 30; ++i) {
		/* Fifty times run the previous test. This will create
		 * fifty clients, because we don't have any
		 * way how to destroy them (worth of adding!). Only one
		 * client will run at a time though and so should have no
		 * effect on the result of the test (after the client
		 * finishes its body, it just 'is' and does nothing). */
		device_release_before_destroy();
	}
}

/* normal work-flow test */
TEST(device_release_after_destroy)
{
	struct client *cl = create_client_and_test_surface(100, 100, 100, 100);

	weston_test_device_release(cl->test->weston_test, "pointer");
	wl_pointer_release(cl->input->pointer->wl_pointer);
	/* we must free the memory manually, otherwise seat.capabilities
	 * will try to free it and will use invalid proxy */
	free(cl->input->pointer);
	cl->input->pointer = NULL;

	client_roundtrip(cl);

	weston_test_device_release(cl->test->weston_test, "keyboard");
	wl_keyboard_release(cl->input->keyboard->wl_keyboard);
	free(cl->input->keyboard);
	cl->input->keyboard = NULL;

	client_roundtrip(cl);

	weston_test_device_release(cl->test->weston_test, "touch");
	wl_touch_release(cl->input->touch->wl_touch);
	free(cl->input->touch);
	cl->input->touch = NULL;

	client_roundtrip(cl);

	assert(cl->input->caps == 0);

	/* restore previous state */
	weston_test_device_add(cl->test->weston_test, "pointer");
	weston_test_device_add(cl->test->weston_test, "keyboard");
	weston_test_device_add(cl->test->weston_test, "touch");
	client_roundtrip(cl);

	assert(cl->input->caps == WL_SEAT_CAPABILITY_ALL);
}

TEST(device_release_after_destroy_multiple)
{
	int i;

	/* if weston crashed during this test, then there is
	 * some inconsistency */
	for (i = 0; i < 30; ++i) {
		device_release_after_destroy();
	}
}

/* see https://bugzilla.gnome.org/show_bug.cgi?id=745008
 * It is a mutter bug, but highly relevant. Weston does not
 * suffer from this bug atm, but it is worth of testing. */
TEST(get_device_after_destroy)
{
	struct client *cl = create_client_and_test_surface(100, 100, 100, 100);
	struct wl_pointer *wl_pointer;
	struct wl_keyboard *wl_keyboard;
	struct wl_touch *wl_touch;

	/* There's a race:
	 *  1) compositor destroyes device
	 *  2) client asks for the device, because it has not got
	 *     new capabilities yet
	 *  3) compositor gets request with new_id for destroyed device
	 *  4) client uses the new_id
	 *  5) client gets new capabilities, destroying the objects
	 *
	 * If compositor just bail out in step 3) and does not create
	 * resource, then client gets error in step 4) - even though
	 * it followed the protocol (it just didn't know about new
	 * capabilities).
	 *
	 * This test simulates this situation
	 */

	/* connection is buffered, so after calling client_roundtrip(),
	 * this whole batch will be delivered to compositor and will
	 * exactly simulate our situation */
	weston_test_device_release(cl->test->weston_test, "pointer");
	wl_pointer = wl_seat_get_pointer(cl->input->wl_seat);
	assert(wl_pointer);

	/* this should be ignored */
	wl_pointer_set_cursor(wl_pointer, 0, NULL, 0, 0);

	/* this should not be ignored */
	wl_pointer_release(wl_pointer);
	client_roundtrip(cl);

	weston_test_device_release(cl->test->weston_test, "keyboard");
	wl_keyboard = wl_seat_get_keyboard(cl->input->wl_seat);
	assert(wl_keyboard);
	wl_keyboard_release(wl_keyboard);
	client_roundtrip(cl);

	weston_test_device_release(cl->test->weston_test, "touch");
	wl_touch = wl_seat_get_touch(cl->input->wl_seat);
	assert(wl_touch);
	wl_touch_release(wl_touch);
	client_roundtrip(cl);

	/* get weston to the previous state */
	weston_test_device_add(cl->test->weston_test, "pointer");
	weston_test_device_add(cl->test->weston_test, "keyboard");
	weston_test_device_add(cl->test->weston_test, "touch");
	client_roundtrip(cl);

	assert(cl->input->caps == WL_SEAT_CAPABILITY_ALL);
}

TEST(get_device_afer_destroy_multiple)
{
	int i;

	/* if weston crashed during this test, then there is
	 * some inconsistency */
	for (i = 0; i < 30; ++i) {
		get_device_after_destroy();
	}
}

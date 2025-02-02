/*------------------------------------------------------------------------
 *  Copyright 2007-2009 (c) Jeff Brown <spadix@users.sourceforge.net>
 *
 *  This file is part of the ZBar Bar Code Reader.
 *
 *  The ZBar Bar Code Reader is free software; you can redistribute it
 *  and/or modify it under the terms of the GNU Lesser Public License as
 *  published by the Free Software Foundation; either version 2.1 of
 *  the License, or (at your option) any later version.
 *
 *  The ZBar Bar Code Reader is distributed in the hope that it will be
 *  useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser Public License
 *  along with the ZBar Bar Code Reader; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *  Boston, MA  02110-1301  USA
 *
 *  http://sourceforge.net/projects/zbar
 *------------------------------------------------------------------------*/

#include <zbar.h>
#pragma hdrstop
#include "processor.h"

#define WIN_STYLE (WS_CAPTION |	WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define EXT_STYLE (WS_EX_APPWINDOW | WS_EX_OVERLAPPEDWINDOW)

int _zbar_event_init(zbar_event_t * event)
{
	*event = CreateEvent(NULL, 0, 0, 0);
	return ((*event) ? 0 : -1);
}

void _zbar_event_destroy(zbar_event_t * event)
{
	if(*event) {
		CloseHandle(*event);
		*event = NULL;
	}
}

void _zbar_event_trigger(zbar_event_t * event)
{
	SetEvent(*event);
}

/* lock must be held */
int _zbar_event_wait(zbar_event_t * event, zbar_mutex_t * lock, zbar_timer_t * timeout)
{
	if(lock)
		_zbar_mutex_unlock(lock);
	int rc = WaitForSingleObject(*event, _zbar_timer_check(timeout));
	if(lock)
		_zbar_mutex_lock(lock);
	if(!rc)
		return 1;  /* got event */
	if(rc == WAIT_TIMEOUT)
		return 0;  /* timed out */
	return -1; /* error (FIXME save info) */
}

int _zbar_thread_start(zbar_thread_t * thr, zbar_thread_proc_t proc, void * arg, zbar_mutex_t * lock)
{
	if(thr->started || thr->running)
		return (-1 /*FIXME*/);
	thr->started = 1;
	_zbar_event_init(&thr->notify);
	_zbar_event_init(&thr->activity);
	HANDLE hthr = CreateThread(NULL, 0, proc, arg, 0, 0);
	int rc = (!hthr || _zbar_event_wait(&thr->activity, NULL, NULL) < 0 || !thr->running);
	CloseHandle(hthr);
	if(rc) {
		thr->started = 0;
		_zbar_event_destroy(&thr->notify);
		_zbar_event_destroy(&thr->activity);
		return (-1 /*FIXME*/);
	}
	return 0;
}

int _zbar_thread_stop(zbar_thread_t * thr, zbar_mutex_t * lock)
{
	if(thr->started) {
		thr->started = 0;
		_zbar_event_trigger(&thr->notify);
		while(thr->running) {
			/* FIXME time out and abandon? */
			_zbar_event_wait(&thr->activity, lock, 0);
		}
		_zbar_event_destroy(&thr->notify);
		_zbar_event_destroy(&thr->activity);
	}
	return 0;
}

static LRESULT CALLBACK win_handle_event(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	zbar_processor_t * proc = (zbar_processor_t*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	/* initialized during window creation */
	if(message == WM_NCCREATE) {
		proc = (zbar_processor_t *)((LPCREATESTRUCT)lparam)->lpCreateParams;
		assert(proc);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)proc);
		proc->display = hwnd;
		zbar_window_attach(proc->window, proc->display, proc->xwin);
	}
	else if(!proc)
		return (DefWindowProc(hwnd, message, wparam, lparam));
	switch(message) {
		case WM_SIZE: {
		    RECT r;
		    GetClientRect(hwnd, &r);
		    zprintf(3, "WM_SIZE %ldx%ld\n", r.right, r.bottom);
		    assert(proc);
		    zbar_window_resize(proc->window, r.right, r.bottom);
		    InvalidateRect(hwnd, NULL, 0);
		    return 0;
	    }
		case WM_PAINT: {
		    PAINTSTRUCT ps;
		    BeginPaint(hwnd, &ps);
		    if(zbar_window_redraw(proc->window)) {
			    HDC hdc = ::GetDC(hwnd);
			    RECT r;
			    ::GetClientRect(hwnd, &r);
			    ::FillRect(hdc, &r, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
			    ReleaseDC(hwnd, hdc);
		    }
		    EndPaint(hwnd, &ps);
		    return 0;
	    }
		case WM_CHAR: _zbar_processor_handle_input(proc, wparam); return 0;
		case WM_LBUTTONDOWN: _zbar_processor_handle_input(proc, 1); return 0;
		case WM_MBUTTONDOWN: _zbar_processor_handle_input(proc, 2); return 0;
		case WM_RBUTTONDOWN: _zbar_processor_handle_input(proc, 3); return 0;
		case WM_CLOSE:
		    zprintf(3, "WM_CLOSE\n");
		    _zbar_processor_handle_input(proc, -1);
		    return 1;
		case WM_DESTROY:
		    zprintf(3, "WM_DESTROY\n");
		    proc->display = NULL;
		    zbar_window_attach(proc->window, NULL, 0);
		    return 0;
	}
	return (DefWindowProc(hwnd, message, wparam, lparam));
}

static int win_handle_events(zbar_processor_t * proc)
{
	while(1) {
		MSG msg;
		const int rc = PeekMessage(&msg, (HWND)proc->display, 0, 0, PM_NOYIELD | PM_REMOVE);
		if(rc > 0) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
			return rc ? err_capture(proc, SEV_ERROR, ZBAR_ERR_WINAPI, __FUNCTION__, "failed to obtain event") : 0;
	}
}

int _zbar_processor_init(zbar_processor_t * proc)
{
	return 0;
}

int _zbar_processor_cleanup(zbar_processor_t * proc)
{
	return 0;
}

int _zbar_processor_input_wait(zbar_processor_t * proc, zbar_event_t * event, int timeout)
{
	int n = (event) ? 1 : 0;
	int rc = MsgWaitForMultipleObjects(n, event, 0, timeout, QS_ALLINPUT);
	if(rc == n)
		return (win_handle_events(proc) < 0) ? -1 : 1;
	else if(!rc)
		return 1;
	else if(rc == WAIT_TIMEOUT)
		return 0;
	else
		return -1;
}

int _zbar_processor_enable(zbar_processor_t * proc)
{
	return 0;
}

static /*inline*/ATOM win_register_class(HINSTANCE hmod)
{
	BYTE and_mask[1] = { 0xff };
	BYTE xor_mask[1] = { 0x00 };
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), 0, };
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hInstance = hmod;
	wc.lpfnWndProc = win_handle_event;
	wc.lpszClassName = _T("_ZBar Class");
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.hCursor = CreateCursor(hmod, 0, 0, 1, 1, and_mask, xor_mask);
	return RegisterClassEx(&wc);
}

int _zbar_processor_open(zbar_processor_t * proc, char * title, uint width, uint height)
{
	HMODULE hmod = NULL;
	if(!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPTSTR>(_zbar_processor_open), &hmod))
		return err_capture(proc, SEV_ERROR, ZBAR_ERR_WINAPI, __FUNCTION__, "failed to obtain module handle");
	ATOM wca = win_register_class(hmod);
	if(!wca)
		return err_capture(proc, SEV_ERROR, ZBAR_ERR_WINAPI, __FUNCTION__, "failed to register window class");
	RECT r = { 0, 0, (LONG)width, (LONG)height };
	AdjustWindowRectEx(&r, WIN_STYLE, 0, EXT_STYLE);
	proc->display = CreateWindowEx(EXT_STYLE, (LPCTSTR)(long)wca, _T("ZBar"), WIN_STYLE, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, hmod, proc);
	if(!proc->display)
		return err_capture(proc, SEV_ERROR, ZBAR_ERR_WINAPI, __FUNCTION__, "failed to open window");
	return 0;
}

int _zbar_processor_close(zbar_processor_t * proc)
{
	if(proc->display) {
		::DestroyWindow((HWND)proc->display);
		proc->display = NULL;
	}
	return 0;
}

int _zbar_processor_set_visible(zbar_processor_t * proc, int visible)
{
	ShowWindow((HWND)proc->display, (visible) ? SW_SHOWNORMAL : SW_HIDE);
	if(visible)
		InvalidateRect((HWND)proc->display, NULL, 0);
	/* no error conditions */
	return 0;
}

int _zbar_processor_set_size(zbar_processor_t * proc, uint width, uint height)
{
	RECT r = { 0, 0, (LONG)width, (LONG)height };
	AdjustWindowRectEx(&r, GetWindowLong((HWND)proc->display, GWL_STYLE), 0, GetWindowLong((HWND)proc->display, GWL_EXSTYLE));
	if(!SetWindowPos((HWND)proc->display, NULL, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOREPOSITION))
		return (-1 /*FIXME*/);
	return 0;
}

int _zbar_processor_invalidate(zbar_processor_t * proc)
{
	if(!InvalidateRect((HWND)proc->display, NULL, 0))
		return (-1 /*FIXME*/);
	return 0;
}

